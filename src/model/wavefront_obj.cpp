//
// Created by crocdialer on 31.08.23.
//

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <vierkant/model/wavefront_obj.hpp>

namespace vierkant::model
{

vierkant::GeometryPtr create_geometry(const tinyobj::attrib_t &inattrib, const std::vector<tinyobj::index_t> &indices)
{
    auto geom = vierkant::Geometry::create();

    bool has_normals = !inattrib.normals.empty();

    // start filled with zeros
    geom->positions.resize(indices.size(), glm::vec3(0.f));
    geom->tex_coords.resize(indices.size(), glm::vec2(0.f));
    if(has_normals) { geom->normals.resize(indices.size(), glm::vec3(0.f)); }

    auto pos_start = reinterpret_cast<const glm::vec<3, tinyobj::real_t, glm::defaultp> *>(inattrib.vertices.data());
    auto normal_start = reinterpret_cast<const glm::vec<3, tinyobj::real_t, glm::defaultp> *>(inattrib.normals.data());
    auto texcoord_start =
            reinterpret_cast<const glm::vec<2, tinyobj::real_t, glm::defaultp> *>(inattrib.texcoords.data());

    for(uint32_t i = 0; i < indices.size(); ++i)
    {
        if(indices[i].vertex_index >= 0) { geom->positions[i] = pos_start[indices[i].vertex_index]; }
        if(has_normals && indices[i].normal_index >= 0) { geom->normals[i] = normal_start[indices[i].normal_index]; }
        if(indices[i].texcoord_index >= 0) { geom->tex_coords[i] = texcoord_start[indices[i].texcoord_index]; }
    }

    // iota fill indices
    geom->indices.resize(indices.size());
    std::iota(geom->indices.begin(), geom->indices.end(), 0);

    // calculate missing normals
    if(geom->normals.empty()) { geom->compute_vertex_normals(); }

    // calculate missing tangents
    if(geom->tangents.empty() && !geom->tex_coords.empty()) { geom->compute_tangents(); }
    return geom;
}

std::optional<model_assets_t> wavefront_obj(const std::filesystem::path &path, crocore::ThreadPool * /*pool*/)
{
    if(!exists(path) || !is_regular_file(path)) { return {}; }

    tinyobj::attrib_t inattrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // use obj-file's location as base-dir for material .mtl search
    auto base_dir = path.parent_path();

    bool ret = tinyobj::LoadObj(&inattrib, &shapes, &materials, &warn, &err, path.string().c_str(),
                                base_dir.string().c_str());
    if(!warn.empty()) { spdlog::warn(warn); }
    if(!err.empty()) { spdlog::error(warn); }
    if(!ret) { spdlog::error("failed to load {}", path.string()); }

    std::unordered_map<std::string, std::tuple<TextureSourceId, crocore::ImagePtr>> image_cache;
    auto get_image = [&image_cache](const std::string &path) -> std::tuple<TextureSourceId, crocore::ImagePtr> {
        auto it = image_cache.find(path);
        if(it != image_cache.end()) { return it->second; }

        std::tuple<TextureSourceId, crocore::ImagePtr> ret;
        try
        {
            ret = {TextureSourceId::random(), crocore::create_image_from_file(path, 4)};
            image_cache[path] = ret;
        } catch(std::exception &e)
        {
            spdlog::warn(e.what());
        }
        return ret;
    };

    model_assets_t mesh_assets = {};
    mesh_assets.root_node = std::make_shared<vierkant::nodes::node_t>();

    for(const auto &mat: materials)
    {
        vierkant::material_t m = {};
        m.name = mat.name;
        m.base_color = {mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], std::clamp(1.f - mat.dissolve, 0.f, 1.f)};
        m.emission = {mat.emission[0], mat.emission[1], mat.emission[2]};
        m.roughness = std::clamp(std::max(mat.roughness, std::pow(1.f - mat.shininess, 2.f)), 0.f, 1.f);
        m.metalness = mat.metallic;
        m.clearcoat_roughness_factor = mat.clearcoat_roughness;
        //        m.transmission = mat.transmittance[0];
        m.ior = mat.ior;
        m.clearcoat_roughness_factor = mat.clearcoat_roughness;

        // vertically flip textures
        m.texture_transform[1][1] = -1.f;

        if(!mat.diffuse_texname.empty())
        {
            auto [tex_id, img] = get_image((base_dir / mat.diffuse_texname).string());
            m.textures[vierkant::TextureType::Color] = tex_id;
            mesh_assets.textures[tex_id] = img;
        }
        if(!mat.normal_texname.empty())
        {
            auto [tex_id, img] = get_image((base_dir / mat.normal_texname).string());
            m.textures[vierkant::TextureType::Normal] = tex_id;
            mesh_assets.textures[tex_id] = img;
        }
        mesh_assets.materials.push_back(m);
    }
    // fallback material
    if(mesh_assets.materials.empty()) { mesh_assets.materials.push_back({}); }

    std::vector<vierkant::Mesh::entry_create_info_t> entry_create_infos;

    for(const auto &shape: shapes)
    {
        vierkant::Mesh::entry_create_info_t entry_info = {};
        entry_info.geometry = create_geometry(inattrib, shape.mesh.indices);
        entry_info.name = shape.name;
        entry_info.material_index = shape.mesh.material_ids.empty() ? 0 : std::max(shape.mesh.material_ids.front(), 0);
        entry_create_infos.push_back(entry_info);
    }
    mesh_assets.geometry_data = entry_create_infos;
    return mesh_assets;
}

}// namespace vierkant::model