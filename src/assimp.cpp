//
//  AssimpConnector.cpp
//  gl
//
//  Created by Fabian on 12/15/12.
//
//

#include <map>
#include <deque>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include <crocore/filesystem.hpp>
#include <crocore/Image.hpp>

#include "vierkant/Object3D.hpp"
#include "vierkant/Material.hpp"
#include "vierkant/assimp.hpp"


namespace vierkant::assimp
{

struct weight_t
{
    uint32_t index = 0;
    float weight = 0.f;
};

using bone_map_t = std::map<std::string, std::pair<int, glm::mat4>>;

using weight_map_t =  std::unordered_map<uint32_t, std::list<weight_t>>;

/////////////////////////////////////////////////////////////////

vierkant::GeometryPtr create_geometry(const aiMesh *aMesh, const aiScene *theScene);

void load_bones_and_weights(const aiMesh *aMesh, uint32_t base_vertex, bone_map_t &bonemap, weight_map_t &weightmap);

void
insert_bone_vertex_data(const vierkant::GeometryPtr &geom, const weight_map_t &weightmap, uint32_t start_index = 0);


vierkant::bones::BonePtr create_bone_hierarchy(const aiNode *theNode, glm::mat4 world_transform,
                                               const std::map<std::string, std::pair<int, glm::mat4>> &boneMap,
                                               vierkant::bones::BonePtr parentBone = nullptr);

void create_bone_animation(const aiNode *theNode, const aiAnimation *theAnimation,
                           const vierkant::bones::BonePtr &root_bone, vierkant::bones::bone_animation_t &outAnim);


void process_node_hierarchy(const aiScene *scene,
                            const std::string &base_path,
                            mesh_assets_t &out_assets,
                            bone_map_t &bonemap,
                            vierkant::AABB &aabb,
                            size_t &num_vertices,
                            size_t &num_indices);

/////////////////////////////////////////////////////////////////

inline glm::mat4 aimatrix_to_glm_mat4(aiMatrix4x4 theMat)
{
    glm::mat4 ret;
    memcpy(&ret[0][0], theMat.Transpose()[0], sizeof(ret));
    return ret;
}

/////////////////////////////////////////////////////////////////

inline glm::vec3 aivector_to_glm_vec3(const aiVector3D &the_vec)
{
    glm::vec3 ret;
    for(int i = 0; i < 3; ++i){ ret[i] = the_vec[i]; }
    return ret;
}

/////////////////////////////////////////////////////////////////

inline glm::vec4 aicolor_convert(const aiColor4D &the_color)
{
    glm::vec4 ret;
    for(int i = 0; i < 4; ++i){ ret[i] = the_color[i]; }
    return ret;
}

/////////////////////////////////////////////////////////////////

inline glm::vec4 aicolor_convert(const aiColor3D &the_color)
{
    glm::vec4 ret;
    for(int i = 0; i < 3; ++i){ ret[i] = the_color[i]; }
    return ret;
}

/////////////////////////////////////////////////////////////////

vierkant::GeometryPtr create_geometry(const aiMesh *aMesh, const aiScene *theScene)
{
    auto geom = vierkant::Geometry::create();

    geom->vertices.insert(geom->vertices.end(), (glm::vec3 *) aMesh->mVertices,
                          (glm::vec3 *) aMesh->mVertices + aMesh->mNumVertices);

    if(aMesh->HasTextureCoords(0))
    {
        geom->tex_coords.reserve(aMesh->mNumVertices);

        for(uint32_t i = 0; i < aMesh->mNumVertices; i++)
        {
            geom->tex_coords.emplace_back(aMesh->mTextureCoords[0][i].x, aMesh->mTextureCoords[0][i].y);
        }
    }
    else{ geom->tex_coords.resize(aMesh->mNumVertices, glm::vec2(0)); }

    geom->indices.reserve(aMesh->mNumFaces * 3);

    for(uint32_t i = 0; i < aMesh->mNumFaces; ++i)
    {
        const aiFace &f = aMesh->mFaces[i];
        if(f.mNumIndices != 3) throw std::runtime_error("Non triangle mesh loaded");
        geom->indices.insert(geom->indices.end(), f.mIndices, f.mIndices + 3);
    }

    if(aMesh->HasNormals())
    {
        geom->normals.insert(geom->normals.end(), (glm::vec3 *) aMesh->mNormals,
                             (glm::vec3 *) aMesh->mNormals + aMesh->mNumVertices);
    }
    else
    {
        geom->compute_vertex_normals();
    }

    if(aMesh->HasVertexColors(0))
    {
        geom->colors.insert(geom->colors.end(), (glm::vec4 *) aMesh->mColors[0],
                            (glm::vec4 *) aMesh->mColors[0] + aMesh->mNumVertices);
    }

    if(aMesh->HasTangentsAndBitangents())
    {
        geom->tangents.insert(geom->tangents.end(), (glm::vec3 *) aMesh->mTangents,
                              (glm::vec3 *) aMesh->mTangents + aMesh->mNumVertices);
    }
    else
    {
        // compute tangents
        geom->compute_tangents();
    }
    return geom;
}

/////////////////////////////////////////////////////////////////

void load_bones_and_weights(const aiMesh *aMesh, uint32_t base_vertex, bone_map_t &bonemap, weight_map_t &weightmap)
{
    if(aMesh->HasBones())
    {
        uint32_t bone_index = 0;

        for(uint32_t i = 0; i < aMesh->mNumBones; ++i)
        {
            aiBone *bone = aMesh->mBones[i];
            if(bonemap.find(bone->mName.data) == bonemap.end())
            {
                bone_index = bonemap.size();
                bonemap[bone->mName.data] = std::make_pair(bone_index,
                                                           aimatrix_to_glm_mat4(bone->mOffsetMatrix));
            }
            else{ bone_index = bonemap[bone->mName.data].first; }

            for(uint32_t j = 0; j < bone->mNumWeights; ++j)
            {
                const aiVertexWeight &w = bone->mWeights[j];
                weightmap[w.mVertexId + base_vertex].push_back({bone_index, static_cast<float>(w.mWeight)});
            }
        }
    }
}

/////////////////////////////////////////////////////////////////

void insert_bone_vertex_data(const vierkant::GeometryPtr &geom, const weight_map_t &weightmap, uint32_t start_index)
{
    if(weightmap.empty()) return;

    // allocate storage for indices and weights
    geom->bone_indices.resize(geom->vertices.size());
    geom->bone_weights.resize(geom->vertices.size());

    for(const auto&[index, weights] : weightmap)
    {
        auto &bone_index = geom->bone_indices[index + start_index];
        auto &bone_weight = geom->bone_weights[index + start_index];

        constexpr uint32_t max_num_weights = glm::ivec4::length();

        // sort by weight decreasing
        auto weights_sorted = weights;
        weights_sorted.sort([](const weight_t &lhs, const weight_t &rhs){ return lhs.weight > rhs.weight; });

        uint32_t i = 0;

        for(auto &w : weights_sorted)
        {
            if(i >= max_num_weights) break;
            bone_index[i] = w.index;
            bone_weight[i] = w.weight;
            i++;
        }
    }
}

/////////////////////////////////////////////////////////////////

//gl::LightPtr create_light(const aiLight *the_light)
//{
//    gl::Light::Type t = gl::Light::UNKNOWN;
//
//    switch(the_light->mType)
//    {
//        case aiLightSource_DIRECTIONAL:
//            t = gl::Light::DIRECTIONAL;
//            break;
//        case aiLightSource_SPOT:
//            t = gl::Light::SPOT;
//            break;
//        case aiLightSource_POINT:
//            t = gl::Light::POINT;
//            break;
////        case aiLightSource_AREA:
////            t = gl::Light::AREA;
////            break;
//        default:
//            break;
//    }
//    auto l = gl::Light::create(t);
//    l->set_name(the_light->mName.data);
//    l->set_spot_cutoff(the_light->mAngleOuterCone);
//    l->set_diffuse(aicolor_convert(the_light->mColorDiffuse));
//    l->set_ambient(aicolor_convert(the_light->mColorAmbient));
//    l->set_attenuation(the_light->mAttenuationConstant, the_light->mAttenuationQuadratic);
//
//    auto pos = aivector_to_glm_vec3(the_light->mPosition);
//    auto y_axis = glm::normalize(aivector_to_glm_vec3(the_light->mUp));
//    auto z_axis = glm::normalize(-aivector_to_glm_vec3(the_light->mDirection));
//    auto x_axis = glm::normalize(glm::cross(z_axis, y_axis));
//    glm::mat4 m(vec4(x_axis, 0.f), vec4(y_axis, 0.f), vec4(z_axis, 0.f), vec4(pos, 1.f));
//    l->set_transform(m);
//    return l;
//}

/////////////////////////////////////////////////////////////////

//gl::CameraPtr create_camera(const aiCamera *the_cam)
//{
//    auto ret = gl::PerspectiveCamera::create();
//    ret->set_name(the_cam->mName.data);
//    ret->set_fov(the_cam->mHorizontalFOV);
//    ret->set_aspect(the_cam->mAspect);
//    ret->set_clipping(the_cam->mClipPlaneNear, the_cam->mClipPlaneFar);
//    auto pos = aivector_to_glm_vec3(the_cam->mPosition);
//    auto y_axis = glm::normalize(aivector_to_glm_vec3(the_cam->mUp));
//    auto z_axis = glm::normalize(-aivector_to_glm_vec3(the_cam->mLookAt));
//    auto x_axis = glm::normalize(glm::cross(z_axis, y_axis));
//    glm::mat4 m(vec4(x_axis, 0.f), vec4(y_axis, 0.f), vec4(z_axis, 0.f), vec4(pos, 1.f));
//    ret->set_transform(m);
//    return ret;
//}

/////////////////////////////////////////////////////////////////

material_t create_material(const std::string &base_path, const aiScene *the_scene, const aiMaterial *mtl,
                           std::map<std::string, crocore::ImagePtr> *the_img_map = nullptr)
{
    material_t material = {};

    int ret1, ret2;
    aiColor4D c;
    float shininess, strength;
    int two_sided;
    int wireframe;
    aiString path_buf;

    LOG_TRACE_IF(the_scene->mNumTextures) << "num embedded textures: " << the_scene->mNumTextures;

    if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &c))
    {
        auto col = aicolor_convert(c);
        col.a = 1.f;

        // transparent material
        if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_TRANSPARENT, &c)){ col.a = c.a; }
        material.diffuse = col;

        material.blending = col.a < 1.f;
    }

    if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_SPECULAR, &c))
    {
        //TODO: introduce cavity param!?
    }

    if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_AMBIENT, &c))
    {
        // got rid of constant ambient
    }

    if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_EMISSIVE, &c))
    {
        auto col = aicolor_convert(c);

        if(col.r > 0.f || col.g > 0.f || col.b > 0.f)
        {
            material.emission = col;
//            theMaterial->set_blending(false);
        }
    }

    float opacity = 1.f;
    aiGetMaterialFloat(mtl, AI_MATKEY_OPACITY, &opacity);
    material.blending = material.blending || opacity != 1.f;
    material.twosided = material.twosided || opacity != 1.f;

    ret1 = aiGetMaterialFloat(mtl, AI_MATKEY_SHININESS, &shininess);
    ret2 = aiGetMaterialFloat(mtl, AI_MATKEY_SHININESS_STRENGTH, &strength);
    float roughness = 1.f;

    if((ret1 == AI_SUCCESS) && (ret2 == AI_SUCCESS))
    {
        roughness = 1.f - crocore::clamp(shininess * strength / 80.f, 0.f, 1.f);
    }
    material.roughness = roughness;

    if(AI_SUCCESS == aiGetMaterialInteger(mtl, AI_MATKEY_ENABLE_WIREFRAME, &wireframe))
    {
        material.wireframe = wireframe;
    }

    if((AI_SUCCESS == aiGetMaterialInteger(mtl, AI_MATKEY_TWOSIDED, &two_sided)))
    {
        material.twosided = two_sided;
    }

    auto create_tex_image = [base_path, the_scene, the_img_map](const std::string &path) -> crocore::ImagePtr
    {
        crocore::ImagePtr img;

        if(the_img_map)
        {
            auto it = the_img_map->find(path);
            if(it != the_img_map->end())
            {
                img = it->second;
                LOG_TRACE << "using cached image: " << path;
            }
        }
        if(!img)
        {
            if(!path.empty() && path[0] == '*')
            {
                size_t tex_index = crocore::string_to<size_t>(path.substr(1));
                const aiTexture *ai_tex = the_scene->mTextures[tex_index];

                // compressed image -> decode
                if(ai_tex->mHeight == 0)
                {
                    img = crocore::create_image_from_data((uint8_t *) ai_tex->pcData, ai_tex->mWidth);
                }
            }
            else
            {
                auto combined_path = crocore::fs::join_paths(base_path, path);
                img = crocore::create_image_from_file(combined_path);
            }

            if(img && img->num_components() == 3)
            {
                auto new_img = crocore::Image_<uint8_t>::create(img->width(), img->height(), 4);

                size_t src_stride = 3, dst_stride = 4;

                auto src = static_cast<uint8_t *>(img->data());
                auto dst = static_cast<uint8_t *>(new_img->data());
                uint8_t *dst_end = dst + new_img->num_bytes();
                constexpr size_t alpha_offset = 3;

                for(; dst < dst_end;)
                {
                    memcpy(dst, src, src_stride);
                    dst[alpha_offset] = 255;
                    dst += dst_stride;
                    src += src_stride;
                }

                img = new_img;
            }
            if(the_img_map){ (*the_img_map)[path] = img; }
        }
        return img;
    };

    std::string ao_map_path;

    // DIFFUSE
    if(AI_SUCCESS == mtl->GetTexture(aiTextureType(aiTextureType_DIFFUSE), 0, &path_buf))
    {
        LOG_TRACE << "adding color map: '" << path_buf.data << "'";
        material.img_diffuse = create_tex_image(path_buf.data);
    }

    // EMISSION
    if(AI_SUCCESS == mtl->GetTexture(aiTextureType(aiTextureType_EMISSIVE), 0, &path_buf))
    {
        LOG_TRACE << "adding emission map: '" << path_buf.data << "'";
        material.img_emission = create_tex_image(path_buf.data);
    }

    // ambient occlusion or lightmap
    if(AI_SUCCESS == mtl->GetTexture(aiTextureType(aiTextureType_LIGHTMAP), 0, &path_buf))
    {
        LOG_TRACE << "adding ambient occlusion map: '" << path_buf.data << "'";
        ao_map_path = path_buf.data;
    }

    // SHINYNESS
    if(AI_SUCCESS == mtl->GetTexture(aiTextureType(aiTextureType_SPECULAR), 0, &path_buf))
    {
        LOG_TRACE << "adding spec/roughness map: '" << path_buf.data << "'";
        material.img_specular = create_tex_image(path_buf.data);
    }

    if(AI_SUCCESS == mtl->GetTexture(aiTextureType(aiTextureType_NORMALS), 0, &path_buf))
    {
        LOG_TRACE << "adding normalmap: '" << path_buf.data << "'";
        material.img_normals = create_tex_image(path_buf.data);
    }

    if(AI_SUCCESS == mtl->GetTexture(aiTextureType(aiTextureType_DISPLACEMENT), 0, &path_buf))
    {
        LOG_TRACE << "adding normalmap: '" << path_buf.data << "'";
        material.img_normals = create_tex_image(path_buf.data);
    }

    if(AI_SUCCESS == mtl->GetTexture(aiTextureType(aiTextureType_HEIGHT), 0, &path_buf))
    {
        LOG_TRACE << "adding normalmap: '" << path_buf.data << "'";
        material.img_normals = create_tex_image(path_buf.data);
    }

    if(AI_SUCCESS == mtl->GetTexture(aiTextureType(aiTextureType_UNKNOWN), 0, &path_buf))
    {
        LOG_TRACE << "unknown texture usage (assuming AO/ROUGHNESS/METAL ): '" << path_buf.data << "'";

        auto ao_rough_metal_img = create_tex_image(path_buf.data);

        if(ao_rough_metal_img)
        {
            constexpr size_t ao_offset = 0;
            uint8_t *dst = (uint8_t *) ao_rough_metal_img->data(), *dst_end = dst + ao_rough_metal_img->num_bytes();

            // there was no texture data for AO -> generate default data
            if(ao_map_path.empty())
            {
                for(; dst < dst_end; dst += ao_rough_metal_img->num_components()){ dst[ao_offset] = 255; }
            }
            else if(ao_map_path != path_buf.data)
            {
                // there was texture data for AO in a separate map -> combine
                auto ao_img = create_tex_image(ao_map_path)->resize(ao_rough_metal_img->width(),
                                                                    ao_rough_metal_img->height());
                uint8_t *src = (uint8_t *) ao_img->data();

                for(; dst < dst_end;)
                {
                    dst[ao_offset] = src[ao_offset];
                    dst += ao_rough_metal_img->num_components();
                    src += ao_img->num_components();
                }
            }
        }
        material.img_ao_roughness_metal = create_tex_image(path_buf.data);
    }
    return material;
}

