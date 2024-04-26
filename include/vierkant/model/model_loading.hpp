//
// Created by crocdialer on 9/3/21.
//
#pragma once

#include <filesystem>
#include <optional>
#include <variant>

#include <crocore/Image.hpp>
#include <crocore/ThreadPool.hpp>

#include <vierkant/Geometry.hpp>
#include <vierkant/Material.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/bc7.hpp>
#include <vierkant/camera_params.hpp>

namespace vierkant
{

//! contains uncompressed or BC7-compressed images
using texture_variant_t = std::variant<crocore::ImagePtr, vierkant::bc7::compress_result_t>;

//! contains raw or packed geometry-information. either way 'can' be used to construct a mesh
using geometry_variant_t = std::variant<std::vector<vierkant::Mesh::entry_create_info_t>,
                                        vierkant::mesh_buffer_bundle_t>;

}// namespace vierkant


namespace vierkant::model
{

enum class LightType : uint32_t
{
    Omni = 0,
    Spot,
    Directional
};

//! adhoc lightsource_t
struct lightsource_t
{
    glm::vec3 position;
    LightType type = LightType::Omni;
    glm::vec3 color = glm::vec3(1);
    float intensity = 1.f;
    glm::vec3 direction = glm::vec3(0.f, 0.f, -1.f);
    float range = std::numeric_limits<float>::infinity();
    float inner_cone_angle = 0.f;
    float outer_cone_angle = glm::quarter_pi<float>();
};

//! adhoc camera_t
struct camera_t
{
    vierkant::transform_t transform = {};
    vierkant::physical_camera_params_t params = {};
};

/**
 * @brief   mesh_assets_t groups assets imported from a model-file.
 */
struct model_assets_t
{
    //! vertex/index/meshlet/submesh data for a mesh with submeshes
    geometry_variant_t geometry_data;

    //! common materials for all submeshes
    std::vector<vierkant::material_t> materials;

    //! common textures for all materials
    std::unordered_map<vierkant::TextureSourceId, texture_variant_t> textures;

    //! texture-sample-states for all materials
    std::unordered_map<vierkant::SamplerId , texture_sampler_t> texture_samplers;

    //! optional lights defined in model-file
    std::vector<lightsource_t> lights;

    //! optional cameras defined in model-file
    std::vector<camera_t> cameras;

    //! node-hierarchy for submeshes
    vierkant::nodes::NodePtr root_node;

    //! optional bone node-hierarchy
    vierkant::nodes::NodePtr root_bone;

    //! optional array of animations defined for nodes
    std::vector<vierkant::nodes::node_animation_t> node_animations;
};

struct load_mesh_params_t
{
    //! handle to a vierkant::Device
    vierkant::DevicePtr device;

    //! parameters for creation of vertex-buffers
    mesh_buffer_params_t mesh_buffers_params = {};

    //! a VkQueue used for required buffer/image-transfers.
    VkQueue load_queue = VK_NULL_HANDLE;

    //! additional buffer-flags for all created vierkant::Buffers.
    VkBufferUsageFlags buffer_flags = 0;
};

/**
 *  @brief  model-loading facade-routine, delegating depending on file-type
 *
 *  @param  path    path to a supported model-file.
 *
 *  @return a struct grouping the loaded assets.
 */
std::optional<model_assets_t> load_model(const std::filesystem::path &path, crocore::ThreadPool *pool = nullptr);

/**
 * @brief   load_mesh can be used to load assets into gpu-buffers
 *          and construct a vierkant::Mesh usable for gpu-operations.
 *
 * @param   params  a struct grouping input-parameters
 * @return  a vierkant::MeshPtr, nullptr in case of failure.
 */
vierkant::MeshPtr load_mesh(const load_mesh_params_t &params, const vierkant::model::model_assets_t &mesh_assets);

/**
 * @brief   compress_textures will compress all images found provided mesh_assets in-place.
 *
 * @param   mesh_assets     a mesh_assets struct.
 * @return  true, if all images contained in mesh_assets are compressed.
 */
bool compress_textures(vierkant::model::model_assets_t &mesh_assets);

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
                                             vierkant::Image::Format format, VkQueue load_queue);

}// namespace vierkant::model