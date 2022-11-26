//
// Created by crocdialer on 9/3/21.
//
#pragma once

#include <filesystem>
#include <optional>
#include <crocore/Image.hpp>
#include <vierkant/Geometry.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/Material.hpp>
#include <vierkant/punctual_light.hpp>
#include <vierkant/bc7.hpp>

namespace vierkant::model
{

struct material_t
{
    std::string name;

    glm::vec4 base_color;
    glm::vec3 emission;
    float emissive_strength = 1.f;

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

    // specular
    float specular_factor = 1.f;
    glm::vec3 specular_color_factor = glm::vec3(1.f);

    // clearcoat
    float clearcoat_factor = 0.f;
    float clearcoat_roughness_factor = 0.f;

    // sheen
    glm::vec3 sheen_color = glm::vec3(0.f);
    float sheen_roughness = 0.f;

    // iridescence
    float iridescence_factor = 0.f;
    float iridescence_ior = 1.3f;

    // iridescence thin-film layer given in nanometers (nm)
    glm::vec2 iridescence_thickness_range = {100.f, 400.f};

    // optional texture-transform (todo: per image)
    glm::mat4 texture_transform = glm::mat4(1);

    std::vector<crocore::ImagePtr> images;

    crocore::ImagePtr img_diffuse;
    crocore::ImagePtr img_emission;

    crocore::ImagePtr img_normals;
    crocore::ImagePtr img_ao_roughness_metal;

    crocore::ImagePtr img_thickness;
    crocore::ImagePtr img_transmission;

    crocore::ImagePtr img_clearcoat;

    crocore::ImagePtr img_sheen_color;
    crocore::ImagePtr img_sheen_roughness;

    crocore::ImagePtr img_specular;
    crocore::ImagePtr img_specular_color;

    // iridescence intensity/thickness stored in RG-channels
    crocore::ImagePtr img_iridescence;
};

/**
 * @brief   mesh_assets_t groups assets imported from a model-file.
 */
struct mesh_assets_t
{
    //! submesh entries
    std::vector<vierkant::Mesh::entry_create_info_t> entry_create_infos;

    //! common materials for all submeshes
    std::vector<material_t> materials;

    //! optional lights defined in model-file
    std::vector<lightsource_t> lights;

    //! node-hierarchy for submeshes
    vierkant::nodes::NodePtr root_node;

    //! optional bone node-hierarchy
    vierkant::nodes::NodePtr root_bone;

    //! optional array of animations defined for nodes
    std::vector<vierkant::nodes::node_animation_t> node_animations;
};

struct asset_bundle_t
{
    vierkant::mesh_buffer_bundle_t mesh_buffer_bundle;
    std::vector<vierkant::bc7::compress_result_t> compressed_images;
};

struct load_mesh_params_t
{
    //! handle to a vierkant::Device
    vierkant::DevicePtr device;

    bool compress_textures = false;
    bool optimize_vertex_cache = true;
    bool generate_lods = false;
    bool generate_meshlets = false;

    //! a VkQueue used for required buffer/image-transfers.
    VkQueue load_queue = VK_NULL_HANDLE;

    //! additional buffer-flags for all created vierkant::Buffers.
    VkBufferUsageFlags buffer_flags = 0;
};

/**
 * @brief   load_mesh can be used to load assets into gpu-buffers
 *          and construct a vierkant::Mesh usable for gpu-operations.
 *
 * @param   params  a struct grouping input-parameters
 * @return  a vierkant::MeshPtr, nullptr in case of failure.
 */
vierkant::MeshPtr load_mesh(const load_mesh_params_t &params,
                            const vierkant::model::mesh_assets_t &mesh_assets,
                            const std::optional<asset_bundle_t> &asset_bundle = {});

std::vector<vierkant::bc7::compress_result_t>
create_compressed_images(const std::vector<vierkant::model::material_t> &materials);

/**
 * @brief   create_compressed_texture can be used to create a texture from pre-compressed bc7 blocks.
 *          used format will be VK_FORMAT_BC7_UNORM_BLOCK.
 *
 * @param   device              handle to a vierkant::Device
 * @param   compression_result  a struct providing compressed bc7-blocks
 * @param   format              a vierkant::Image::Format struct providing sampler+texture settings
 * @param   load_queue          the VkQueue that shall be used for required image-transfers.
 * @return  a newly created texture
 */
vierkant::ImagePtr create_compressed_texture(const vierkant::DevicePtr &device,
                                             const vierkant::bc7::compress_result_t &compression_result,
                                             vierkant::Image::Format format,
                                             VkQueue load_queue);

}// namespace vierkant::model