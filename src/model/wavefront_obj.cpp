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

    // start filled with zeros
    geom->positions.resize(indices.size(), glm::vec3(0.f));
    geom->tex_coords.resize(indices.size(), glm::vec2(0.f));
    geom->normals.resize(indices.size(), glm::vec3(0.f));

    auto pos_start = reinterpret_cast<const glm::vec<3, tinyobj::real_t, glm::defaultp> *>(inattrib.vertices.data());
    auto normal_start = reinterpret_cast<const glm::vec<3, tinyobj::real_t, glm::defaultp> *>(inattrib.normals.data());
    auto texcoord_start =
            reinterpret_cast<const glm::vec<2, tinyobj::real_t, glm::defaultp> *>(inattrib.texcoords.data());

    for(uint32_t i = 0; i < indices.size(); ++i)
    {
        if(indices[i].vertex_index >= 0) { geom->positions[i] = pos_start[indices[i].vertex_index]; }
        if(indices[i].normal_index >= 0) { geom->normals[i] = normal_start[indices[i].normal_index]; }
        if(indices[i].texcoord_index >= 0) { geom->tex_coords[i] = texcoord_start[indices[i].texcoord_index]; }
    }

    // iota fill indices
    geom->indices.resize(indices.size());
    std::iota(geom->indices.begin(), geom->indices.end(), 0);

    // calculate missing tangents
    if(geom->tangents.empty() && !geom->tex_coords.empty()) { geom->compute_tangents(); }
    return geom;
}

mesh_assets_t wavefront_obj(const std::filesystem::path &path, crocore::ThreadPool * /*pool*/)
{
    if(!exists(path) || !is_regular_file(path)) { return {}; }

    tinyobj::attrib_t inattrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // use obj-file's location as base-dir for material .mtl search
    std::string base_dir = path.parent_path().string();

    bool ret = tinyobj::LoadObj(&inattrib, &shapes, &materials, &warn, &err, path.string().c_str(), base_dir.c_str());
    if(!warn.empty()) { spdlog::warn(warn); }
    if(!err.empty()) { spdlog::error(warn); }
    if(!ret) { spdlog::error("failed to load {}", path.string()); }

    mesh_assets_t mesh_assets = {};
    mesh_assets.root_node = std::make_shared<vierkant::nodes::node_t>();

    for(const auto &mat: materials)
    {
        vierkant::model::material_t m = {};
        m.name = mat.name;
        m.base_color = {mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], 1.f};
        m.emission = {mat.emission[0], mat.emission[1], mat.emission[2]};

        if(!mat.diffuse_texname.empty())
        {
            spdlog::debug("tinyobjloader: found diffuse-texture: {}", mat.diffuse_texname);
        }
        if(!mat.normal_texname.empty())
        {
            spdlog::debug("tinyobjloader: found normal-texture: {}", mat.normal_texname);
        }
        mesh_assets.materials.push_back(m);
    }

    for(const auto &shape: shapes)
    {
        spdlog::debug("tinyobjloader: found shape {}", shape.name);

        vierkant::Mesh::entry_create_info_t entry_info = {};
        entry_info.geometry = create_geometry(inattrib, shape.mesh.indices);
        entry_info.name = shape.name;
        entry_info.material_index = shape.mesh.material_ids.empty() ? 0 : shape.mesh.material_ids.front();
        mesh_assets.entry_create_infos.push_back(entry_info);
    }
    return mesh_assets;
}

}// namespace vierkant::model