/////////////////////////////////////////////////////////////////

mesh_assets_t load_model(const std::string &path)
{
    Assimp::Importer importer;
    std::string found_path;

    try{ found_path = crocore::fs::search_file(path); }
    catch(crocore::fs::FileNotFoundException &e)
    {
        LOG_ERROR << e.what();
        return {};
    }

    auto base_path = crocore::fs::get_directory_part(found_path);

    LOG_DEBUG << "loading model '" << path << "' ...";
    const aiScene *theScene = importer.ReadFile(found_path, 0);

    // super useful postprocessing steps
    theScene = importer.ApplyPostProcessing(aiProcess_Triangulate
                                            | aiProcess_FlipUVs
                                            | aiProcess_JoinIdenticalVertices
                                            | aiProcess_CalcTangentSpace
                                            | aiProcess_LimitBoneWeights);
    if(theScene)
    {
        mesh_assets_t mesh_assets = {};
        mesh_assets.materials.resize(theScene->mNumMaterials);

        size_t num_vertices = 0, num_indices = 0;

        vierkant::AABB aabb;
        bone_map_t bonemap;


        std::map<std::string, crocore::ImagePtr> image_cache;

        // iterate node hierarchy, find geometries and node animations
        process_node_hierarchy(theScene, base_path, mesh_assets, bonemap, aabb, num_vertices, num_indices);

        // create bone hierarchy
        auto root_bone = create_bone_hierarchy(theScene->mRootNode, glm::mat4(1), bonemap);

        for(uint32_t i = 0; i < theScene->mNumAnimations; i++)
        {
            aiAnimation *assimpAnimation = theScene->mAnimations[i];
            vierkant::bones::bone_animation_t anim;
            anim.duration = assimpAnimation->mDuration;
            anim.ticks_per_sec = assimpAnimation->mTicksPerSecond;
            create_bone_animation(theScene->mRootNode, assimpAnimation, root_bone, anim);
            mesh_assets.animations.push_back(std::move(anim));
        }

        // extract model name from filename
        auto model_name = crocore::fs::get_filename_part(found_path);


        LOG_DEBUG << crocore::format("loaded model: geometries: %d -- vertices: %d -- faces: %d -- bones: %d ",
                                     mesh_assets.geometries.size(), num_vertices, num_indices * 3,
                                     bones::num_bones_in_hierarchy(root_bone));
        LOG_DEBUG << "bounds: " << glm::to_string(aabb.min) << " - " << glm::to_string(aabb.max);

        importer.FreeScene();

        // search for external animations
        auto anim_files = crocore::fs::get_directory_entries(crocore::fs::join_paths(base_path, "animations"),
                                                             crocore::fs::FileType::MODEL, 2);
        for(auto &p : anim_files){ add_animations_to_mesh(p, mesh_assets); }
        return mesh_assets;
    }
    return {};
}

