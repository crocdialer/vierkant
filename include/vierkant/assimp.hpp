#pragma once

#include <vierkant/Geometry.hpp>
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
    bool blending = false;
    bool wireframe = false;
    bool twosided = false;

    crocore::ImagePtr img_diffuse;
    crocore::ImagePtr img_emission;
    crocore::ImagePtr img_specular;
    crocore::ImagePtr img_ambient_occlusion;
    crocore::ImagePtr img_roughness;
    crocore::ImagePtr img_normals;
    crocore::ImagePtr img_ao_roughness_metal;
};

struct mesh_assets_t
{
    // per submesh
    std::vector<vierkant::GeometryPtr> geometries;
    std::vector<glm::mat4> transforms;
    std::vector<uint32_t> material_indices;

    // global for mesh
    std::vector<material_t> materials;
    vierkant::bones::BonePtr root_bone;
    std::vector<vierkant::bones::bone_animation_t> animations;
};

//! load a single 3D model from file
mesh_assets_t load_model(const std::string &path);

//! load animations from file and add to existing geometry
size_t add_animations_to_mesh(const std::string &path, mesh_assets_t& mesh_assets);

} //namespace vierkant::assimp
