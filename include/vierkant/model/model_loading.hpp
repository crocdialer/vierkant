//
// Created by crocdialer on 9/3/21.
//
#pragma once

#include <filesystem>
#include <optional>
#include <unordered_map>
#include <variant>

#include <crocore/Image.hpp>
#include <crocore/ThreadPoolClassic.hpp>

#include <vierkant/Geometry.hpp>
#include <vierkant/Material.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/camera_params.hpp>
#include <vierkant/hash.hpp>
#include <vierkant/texture_block_compression.hpp>

namespace vierkant
{

//! contains raw or packed geometry-information. either way 'can' be used to construct a mesh
using geometry_variant_t =
        std::variant<std::vector<vierkant::Mesh::entry_create_info_t>, vierkant::mesh_buffer_bundle_t>;

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
    std::unordered_map<vierkant::TextureId, texture_variant_t> textures;

    //! texture-sample-states for all materials
    std::unordered_map<vierkant::SamplerId, texture_sampler_t> texture_samplers;

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

struct omm_gen_params_t
{
    int   max_level   = 4;
    float target_edge = 0.5f;
    int   states      = 4;  // 4 = VK_OPACITY_MICROMAP_FORMAT_4_STATE_EXT
};

struct mesh_omm_entry_t
{
    std::vector<uint8_t>               data;
    std::vector<VkMicromapTriangleEXT> triangles;
    std::vector<int32_t>               indices;
};

struct mesh_omm_key_t
{
    vierkant::MeshId     mesh_id;
    uint32_t             entry_index = 0;
    vierkant::MaterialId material_id;
    bool operator==(const mesh_omm_key_t &) const = default;
};

using mesh_omm_cache_t = std::unordered_map<mesh_omm_key_t, mesh_omm_entry_t>;

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

    //! optional OMM generation parameters; nullopt = skip
    std::optional<omm_gen_params_t> omm_params;
};

/**
 *  @brief  model-loading facade-routine, delegating depending on file-type
 *
 *  @param  path    path to a supported model-file.
 *
 *  @return a struct grouping the loaded assets.
 */
std::optional<model_assets_t> load_model(const std::filesystem::path &path, crocore::ThreadPoolClassic *pool = nullptr);

//! struct grouping results of a load_mesh operation
struct load_mesh_result_t
{
    vierkant::MeshPtr mesh;
    std::unordered_map<vierkant::MaterialId, vierkant::material_t> materials;

    std::unordered_map<vierkant::TextureId, vierkant::ImagePtr> textures;
    std::unordered_map<vierkant::SamplerId, vierkant::VkSamplerPtr> samplers;

    //! CPU-side OMM data; caller accumulates into a scene-level cache and passes to RayBuilder
    mesh_omm_cache_t omm_cache;
};

/**
 * @brief   load_mesh can be used to load assets into gpu-buffers
 *          and construct a vierkant::Mesh usable for gpu-operations.
 *
 * @param   params  a struct grouping input-parameters
 * @return  a model::load_mesh_result_t, containing loaded mesh/textures/samplers.
 */
load_mesh_result_t load_mesh(const load_mesh_params_t &params, const vierkant::model::model_assets_t &mesh_assets);

/**
 * @brief   compress_textures will compress all images found provided mesh_assets in-place.
 *
 * @param   mesh_assets     a mesh_assets struct.
 * @return  true, if all images contained in mesh_assets are compressed.
 */
bool compress_textures(vierkant::model::model_assets_t &mesh_assets, crocore::ThreadPoolClassic *pool = nullptr);

/**
 * @brief   create_texture can be used to create a texture from an existing host-image
 *
 * @param   device              handle to a vierkant::Device
 * @param   img                 an image
 * @param   format              a vierkant::Image::Format struct providing sampler+texture settings
 * @param   load_queue          the VkQueue that shall be used for required image-transfers.
 * @return  a newly created texture
 */
vierkant::ImagePtr create_texture(const vierkant::DevicePtr &device, const crocore::ImagePtr &img,
                                  vierkant::Image::Format format, VkQueue load_queue);

/**
 * @brief   create_compressed_texture can be used to create a texture from pre-compressed block-compressed blocks.
 *          used format will be either VK_FORMAT_BC5_UNORM_BLOCK or VK_FORMAT_BC7_UNORM_BLOCK.
 *
 * @param   device              handle to a vierkant::Device
 * @param   compression_result  a struct providing compressed blocks
 * @param   format              a vierkant::Image::Format struct providing sampler+texture settings
 * @param   load_queue          the VkQueue that shall be used for required image-transfers.
 * @return  a newly created texture
 */
vierkant::ImagePtr create_compressed_texture(const vierkant::DevicePtr &device,
                                             const vierkant::bcn::compress_result_t &compression_result,
                                             vierkant::Image::Format format, VkQueue load_queue);

}// namespace vierkant::model

namespace std
{
template<>
struct hash<vierkant::model::mesh_omm_key_t>
{
    size_t operator()(const vierkant::model::mesh_omm_key_t &k) const noexcept
    {
        size_t h = 0;
        vierkant::hash_combine(h, k.mesh_id);
        vierkant::hash_combine(h, k.entry_index);
        vierkant::hash_combine(h, k.material_id);
        return h;
    }
};
}// namespace std