/////////////////////////////////////////////////////////////////

void process_node_hierarchy(const aiScene *scene,
                            const std::string &base_path,
                            mesh_assets_t &out_assets,
                            bone_map_t &bonemap,
                            vierkant::AABB &aabb,
                            size_t &num_vertices,
                            size_t &num_indices)
{
    struct node_t
    {
        const aiNode *node;
        glm::mat4 global_transform;
    };

    // cache duplicate geometries
    std::unordered_map<const aiMesh *, vierkant::GeometryPtr> geometry_map;

    // cache duplicate images
    std::map<std::string, crocore::ImagePtr> image_cache;

    std::deque<node_t> node_queue;
    node_queue.push_back({scene->mRootNode, glm::mat4(1)});

    while(!node_queue.empty())
    {
        // dequeue node struct
        const aiNode *ai_node = node_queue.front().node;
        glm::mat4 node_transform = node_queue.front().global_transform;
        node_queue.pop_front();

        for(uint32_t i = 0; i < ai_node->mNumMeshes; ++i)
        {
            const aiMesh *ai_mesh = scene->mMeshes[ai_node->mMeshes[i]];
            vierkant::GeometryPtr geometry;

            // search geometry cache
            auto geom_it = geometry_map.find(ai_mesh);

            if(geom_it != geometry_map.end()){ geometry = geom_it->second; }
            else
            {
                geometry = create_geometry(ai_mesh, scene);
                geometry_map[ai_mesh] = geometry;
            }

            // add this node's mesh-assets
            weight_map_t weightmap;
            load_bones_and_weights(ai_mesh, 0, bonemap, weightmap);
            insert_bone_vertex_data(geometry, weightmap, 0);

            geometry->colors.resize(geometry->vertices.size(), glm::vec4(1));

            num_vertices += geometry->vertices.size();
            num_indices += geometry->indices.size();

            out_assets.geometries.push_back(geometry);
            out_assets.transforms.push_back(node_transform);
            out_assets.material_indices.push_back(ai_mesh->mMaterialIndex);

            out_assets.materials[ai_mesh->mMaterialIndex] = create_material(base_path, scene,
                                                                            scene->mMaterials[ai_mesh->mMaterialIndex],
                                                                            &image_cache);

            aabb += vierkant::compute_aabb(geometry->vertices).transform(node_transform);
        }

        for(uint32_t c = 0; c < ai_node->mNumChildren; ++c)
        {
            glm::mat4 child_transform = aimatrix_to_glm_mat4(ai_node->mChildren[c]->mTransformation);

            // enqueue child node and transform
            node_queue.push_back({ai_node->mChildren[c], node_transform * child_transform});
        }
    }
}

