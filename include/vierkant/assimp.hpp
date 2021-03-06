#pragma once

#include <crocore/Image.hpp>
#include <crocore/ThreadPool.hpp>
#include <vierkant/Geometry.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/Material.hpp>

namespace vierkant::assimp
{

struct material_t
{
    glm::vec4 diffuse;
    glm::vec4 specular;
    glm::vec4 ambient;
    glm::vec4 emission;
    float roughness;
    float metalness;
    bool blending = false;
    bool wireframe = false;
    bool twosided = false;

    crocore::ImagePtr img_diffuse;
    crocore::ImagePtr img_emission;
//    crocore::ImagePtr img_specular;
//    crocore::ImagePtr img_ambient_occlusion;
//    crocore::ImagePtr img_roughness;
    crocore::ImagePtr img_normals;
    crocore::ImagePtr img_ao_roughness_metal;
};

struct mesh_assets_t
{
    // per submesh
    std::vector<vierkant::Mesh::entry_create_info_t> entry_create_infos;

    // global for mesh
    std::vector<material_t> materials;

    vierkant::nodes::NodePtr root_bone, root_node;
    std::vector<vierkant::nodes::node_animation_t> node_animations;
};

//! load a single 3D model from file
mesh_assets_t load_model(const std::string &path, const crocore::ThreadPool &threadpool);

//! load animations from file and add to existing geometry
size_t add_animations_to_mesh(const std::string &path, mesh_assets_t &mesh_assets);

} //namespace vierkant::assimp
