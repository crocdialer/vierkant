//
// Created by crocdialer on 9/3/21.
//
#pragma once

#include <filesystem>
#include <crocore/Image.hpp>
#include <crocore/ThreadPool.hpp>
#include <vierkant/Geometry.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/Material.hpp>

namespace vierkant::model
{

struct material_t
{
    std::string name;

    glm::vec4 diffuse;
    glm::vec3 emission;

    float roughness = 1.f;
    float metalness = 1.f;

    // deprecated !?
    glm::vec3 specular;

    // volumes
    float transmission = 0.f;
    glm::vec3 attenuation_color = glm::vec3(1.f);
    float attenuation_distance = 1.f;
    float ior = 1.5f;

    // idk rasterizer only thingy
    float thickness = 1.f;

    vierkant::Material::BlendMode blend_mode = vierkant::Material::BlendMode::Opaque;
    float alpha_cutoff = 1.f;

    bool wireframe = false;
    bool twosided = false;

    crocore::ImagePtr img_diffuse;
    crocore::ImagePtr img_emission;

    crocore::ImagePtr img_normals;
    crocore::ImagePtr img_ao_roughness_metal;

    crocore::ImagePtr img_thickness;
};

struct mesh_assets_t
{
    // per submesh
    std::vector <vierkant::Mesh::entry_create_info_t> entry_create_infos;

    // global for mesh
    std::vector <material_t> materials;

    vierkant::nodes::NodePtr root_bone, root_node;
    std::vector <vierkant::nodes::node_animation_t> node_animations;
};

}// namespace vierkant::model