/////////////////////////////////////////////////////////////////

//void process_node(const aiScene *the_scene, const aiNode *the_in_node,
//                  const vierkant::Object3DPtr &the_parent_node)
//{
//    if(!the_in_node){ return; }
//
////    string node_name(the_in_node->mName.data);
//
//    auto node = vierkant::Object3D::create(the_in_node->mName.data);
//    node->set_transform(aimatrix_to_glm_mat4(the_in_node->mTransformation));
//    the_parent_node->add_child(node);
//
//    // meshes assigned to this node
//    for(uint32_t n = 0; n < the_in_node->mNumMeshes; ++n)
//    {
////        const aiMesh *mesh = the_scene->mMeshes[the_in_node->mMeshes[n]];
//    }
//
//    for(uint32_t i = 0; i < the_in_node->mNumChildren; ++i)
//    {
//        process_node(the_scene, the_in_node->mChildren[i], node);
//    }
//}

/////////////////////////////////////////////////////////////////

vierkant::bones::BonePtr create_bone_hierarchy(const aiNode *theNode, glm::mat4 world_transform,
                                               const std::map<std::string, std::pair<int, glm::mat4>> &boneMap,
                                               vierkant::bones::BonePtr parentBone)
{
    vierkant::bones::BonePtr currentBone;
    std::string nodeName(theNode->mName.data);
    glm::mat4 nodeTransform = aimatrix_to_glm_mat4(theNode->mTransformation);

    world_transform = world_transform * nodeTransform;
    auto it = boneMap.find(nodeName);

    // current node corresponds to a bone
    if(it != boneMap.end())
    {
        int boneIndex = it->second.first;
        const glm::mat4 &offset = it->second.second;
        currentBone = std::make_shared<vierkant::bones::bone_t>();
        currentBone->name = nodeName;
        currentBone->index = boneIndex;
        currentBone->transform = nodeTransform;
        currentBone->world_transform = world_transform;
        currentBone->offset = offset;
        currentBone->parent = std::move(parentBone);
    }

    for(uint32_t i = 0; i < theNode->mNumChildren; i++)
    {
        vierkant::bones::BonePtr child = create_bone_hierarchy(theNode->mChildren[i], world_transform,
                                                               boneMap, currentBone);

        if(currentBone && child){ currentBone->children.push_back(child); }
        else if(child)
        {
            // we are at root lvl
            currentBone = child;
        }
    }
    return currentBone;
}

