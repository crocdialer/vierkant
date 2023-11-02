//
// Created by crocdialer on 1/26/21.
//
#pragma once

#include <crocore/Cache.hpp>

#include <vierkant/Device.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/Scene.hpp>
#include <vierkant/descriptor.hpp>
#include <vierkant/mesh_compute.hpp>
#include <vierkant/transform.hpp>

namespace vierkant
{

/**
 * @brief   RayBuilder can be used to create bottom and toplevel acceleration-structures
 *          used by raytracing pipelines.
 *
 */
class RayBuilder
{
public:
    struct alignas(16) entry_t
    {
        // per entry
        glm::mat4 texture_matrix = glm::mat4(1);
        vierkant::transform_t transform;
        uint32_t material_index = 0;

        int32_t vertex_offset = 0;
        uint32_t base_index = 0;

        // per mesh
        uint32_t buffer_index = 0;
    };

    struct alignas(16) material_struct_t
    {
        glm::vec4 color = glm::vec4(1);

        glm::vec4 emission = glm::vec4(0, 0, 0, 1);

        float metalness = 0.f;

        float roughness = 1.f;

        float transmission = 0.f;

        uint32_t null_surface = false;

        glm::vec3 attenuation_color = glm::vec3(1.f);

        float attenuation_distance = std::numeric_limits<float>::infinity();

        float ior = 1.5f;

        float clearcoat_factor = 0.f;

        float clearcoat_roughness_factor = 0.f;

        float sheen_roughness = 0.f;

        glm::vec4 sheen_color = glm::vec4(0.f);

        float iridescence_strength = 0.f;

        float iridescence_ior = 1.3f;

        // range of thin-film thickness in nanometers (nm)
        glm::vec2 iridescence_thickness_range = {100.f, 400.f};

        uint32_t albedo_index = 0;

        uint32_t normalmap_index = 0;

        uint32_t emission_index = 0;

        uint32_t ao_rough_metal_index = 0;

        uint32_t texture_type_flags = 0;

        uint32_t blend_mode = static_cast<uint32_t>(Material::BlendMode::Opaque);

        float alpha_cutoff = 0.5f;

        uint32_t two_sided = false;

        //! phase-function asymmetry parameter (forward- vs. back-scattering) [-1, 1]
        float phase_asymmetry_g = 0.f;

        //! ratio of scattering vs. absorption (sigma_s / sigma_t)
        float scattering_ratio = 0.f;
    };

    //! used for both bottom and toplevel acceleration-structures
    struct acceleration_asset_t
    {
        vierkant::AccelerationStructurePtr structure = nullptr;
        VkDeviceAddress device_address = 0;
        vierkant::BufferPtr buffer = nullptr;

        //! vertex- and index-buffers for the entire scene
        vierkant::BufferPtr vertex_buffer;
        VkDeviceSize vertex_buffer_offset;

        //! keep-alives, used during toplevel builds
        vierkant::BufferPtr instance_buffer = nullptr;
        vierkant::BufferPtr scratch_buffer = nullptr;
        vierkant::AccelerationStructurePtr update_structure = nullptr;
    };

    //! shared acceleration_asset_t
    using acceleration_asset_ptr = std::shared_ptr<acceleration_asset_t>;

    //! can be used to used to cache an array of shared (bottom-lvl) acceleration-structures per whatever
    using entity_asset_map_t = std::map<uint64_t, std::vector<RayBuilder::acceleration_asset_ptr>>;

    enum UpdateSemaphoreValue : uint64_t
    {
        INVALID = 0,
        MESH_COMPUTE,
        UPDATE_BOTTOM,
        UPDATE_TOP,
        MAX_VALUE,
    };

    //! opaque handle owning a scene_acceleration_context_t
    struct scene_acceleration_context_t;
    using scene_acceleration_context_ptr =
            std::unique_ptr<scene_acceleration_context_t, std::function<void(scene_acceleration_context_t *)>>;

    struct timings_t
    {
        double mesh_compute_ms = 0.0;
        double update_bottom_ms = 0.0;
        double update_top_ms = 0.0;
    };

    //! return an array listing required device-extensions for raytracing-acceleration structures.
    static std::vector<const char *> required_extensions();

    RayBuilder() = default;

    explicit RayBuilder(const vierkant::DevicePtr &device, VkQueue queue, vierkant::VmaPoolPtr pool = nullptr);

    /**
     * @brief   create_scene_acceleration_context is a factory to create a
     *          context for building acceleration structures for a scene.
     *
     * @return  a populated scene_acceleration_context_t.
     */
    scene_acceleration_context_ptr create_scene_acceleration_context();

    //! struct grouping return values of 'build_scene_acceleration'-routine.
    struct scene_acceleration_data_t
    {
        acceleration_asset_t top_lvl;
        vierkant::semaphore_submit_info_t semaphore_info;

