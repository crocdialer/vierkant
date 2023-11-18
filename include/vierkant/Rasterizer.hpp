//
// Created by crocdialer on 03/22/19.
//

#pragma once

#include <mutex>

#include "vierkant/Camera.hpp"
#include "vierkant/Framebuffer.hpp"
#include "vierkant/Material.hpp"
#include "vierkant/Mesh.hpp"
#include "vierkant/Pipeline.hpp"
#include "vierkant/PipelineCache.hpp"
#include "vierkant/descriptor.hpp"

#include <vierkant/drawable.hpp>

namespace vierkant
{

using double_millisecond_t = std::chrono::duration<double, std::milli>;

/**
 * @brief   Rasterizer can be used to run arbitrary rasterization/graphics pipelines.
 *
 *          It will not render anything on its own, only record secondary command-buffers,
 *          meant to be executed within an existing renderpass.
 *
 *          Required resources like descriptor-sets and uniform-buffers will be created
 *          and kept alive, depending on the requested number of in-flight (pending) frames.
 *
 *          Renderer is NOT thread-safe, with the exception of stage_drawables(...).
 */
class Rasterizer
{
public:
    enum DescriptorBinding
    {
        BINDING_VERTICES = 0,
        BINDING_INDICES = 1,
        BINDING_DRAW_COMMANDS = 2,
        BINDING_MESH_DRAWS = 3,
        BINDING_MATERIAL = 4,
        BINDING_TEXTURES = 5,
        BINDING_BONE_VERTEX_DATA = 6,
        BINDING_BONES = 7,
        BINDING_PREVIOUS_BONES = 8,
        BINDING_JITTER_OFFSET = 9,
        BINDING_MORPH_TARGETS = 10,
        BINDING_MORPH_PARAMS = 11,
        BINDING_PREVIOUS_MORPH_PARAMS = 12,
        BINDING_MESHLETS = 13,
        BINDING_MESHLET_VERTICES = 14,
        BINDING_MESHLET_TRIANGLES = 15,
        BINDING_MAX_RANGE
    };

    struct alignas(16) mesh_draw_t
    {
        matrix_struct_t current_matrices = {};
        matrix_struct_t last_matrices = {};
        uint32_t mesh_index = 0;
        uint32_t material_index = 0;
    };

    struct alignas(16) mesh_entry_t
    {
        glm::vec3 center;
        float radius;

        uint32_t vertex_offset;
        uint32_t vertex_count;

        uint32_t lod_count;
        vierkant::Mesh::lod_t lods[8];
    };

    struct indexed_indirect_command_t
    {
        VkDrawIndexedIndirectCommand vk_draw = {};// size: 5

        VkDrawMeshTasksIndirectCommandEXT vk_mesh_draw = {};// size: 3

        uint32_t visible = false;
        uint32_t object_index = 0;
        uint32_t base_meshlet = 0;
        uint32_t num_meshlets = 0;
        uint32_t count_buffer_offset = 0;
        uint32_t first_draw_index = 0;
    };

    struct indirect_draw_bundle_t
    {
        //! number of array-elements in 'draws_in'
        uint32_t num_draws = 0;

        //! device array containing any array of mesh_draw_t
        vierkant::BufferPtr mesh_draws;

        //! device array containing any array of mesh_entry_t
        vierkant::BufferPtr mesh_entries;

        //! device array containing any array of material_t
        vierkant::BufferPtr materials;

        //! host-visible array of indexed_indirect_command_t
        vierkant::BufferPtr draws_in;

        //! device array of indexed_indirect_command_t
        vierkant::BufferPtr draws_out;

        //! device array of uint32_t
        vierkant::BufferPtr draws_counts_out;
    };

    //! define syntax for a culling-delegate
    using indirect_draw_delegate_t = std::function<void(indirect_draw_bundle_t &)>;

    struct create_info_t
    {
        VkViewport viewport = {.x = 0.f, .y = 0.f, .width = 1.f, .height = 1.f, .minDepth = 0.f, .maxDepth = 1.f};
        VkRect2D scissor = {.offset = {0, 0}, .extent = {0, 0}};
        uint32_t num_frames_in_flight = 1;
        VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
        bool indirect_draw = false;
        bool enable_mesh_shader = false;
        vierkant::PipelineCachePtr pipeline_cache = nullptr;
        vierkant::CommandPoolPtr command_pool = nullptr;
        vierkant::DescriptorPoolPtr descriptor_pool = nullptr;
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t random_seed = 0;
        std::optional<vierkant::debug_label_t> debug_label;
    };

    //! struct grouping information for direct-rendering
    struct rendering_info_t
    {
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        uint32_t view_mask = 0;
        std::vector<VkFormat> color_attachment_formats;
        VkFormat depth_attachment_format = VK_FORMAT_UNDEFINED;
        VkFormat stencil_attachment_format = VK_FORMAT_UNDEFINED;
    };

    //! num samples used.
    VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;

    //! Viewport parameters currently used.
    VkViewport viewport = {.x = 0.f, .y = 0.f, .width = 1.f, .height = 1.f, .minDepth = 0.f, .maxDepth = 1.f};

    //! Scissor parameters currently used.
    VkRect2D scissor = {.offset = {0, 0}, .extent = {0, 0}};

    //! option to disable colors from materials.
    bool disable_material = false;