/////////////////////////////////////////////////////////////////

void create_bone_animation(const aiNode *theNode, const aiAnimation *theAnimation,
                           const vierkant::bones::BonePtr &root_bone, vierkant::bones::bone_animation_t &outAnim)
{
    std::string nodeName(theNode->mName.data);
    const aiNodeAnim *nodeAnim = nullptr;

    if(theAnimation)
    {
        for(uint32_t i = 0; i < theAnimation->mNumChannels; i++)
        {
            aiNodeAnim *ptr = theAnimation->mChannels[i];

            if(std::string(ptr->mNodeName.data) == nodeName)
            {
                nodeAnim = ptr;
                break;
            }
        }
    }

    auto bone = vierkant::bones::bone_by_name(root_bone, nodeName);

    // this node corresponds to a bone node in the hierarchy
    // and we have animation keys for this bone
    if(bone && nodeAnim)
    {
        char buf[1024];
        sprintf(buf, "Found animation for %s: %d posKeys -- %d rotKeys -- %d scaleKeys",
                nodeAnim->mNodeName.data,
                nodeAnim->mNumPositionKeys,
                nodeAnim->mNumRotationKeys,
                nodeAnim->mNumScalingKeys);
        LOG_TRACE << buf;

        vierkant::animation_keys_t animKeys;
        glm::vec3 bonePosition;
        glm::vec3 boneScale;
        glm::quat boneRotation;

        for(uint32_t i = 0; i < nodeAnim->mNumRotationKeys; i++)
        {
            aiQuaternion rot = nodeAnim->mRotationKeys[i].mValue;
            boneRotation = glm::quat(rot.w, rot.x, rot.y, rot.z);
            animKeys.rotations.push_back({static_cast<float>(nodeAnim->mRotationKeys[i].mTime), boneRotation});
        }

        for(uint32_t i = 0; i < nodeAnim->mNumPositionKeys; i++)
        {
            aiVector3D pos = nodeAnim->mPositionKeys[i].mValue;
            bonePosition = glm::vec3(pos.x, pos.y, pos.z);
            animKeys.positions.push_back({static_cast<float>(nodeAnim->mPositionKeys[i].mTime), bonePosition});
        }

        for(uint32_t i = 0; i < nodeAnim->mNumScalingKeys; i++)
        {
            aiVector3D scaleTmp = nodeAnim->mScalingKeys[i].mValue;
            boneScale = glm::vec3(scaleTmp.x, scaleTmp.y, scaleTmp.z);
            animKeys.scales.push_back({static_cast<float>(nodeAnim->mScalingKeys[i].mTime), boneScale});
        }
        outAnim.keys[bone] = animKeys;
    }

    for(uint32_t i = 0; i < theNode->mNumChildren; i++)
    {
        create_bone_animation(theNode->mChildren[i], theAnimation, root_bone, outAnim);
    }
}

/////////////////////////////////////////////////////////////////

size_t add_animations_to_mesh(const std::string &path, mesh_assets_t &mesh_assets)
{
    LOG_TRACE << "loading animations from '" << path << "' ...";

    Assimp::Importer importer;
    std::string found_path;
    const aiScene *theScene = nullptr;

    try{ theScene = importer.ReadFile(crocore::fs::search_file(path), 0); }
    catch(crocore::fs::FileNotFoundException &e)
    {
        LOG_WARNING << e.what();
        return 0;
    }

    if(theScene)
    {
        for(uint32_t i = 0; i < theScene->mNumAnimations; i++)
        {
            aiAnimation *assimpAnimation = theScene->mAnimations[i];
            vierkant::bones::bone_animation_t anim;
            anim.duration = assimpAnimation->mDuration;
            anim.ticks_per_sec = assimpAnimation->mTicksPerSecond;
            create_bone_animation(theScene->mRootNode, assimpAnimation, mesh_assets.root_bone, anim);
            mesh_assets.animations.push_back(std::move(anim));
        }
    }
    return theScene ? theScene->mNumAnimations : 0;
}

} //namespace vierkant::assimp
