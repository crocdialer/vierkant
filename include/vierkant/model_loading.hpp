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

    // transmission
    float ior = 1.5f;
    glm::vec3 attenuation_color = glm::vec3(1.f);

    // volumes
    float transmission = 0.f;
    float attenuation_distance = std::numeric_limits<float>::infinity();

    // idk rasterizer only thingy
    float thickness = 1.f;

    vierkant::Material::BlendMode blend_mode = vierkant::Material::BlendMode::Opaque;
    float alpha_cutoff = 0.5f;

    bool twosided = false;

    crocore::ImagePtr img_diffuse;
    crocore::ImagePtr img_emission;

    crocore::ImagePtr img_normals;
    crocore::ImagePtr img_ao_roughness_metal;

    crocore::ImagePtr img_thickness;
    crocore::ImagePtr img_transmission;
};

/**
 * @brief   mesh_assets_t groups assets imported from a model-file.
 */
struct mesh_assets_t
{
    //! submesh entries
    std::vector <vierkant::Mesh::entry_create_info_t> entry_create_infos;

    //! common materials for all submeshes
    std::vector <material_t> materials;

    //! node-hierarchy for submeshes
    vierkant::nodes::NodePtr root_node;

    //! optional bone node-hierarchy
    vierkant::nodes::NodePtr root_bone;

    //! optional array of animations defined for nodes
    std::vector <vierkant::nodes::node_animation_t> node_animations;
};

/**
 * @brief   load_mesh can be used to load assets into gpu-buffers
 *          and construct a vierkant::Mesh usable for gpu-operations.
 *
 * @param   device          handle to a vierkant::Device
 * @param   mesh_assets     a struct grouping the assets to be extracted.
 * @param   load_queue      the VkQueue that shall be used for required image-transfers.
 * @param   buffer_flags    optionally pass additional buffer-flags for all created vierkant::Buffers.
 * @return  a vierkant::MeshPtr, nullptr in case of failure.
 */
vierkant::MeshPtr load_mesh(const vierkant::DevicePtr &device,
                            const vierkant::model::mesh_assets_t &mesh_assets,
                            VkQueue load_queue,
                            VkBufferUsageFlags buffer_flags);

}// namespace vierkant::model