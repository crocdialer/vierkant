//
// Created by crocdialer on 9/3/21.
//
#pragma once

#include <filesystem>
#include <optional>
#include <variant>

#include <crocore/Image.hpp>
#include <crocore/NamedUUID.hpp>
#include <crocore/ThreadPool.hpp>

#include <vierkant/Geometry.hpp>
#include <vierkant/Material.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/bc7.hpp>
#include <vierkant/physical_camera_params.hpp>

namespace vierkant
{
DEFINE_NAMED_UUID(TextureId)
DEFINE_NAMED_UUID(SamplerId)

//! contains uncompressed or BC7-compressed images
using texture_variant_t = std::variant<crocore::ImagePtr, vierkant::bc7::compress_result_t>;
}// namespace vierkant


namespace vierkant::model
{

struct material_t
{
    std::string name;

    glm::vec4 base_color = glm::vec4(1.f);
    glm::vec3 emission;
    float emissive_strength = 1.f;

    float roughness = 1.f;
    float metalness = 0.f;

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
    glm::vec3 specular_color = glm::vec3(1.f);

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

    // maps TextureType to a TextureId/SamplerId. sorted in enum order, which is important in other places.
    std::map<Material::TextureType, vierkant::TextureId> textures;
//    std::map<Material::TextureType, vierkant::SamplerId> samplers;
};

struct texture_sampler_state_t
{
    enum class Filter
    {
        NEAREST = 0,
        LINEAR,
        CUBIC
    };

    enum class AddressMode
    {
        REPEAT = 0,
        MIRRORED_REPEAT,
        CLAMP_TO_EDGE,
        CLAMP_TO_BORDER,
        MIRROR_CLAMP_TO_EDGE,
    };

    AddressMode address_mode_u = AddressMode::REPEAT;
    AddressMode address_mode_v = AddressMode::REPEAT;

    Filter min_filter = Filter::LINEAR;
    Filter mag_filter = Filter::LINEAR;
    glm::mat4 transform = glm::mat4(1);
};

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
struct mesh_assets_t
{
    //! submesh entries
    std::vector<vierkant::Mesh::entry_create_info_t> entry_create_infos;

    //! common materials for all submeshes
    std::vector<material_t> materials;

    //! common textures for all materials
    std::unordered_map<vierkant::TextureId, texture_variant_t> textures;

    //! texture-sample-states for all materials (TODO: correct this to use SamplerId)
    std::unordered_map<vierkant::TextureId, texture_sampler_state_t> texture_sampler_states;

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

struct asset_bundle_t
{
    //! packed vertex/index/meshlet-buffers with entry information
    vierkant::mesh_buffer_bundle_t mesh_buffer_bundle;

    //! common materials for all meshes
    std::vector<material_t> materials;

    //! common textures for all materials
    std::unordered_map<vierkant::TextureId, texture_variant_t> textures;
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
std::optional<mesh_assets_t> load_model(const std::filesystem::path &path, crocore::ThreadPool *pool = nullptr);

/**
 * @brief   load_mesh can be used to load assets into gpu-buffers
 *          and construct a vierkant::Mesh usable for gpu-operations.
 *
 * @param   params  a struct grouping input-parameters
 * @return  a vierkant::MeshPtr, nullptr in case of failure.
 */
vierkant::MeshPtr load_mesh(const load_mesh_params_t &params, const vierkant::model::mesh_assets_t &sampler_state,
                            const std::optional<asset_bundle_t> &asset_bundle = {});

/**
 * @brief   compress_textures will compress all images found in contained materials in-place.
 *
 * @param   mesh_assets     a mesh_assets struct.
 * @return  true, if all images contained in mesh_assets are compressed.
 */
bool compress_textures(vierkant::model::mesh_assets_t &mesh_assets);

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