    //! option to use indirect drawing
    bool indirect_draw = false;

    //! option to use a meshlet-based pipeline
    bool use_mesh_shader = false;

    //! optional flag to visualize object/meshlet indices
    bool debug_draw_ids = false;

    //! optional label for frame-debugging
    std::optional<vierkant::debug_label_t> debug_label;

    //! optional cull-delegate
    indirect_draw_delegate_t draw_indirect_delegate;

    Rasterizer() = default;

    /**
     * @brief   Construct a new Renderer object
     * @param   device          handle for the vk::Device to create the Renderer
     * @param   create_info     a create_info_t object
     */
    Rasterizer(DevicePtr device, const create_info_t &create_info);

    Rasterizer(Rasterizer &&other) noexcept;

    Rasterizer(const Rasterizer &) = delete;

    Rasterizer &operator=(Rasterizer other);

    /**
     * @brief   Stage a drawable to be rendered.
     *
     * @param   drawable    a drawable_t object.
     */
    void stage_drawable(drawable_t drawable);

    /**
     * @brief   Stage an ordered sequence of drawables to be rendered.
     *
     * @param   drawables    an array of drawable_t objects.
     */
    void stage_drawables(std::vector<drawable_t> drawables);

    /**
     * @brief   Records drawing-commands for all staged drawables into a secondary VkCommandBuffer.
     *          Also advances the current in-flight-index.
     *
     * @return  handle to the recorded VkCommandBuffer.
     */
    VkCommandBuffer render(const vierkant::Framebuffer &framebuffer, bool recycle_commands = false);

    /**
     * @brief   Records drawing-commands for all staged drawables into a provided command-buffer,
     *          using direct-rendering.
     *
     * @param   rendering_info  a struct grouping parameters for a direct-rendering pass.
     */
    void render(const rendering_info_t &rendering_info);

    /**
     * @return  the current frame-index.
     */
    [[nodiscard]] uint32_t current_index() const { return m_current_index; }

    /**
     * @return  the number of concurrent (in-flight) frames.
     */
    [[nodiscard]] uint32_t num_concurrent_frames() const { return static_cast<uint32_t>(m_frame_assets.size()); }

    /**
     * @return  last measured frame's millisecond-duration
     */
    [[nodiscard]] double_millisecond_t last_frame_ms() const { return m_frame_assets[m_current_index].frame_time; }

    /**
     * @brief   Release all cached rendering assets.
     */
    void reset();

    [[nodiscard]] const vierkant::DevicePtr &device() const { return m_device; }

    friend void swap(Rasterizer &lhs, Rasterizer &rhs) noexcept;

private:
    struct alignas(16) push_constants_t
    {
        //! current viewport-size
        glm::vec2 size;

        //! current time since start in seconds
        float time;

        //! seed for shader-based rng
        uint32_t random_seed = 0;

        //! optional flag to disable colors from materials
        int disable_material = 0;

        //! optional flag to visualize object/meshlet indices
        int debug_draw_ids = 0;

        //! base index into an array of indexed_indirect_command_t
        uint32_t base_draw_index = 0;
    };

    struct frame_assets_t
    {
        std::unordered_map<descriptor_map_t, DescriptorSetLayoutPtr> descriptor_set_layouts;
        descriptor_set_map_t descriptor_sets;

        // SSBOs containing everything (using gpu-mem iff a queue was provided)
        vierkant::BufferPtr mesh_draw_buffer;
        vierkant::BufferPtr mesh_entry_buffer;
        vierkant::BufferPtr material_buffer;

        // host visible keep-alive staging-buffer
        vierkant::BufferPtr staging_buffer;

        // draw-indirect buffers
        indirect_draw_bundle_t indirect_bundle;
        indirect_draw_bundle_t indirect_indexed_bundle;

        std::vector<drawable_t> drawables;
        vierkant::CommandBuffer command_buffer, staging_command_buffer;

        // used for gpu timestamps
        vierkant::QueryPoolPtr query_pool;
        double_millisecond_t frame_time;
    };

    //! internal rendering-workhorse, creating assets and recording drawing-commands
    void render(VkCommandBuffer command_buffer, frame_assets_t &frame_assets);

    //! update the combined uniform buffers
    void update_buffers(const std::vector<drawable_t> &drawables, frame_assets_t &frame_asset);

    //! create/resize draw_indirect buffers
    void resize_draw_indirect_buffers(uint32_t num_drawables, frame_assets_t &frame_assets);

    //! increment counter, retrieve next frame-assets, update timings, ...
    frame_assets_t &next_frame();

    DevicePtr m_device;

    vierkant::PipelineCachePtr m_pipeline_cache;

    VkQueue m_queue = VK_NULL_HANDLE;

    vierkant::CommandPoolPtr m_command_pool;

    vierkant::DescriptorPoolPtr m_descriptor_pool;

    std::vector<std::vector<drawable_t>> m_staged_drawables;

    std::vector<frame_assets_t> m_frame_assets;

    std::mutex m_staging_mutex;

    uint32_t m_current_index = 0;

    VkPushConstantRange m_push_constant_range = {};

    std::chrono::steady_clock::time_point m_start_time = std::chrono::steady_clock::now();

    std::default_random_engine m_random_engine;

    uint32_t m_mesh_task_count = 0;
};

}//namespace vierkant