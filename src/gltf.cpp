//
// Created by crocdialer on 9/3/21.
//

#define TINYGLTF_IMPLEMENTATION

#include <tiny_gltf.h>

#include <vierkant/gltf.hpp>
#include <deque>

namespace vierkant::model
{

// vertex attributes
constexpr char attrib_position[] = "POSITION";
constexpr char attrib_normal[] = "NORMAL";
constexpr char attrib_tangent[] = "TANGENT";
constexpr char attrib_color[] = "COLOR_0";
constexpr char attrib_texcoord[] = "TEXCOORD_0";
constexpr char attrib_joints[] = "JOINTS_0";
constexpr char attrib_weights[] = "WEIGHTS_0";

// extensions
constexpr char KHR_materials_specular[] = "KHR_materials_specular";
constexpr char KHR_materials_transmission[] = "KHR_materials_transmission";
constexpr char KHR_materials_volume[] = "KHR_materials_volume";
constexpr char KHR_materials_ior[] = "KHR_materials_ior";

// extension properties
constexpr char ext_transmission_factor[] = "transmissionFactor";
constexpr char ext_volume_thickness_factor[] = "thicknessFactor";
constexpr char ext_volume_thickness_texture[] = "thicknessTexture";
constexpr char ext_volume_attenuation_distance[] = "attenuationDistance";
constexpr char ext_volume_attenuation_color[] = "attenuationColor";

// animation targets
constexpr char animation_target_translation[] = "translation";
constexpr char animation_target_rotation[] = "rotation";
constexpr char animation_target_scale[] = "scale";
constexpr char animation_target_weights[] = "weights";

struct node_t
{
    size_t index;
    glm::mat4 world_transform;
    vierkant::nodes::NodePtr node;
};

using node_map_t = std::unordered_map<uint32_t, vierkant::nodes::NodePtr>;

using joint_map_t = std::unordered_map<uint32_t, uint32_t>;

glm::mat4 node_transform(const tinygltf::Node &tiny_node)
{
    if(tiny_node.matrix.size() == 16)
    {
        return *reinterpret_cast<const glm::dmat4 *>(tiny_node.matrix.data());
    }

    glm::dquat rotation(1, 0, 0, 0);
    glm::dvec3 scale(1.);
    glm::dvec3 translation(0.);

    if(tiny_node.translation.size() == 3)
    {
        translation = glm::dvec3(tiny_node.translation[0], tiny_node.translation[1], tiny_node.translation[2]);
    }

    if(tiny_node.scale.size() == 3)
    {
        scale = glm::dvec3(tiny_node.scale[0], tiny_node.scale[1], tiny_node.scale[2]);
    }

    if(tiny_node.rotation.size() == 4)
    {
        rotation = glm::dquat(tiny_node.rotation[3], tiny_node.rotation[0], tiny_node.rotation[1],
                              tiny_node.rotation[2]);
    }
    return glm::translate(glm::dmat4(1), translation) * glm::mat4_cast(rotation) * glm::scale(glm::dmat4(1), scale);
}

model::material_t convert_material(const tinygltf::Material &tiny_mat,
                                   const tinygltf::Model &model,
                                   const std::map<uint32_t, crocore::ImagePtr> &image_cache)
{
    model::material_t ret;
    ret.name = tiny_mat.name;
    ret.diffuse = *reinterpret_cast<const glm::dvec4 *>(tiny_mat.pbrMetallicRoughness.baseColorFactor.data());
    ret.emission = *reinterpret_cast<const glm::dvec3 *>(tiny_mat.emissiveFactor.data());

    // blend_mode defaults to opaque
    if(tiny_mat.alphaMode == "BLEND"){ ret.blend_mode = vierkant::Material::BlendMode::Blend; }
    else if(tiny_mat.alphaMode == "MASK"){ ret.blend_mode = vierkant::Material::BlendMode::Mask; }
    ret.alpha_cutoff = static_cast<float>(tiny_mat.alphaCutoff);

    ret.metalness = static_cast<float>(tiny_mat.pbrMetallicRoughness.metallicFactor);
    ret.roughness = static_cast<float>(tiny_mat.pbrMetallicRoughness.roughnessFactor);
    ret.twosided = tiny_mat.doubleSided;


    // albedo
    if(tiny_mat.pbrMetallicRoughness.baseColorTexture.index >= 0)
    {
        ret.img_diffuse = image_cache.at(model.textures[tiny_mat.pbrMetallicRoughness.baseColorTexture.index].source);
    }

    // ao / rough / metal
    if(tiny_mat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
    {
        ret.img_ao_roughness_metal = image_cache.at(
                model.textures[tiny_mat.pbrMetallicRoughness.metallicRoughnessTexture.index].source);
    }

    // normals
    if(tiny_mat.normalTexture.index >= 0)
    {
        ret.img_normals = image_cache.at(model.textures[tiny_mat.normalTexture.index].source);
    }

    // emission
    if(tiny_mat.emissiveTexture.index >= 0)
    {
        ret.img_emission = image_cache.at(model.textures[tiny_mat.emissiveTexture.index].source);
    }

    for(const auto&[ext, value] : tiny_mat.extensions)
    {
        LOG_DEBUG << "material-extension: " << ext;
        if(ext == KHR_materials_transmission)
        {
            if(value.Has(ext_transmission_factor))
            {
                ret.transmission = static_cast<float>(value.Get(ext_transmission_factor).GetNumberAsDouble());
            }
        }
        else if(ext == KHR_materials_volume)
        {
            if(value.Has(ext_volume_thickness_factor))
            {
                const auto &thickness_value = value.Get(ext_volume_thickness_factor);
                ret.thickness = static_cast<float>(thickness_value.GetNumberAsDouble());
            }

            if(value.Has(ext_volume_attenuation_color))
            {
                const auto &attenuation_value = value.Get(ext_volume_attenuation_color);
                ret.attenuation_color = glm::dvec3(attenuation_value.Get(0).GetNumberAsDouble(),
                                                   attenuation_value.Get(1).GetNumberAsDouble(),
                                                   attenuation_value.Get(2).GetNumberAsDouble());
            }

            if(value.Has(ext_volume_attenuation_distance))
            {
                const auto &attenuation_distance_value = value.Get(ext_volume_attenuation_distance);
                ret.attenuation_distance = static_cast<float>(attenuation_distance_value.GetNumberAsDouble());
            }
            if(value.Has(ext_volume_attenuation_distance))
            {
                const auto &thickness_texture_value = value.Get(ext_volume_thickness_texture);
                int tex_index = thickness_texture_value.Get("index").GetNumberAsInt();
                ret.img_thickness = image_cache.at(model.textures[tex_index].source);
            }
        }
        else if(ext == KHR_materials_ior)
        {
            if(value.Has("ior"))
            {
                ret.ior = static_cast<float>(value.Get("ior").GetNumberAsDouble());
            }
        }
    }
    return ret;
}

vierkant::nodes::NodePtr create_bone_hierarchy_bfs(const tinygltf::Skin &skin,
                                                   const tinygltf::Model &model,
                                                   node_map_t &node_map)
{
    vierkant::nodes::NodePtr root_bone;

    // optional vertex skinning
    std::vector<glm::mat4> inverse_binding_matrices;

    joint_map_t joint_map;
    for(uint32_t i = 0; i < skin.joints.size(); ++i){ joint_map[skin.joints[i]] = i; }

    if(skin.inverseBindMatrices >= 0 &&
       static_cast<uint32_t>(skin.inverseBindMatrices) < model.accessors.size())
    {
        tinygltf::Accessor bind_matrix_accessor = model.accessors[skin.inverseBindMatrices];

        assert(bind_matrix_accessor.type == TINYGLTF_TYPE_MAT4);
        assert(bind_matrix_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

        const auto &buffer_view = model.bufferViews[bind_matrix_accessor.bufferView];
        const auto &buffer = model.buffers[buffer_view.buffer];
        int stride = bind_matrix_accessor.ByteStride(buffer_view);
        assert(stride == sizeof(glm::mat4));

        auto data = reinterpret_cast<const glm::mat4 *>(buffer.data.data() +
                                                        bind_matrix_accessor.byteOffset +
                                                        buffer_view.byteOffset);
        inverse_binding_matrices = {data, data + bind_matrix_accessor.count};
    }

    if(skin.skeleton >= 0 && static_cast<uint32_t>(skin.skeleton) < model.nodes.size() &&
       !inverse_binding_matrices.empty())
    {
        std::deque<node_t> node_queue;
        node_queue.push_back({static_cast<size_t>(skin.skeleton), glm::mat4(1), nullptr});

        while(!node_queue.empty())
        {
            auto[current_index, world_transform, parent_node] = std::move(node_queue.front());
            node_queue.pop_front();

            const tinygltf::Node &skeleton_node = model.nodes[current_index];
            auto local_joint_transform = node_transform(skeleton_node);
            world_transform = world_transform * local_joint_transform;

            auto bone_node = std::make_shared<vierkant::nodes::node_t>();
            bone_node->parent = parent_node;
            bone_node->name = skeleton_node.name;
            bone_node->index = joint_map[current_index];
            bone_node->offset = inverse_binding_matrices[joint_map[current_index]];
            bone_node->transform = local_joint_transform;

            node_map[current_index] = bone_node;

            if(!root_bone){ root_bone = bone_node; }
            if(parent_node){ parent_node->children.push_back(bone_node); }

            for(auto child_index : skeleton_node.children)
            {
                node_queue.push_back({static_cast<size_t>(child_index), world_transform, bone_node});
            }
        }
    }
    return root_bone;
}

vierkant::nodes::node_animation_t create_node_animation(const tinygltf::Animation &tiny_animation,
                                                        const tinygltf::Model &model,
                                                        const node_map_t &node_map)
{
    LOG_DEBUG << "animation: " << tiny_animation.name;

    vierkant::nodes::node_animation_t animation;
    animation.name = tiny_animation.name;

    for(const auto &channel : tiny_animation.channels)
    {
        auto it = node_map.find(channel.target_node);

        if(it != node_map.end())
        {
            const auto &node_ptr = it->second;
            animation_keys_t &animation_keys = animation.keys[node_ptr];

            const auto &sampler = tiny_animation.samplers[channel.sampler];

            // TODO: cache input times
            std::vector<float> input_times;
            {
                const auto &accessor = model.accessors[sampler.input];
                const auto &buffer_view = model.bufferViews[accessor.bufferView];
                const auto &buffer = model.buffers[buffer_view.buffer];
                assert(accessor.type == TINYGLTF_TYPE_SCALAR);
                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                auto data = buffer.data.data() + accessor.byteOffset + buffer_view.byteOffset;
                auto ptr = reinterpret_cast<const float *>(data);
                input_times = {ptr, ptr + accessor.count};
                animation.duration = std::max(animation.duration,
                                              *std::max_element(input_times.begin(), input_times.end()));
            }

            const auto &accessor = model.accessors[sampler.output];
            const auto &buffer_view = model.bufferViews[accessor.bufferView];
            const auto &buffer = model.buffers[buffer_view.buffer];
            auto data = buffer.data.data() + accessor.byteOffset + buffer_view.byteOffset;

            if(channel.target_path == animation_target_translation)
            {
                assert(accessor.type == TINYGLTF_TYPE_VEC3);
                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                auto ptr = reinterpret_cast<const glm::vec3 *>(data);
                for(float t : input_times){ animation_keys.positions.insert({t, *ptr++}); }

            }
            else if(channel.target_path == animation_target_rotation)
            {
                assert(accessor.type == TINYGLTF_TYPE_VEC4);
                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                auto ptr = reinterpret_cast<const glm::quat *>(data);
                for(float t : input_times){ animation_keys.rotations.insert({t, *ptr++}); }
            }
            else if(channel.target_path == animation_target_scale)
            {
                assert(accessor.type == TINYGLTF_TYPE_VEC3);
                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                auto ptr = reinterpret_cast<const glm::vec3 *>(data);
                for(float t : input_times){ animation_keys.scales.insert({t, *ptr++}); }
            }
            else if(channel.target_path == animation_target_weights)
            {

            }
        }
    }
    return animation;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mesh_assets_t gltf(const std::filesystem::path &path)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ret = false;
    auto ext_str = path.extension();
    if(ext_str == ".gltf"){ ret = loader.LoadASCIIFromFile(&model, &err, &warn, path); }
    else if(ext_str == ".glb"){ ret = loader.LoadBinaryFromFile(&model, &err, &warn, path); }
    if(!warn.empty()){ LOG_WARNING << warn; }
    if(!err.empty()){ LOG_ERROR << err; }
    if(!ret){ return {}; }

    for(const auto &ext : model.extensionsUsed)
    {
        LOG_DEBUG << "model using extension: " << ext;
    }

    const tinygltf::Scene &scene = model.scenes[model.defaultScene];

    // create vierkant::node root
    vierkant::model::mesh_assets_t out_assets = {};
    out_assets.root_node = std::make_shared<vierkant::nodes::node_t>();
    out_assets.root_node->name = model.nodes[scene.nodes[0]].name;

    // create images
    std::map<uint32_t, crocore::ImagePtr> image_cache;

    for(const auto &t : model.textures)
    {
        LOG_TRACE << "loading image: " << t.name;
//        auto &sampler = model.samplers[t.sampler];
//        sampler.magFilter
//        sampler.minFilter
//        sampler.wrapS
//        sampler.wrapT

        auto &tiny_image = model.images[t.source];

        if(!image_cache.count(t.source))
        {
            image_cache[t.source] = crocore::Image_<uint8_t>::create(tiny_image.image.data(), tiny_image.width,
                                                                     tiny_image.height,
                                                                     tiny_image.component);
        }
    }

    // create materials
    for(const auto &tiny_mat : model.materials)
    {
        out_assets.materials.push_back(convert_material(tiny_mat, model, image_cache));
    }

    std::deque<node_t> node_queue;

    for(int node_index : scene.nodes)
    {
        node_queue.push_back({static_cast<size_t>(node_index), glm::mat4(1), out_assets.root_node});
    }

    node_map_t node_map;

    while(!node_queue.empty())
    {
        auto[current_index, world_transform, parent_node] = node_queue.front();
        node_queue.pop_front();

        const tinygltf::Node &tiny_node = model.nodes[current_index];

        // create vierkant::node
        auto current_node = std::make_shared<vierkant::nodes::node_t>();
        current_node->name = tiny_node.name;
        current_node->index = out_assets.entry_create_infos.size();
        current_node->parent = parent_node;
        current_node->transform = node_transform(tiny_node);

        assert(parent_node);
        parent_node->children.push_back(current_node);

        world_transform = world_transform * current_node->transform;

        if(tiny_node.mesh >= 0 && static_cast<uint32_t>(tiny_node.mesh) < model.meshes.size())
        {
            tinygltf::Mesh &mesh = model.meshes[tiny_node.mesh];

            joint_map_t joint_map;

            if(tiny_node.skin >= 0 && static_cast<uint32_t>(tiny_node.skin) < model.skins.size())
            {
                const tinygltf::Skin &skin = model.skins[tiny_node.skin];
                out_assets.root_bone = create_bone_hierarchy_bfs(skin, model, node_map);
            }

            for(const auto &primitive : mesh.primitives)
            {
                auto geometry = vierkant::Geometry::create();

                // extract indices
                if(primitive.indices >= 0 && static_cast<size_t>(primitive.indices) < model.accessors.size())
                {
                    tinygltf::Accessor index_accessor = model.accessors[primitive.indices];
                    const auto &buffer_view = model.bufferViews[index_accessor.bufferView];
                    const auto &buffer = model.buffers[buffer_view.buffer];

                    if(buffer_view.target == 0){ LOG_WARNING << "bufferView.target is zero"; }

                    assert(index_accessor.type == TINYGLTF_TYPE_SCALAR);

                    auto data = static_cast<const uint8_t *>(buffer.data.data() + index_accessor.byteOffset +
                                                             buffer_view.byteOffset);

                    if(index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        const auto *ptr = reinterpret_cast<const uint16_t *>(data);
                        geometry->indices = {ptr, ptr + index_accessor.count};
                    }
                    else if(index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                    {
                        const auto *ptr = reinterpret_cast<const uint32_t *>(data);
                        geometry->indices = {ptr, ptr + index_accessor.count};
                    }
                    else{ LOG_ERROR << "unsupported index-type: " << index_accessor.componentType; }
                }

                for(const auto &[attrib, accessor_idx] : primitive.attributes)
                {
                    tinygltf::Accessor accessor = model.accessors[accessor_idx];
                    const auto &buffer_view = model.bufferViews[accessor.bufferView];
                    const auto &buffer = model.buffers[buffer_view.buffer];

                    auto insert = [&accessor, &buffer_view](const tinygltf::Buffer &input, auto &array)
                    {
                        using elem_t = typename std::decay<decltype(array)>::type::value_type;
                        constexpr size_t elem_size = sizeof(elem_t);
                        assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

                        // data with offset
                        const uint8_t *data = input.data.data() + buffer_view.byteOffset + accessor.byteOffset;
                        uint32_t stride = accessor.ByteStride(buffer_view);

                        if(stride == elem_size)
                        {
                            const auto *ptr = reinterpret_cast<const elem_t *>(data);
                            auto end = ptr + accessor.count;
                            array = {ptr, end};
                        }
                        else
                        {
                            auto ptr = data;
                            auto end = data + accessor.count * stride;

                            // prealloc
                            array.resize(accessor.count);
                            auto dst = reinterpret_cast<uint8_t *>(array.data());

                            // stride copy
                            for(; ptr < end; ptr += stride, dst += elem_size){ std::memcpy(dst, ptr, elem_size); }
                        }
                    };

                    if(attrib == attrib_position){ insert(buffer, geometry->vertices); }
                    else if(attrib == attrib_normal){ insert(buffer, geometry->normals); }
                    else if(attrib == attrib_tangent){ insert(buffer, geometry->tangents); }
                    else if(attrib == attrib_color){ insert(buffer, geometry->colors); }
                    else if(attrib == attrib_texcoord){ insert(buffer, geometry->tex_coords); }
                    else if(attrib == attrib_joints)
                    {
                        assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
                        assert(accessor.type == TINYGLTF_TYPE_VEC4);
                        assert(buffer_view.byteStride == sizeof(glm::vec<4, uint16_t>));

                        auto data = buffer.data.data() + accessor.byteOffset + buffer_view.byteOffset;
                        const auto *ptr = reinterpret_cast<const glm::vec<4, uint16_t> *>(data);
                        geometry->bone_indices = {ptr, ptr + accessor.count};
                    }
                    else if(attrib == attrib_weights)
                    {
                        assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                        assert(accessor.type == TINYGLTF_TYPE_VEC4);
                        auto data = buffer.data.data() + accessor.byteOffset + buffer_view.byteOffset;
                        const auto *ptr = reinterpret_cast<const glm::vec4 *>(data);
                        geometry->bone_weights = {ptr, ptr + accessor.count};

                    }
                }// for all attributes

                if(geometry->indices.empty())
                {
                    geometry->indices.resize(geometry->vertices.size());
                    std::iota(geometry->indices.begin(), geometry->indices.end(), 0);
                }

                // sanity resize
                geometry->colors.resize(geometry->vertices.size(), glm::vec4(1.f));
                geometry->tex_coords.resize(geometry->vertices.size(), glm::vec2(0.f));

//                if(!geometry->tex_coords.empty() && geometry->tangents.empty()){ geometry->compute_tangents(); }
//                else if(geometry->tangents.empty())
                {
                    geometry->tangents.resize(geometry->vertices.size(), glm::vec3(0.f));
                }

                vierkant::Mesh::entry_create_info_t create_info = {};
                create_info.geometry = geometry;
                create_info.transform = world_transform;
                create_info.node_index = out_assets.entry_create_infos.size();
                create_info.material_index = primitive.material;

                // pushback new entry
                out_assets.entry_create_infos.push_back(std::move(create_info));

            }// for all primitives
        }// mesh

        if(!node_map.count(current_index))
        {
            // cache node
            node_map[current_index] = current_node;
        }

        for(auto child_index : tiny_node.children)
        {
            if(child_index >= 0 && static_cast<size_t>(child_index) < model.nodes.size())
            {
                // enqueue child
                node_queue.push_back({static_cast<size_t>(child_index), world_transform, current_node});
            }
        }
    }

    // animations
    for(const auto &tiny_animation : model.animations)
    {
        auto node_animation = create_node_animation(tiny_animation, model, node_map);

        out_assets.node_animations.push_back(std::move(node_animation));
    }

    return out_assets;
}

}// namespace vierkant::model