        //! buffer containing entry-information
        vierkant::BufferPtr entry_buffer = nullptr;

        //! buffer containing material-information
        vierkant::BufferPtr material_buffer = nullptr;

        //! array containing all textures for a scene
        std::vector<vierkant::ImagePtr> textures;

        //! vertex- and index-buffers for the entire scene
        std::vector<vierkant::BufferPtr> vertex_buffers;
        std::vector<vierkant::BufferPtr> index_buffers;
        std::vector<VkDeviceSize> vertex_buffer_offsets;
        std::vector<VkDeviceSize> index_buffer_offsets;
    };

    //! struct grouping parameters for 'build_scene_acceleration'-routine.
    struct build_scene_acceleration_params_t
    {
        //! provided scene
        SceneConstPtr scene;

        //! enable mesh-compute for baking animated meshes per frame
        bool use_mesh_compute = true;

        //! enable compaction for bottom-lvl structures
        bool use_compaction = true;

        //! request to provide all vertex/index/material-buffers and textures.
        bool use_scene_assets = true;

        //! optionally provide a handle to a previous context, in order to re-use existing acceleration-assets.
        const scene_acceleration_context_t *previous_context = nullptr;
    };

    /**
     * @brief   'build_scene_acceleration' can be used to create assets required for raytracing a scene.
     *
     * internally it will bake vertex-buffers for animated meshes if necessary, build bottom- and top-level structures,
     * and provide all index/vertex-buffers/textures/materials for all objects if requested.
     *
     * @param   context an opaque context handle.
     * @param   scene   a provided scene.
     */
    scene_acceleration_data_t build_scene_acceleration(const scene_acceleration_context_ptr &context,
                                                       const build_scene_acceleration_params_t &params);

    /**
     * @brief   'timings' can be used to query gpu-timings for a recent run.
     *
     * @param   context an opaque context handle, used for the run to query timings
     * @return  a struct ggrouping timing-values.
     */
    timings_t timings(const scene_acceleration_context_ptr &context);

private:
    enum SemaphoreValueBuild : uint64_t
    {
        BUILD = 1,
        COMPACTED,
    };

    struct build_result_t
    {
        std::vector<acceleration_asset_ptr> acceleration_assets;
        std::vector<acceleration_asset_ptr> update_assets;
        std::vector<acceleration_asset_ptr> compacted_assets;
        vierkant::Semaphore semaphore;
        vierkant::QueryPoolPtr query_pool;

        bool compact = true;

        //! bottom-lvl-build
        vierkant::CommandBuffer build_command;

        //! copy/compaction
        vierkant::CommandBuffer compact_command;
    };

    struct create_mesh_structures_params_t
    {
        vierkant::MeshConstPtr mesh = nullptr;
        vierkant::semaphore_submit_info_t semaphore_info = {};

        //! optional override for vertex-buffer
        vierkant::BufferPtr vertex_buffer = nullptr;
        size_t vertex_buffer_offset = 0;

        bool enable_compaction = true;
        std::vector<acceleration_asset_ptr> update_assets = {};
    };

    /**
     * @brief   create_mesh_structures can be used to create new bottom-level acceleration structures
     *          for each mesh-entry.
     *
     * @param   mesh        a provided vierkant::MeshConstPtr
     * @param   transform   a provided transformation-matrix
     */
    [[nodiscard]] build_result_t create_mesh_structures(const create_mesh_structures_params_t &params) const;

    void compact(build_result_t &build_result) const;

    /**
     * @brief   create_toplevel will create a bundle containing a toplevel acceleration structure and all scene-assets.
     *          assuming required bottom-levels are already contained in the provided context.
     *
     * @param   context a provided scene_acceleration_context_ptr.
     * @param   params  provided params.
     * @param   last    optionally provide an existing acceleration-structure to update.
     * @return  a populated scene_acceleration_data_t bundle.
     */
    [[nodiscard]] scene_acceleration_data_t create_toplevel(const scene_acceleration_context_ptr &context,
                                                            const build_scene_acceleration_params_t &params,
                                                            const vierkant::AccelerationStructurePtr &last) const;

    [[nodiscard]] acceleration_asset_t
    create_acceleration_asset(VkAccelerationStructureCreateInfoKHR create_info) const;

    vierkant::DevicePtr m_device;

    VkPhysicalDeviceAccelerationStructurePropertiesKHR m_properties = {};

    VkQueue m_queue = VK_NULL_HANDLE;

    vierkant::VmaPoolPtr m_memory_pool = nullptr;

    vierkant::CommandPoolPtr m_command_pool;

    vierkant::ImagePtr m_placeholder_solid_white;

    vierkant::BufferPtr m_placeholder_buffer;
};

}// namespace vierkant
