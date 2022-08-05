//
// Created by crocdialer on 03/22/19.
//

#pragma once

#include <optional>
#include <mutex>
#include "crocore/Area.hpp"

#include "vierkant/Mesh.hpp"
#include "vierkant/descriptor.hpp"
#include "vierkant/Framebuffer.hpp"
#include "vierkant/PipelineCache.hpp"
#include "vierkant/Pipeline.hpp"
#include "vierkant/Camera.hpp"
#include "vierkant/Material.hpp"

namespace vierkant
{

using frame_millisecond_t = std::chrono::duration<double, std::milli>;

/**
 * @brief   Renderer can be used to run arbitrary rasterization/graphics pipelines.
 *
 *          It will not render anything on its own, only record secondary command-buffers,
 *          meant to be executed within an existing renderpass.
 *
 *          Required resources like descriptor-sets and uniform-buffers will be created
 *          and kept alive, depending on the requested number of in-flight (pending) frames.
 *
 *          Renderer is NOT thread-safe, with the exception of stage_drawables(...).
 */
class Renderer
{
public:

    enum DescriptorBinding
    {
        BINDING_VERTICES = 0,
        BINDING_INDICES = 1,
        BINDING_DRAW_COMMANDS = 2,
        BINDING_MATRIX = 3,
        BINDING_PREVIOUS_MATRIX = 4,
        BINDING_MATERIAL = 5,
        BINDING_TEXTURES = 6,
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

    struct alignas(16) matrix_struct_t
    {
        glm::mat4 modelview = glm::mat4(1);
        glm::mat4 projection = glm::mat4(1);
        glm::mat4 normal = glm::mat4(1);
        glm::mat4 texture = glm::mat4(1);
    };

    struct alignas(16) material_struct_t
    {
        glm::vec4 color = glm::vec4(1);

        glm::vec4 emission = glm::vec4(0, 0, 0, 1);

        float metalness = 0.f;

        float roughness = 1.f;

        float ambient = 1.f;

        uint32_t blend_mode = static_cast<uint32_t>(Material::BlendMode::Opaque);

        float alpha_cutoff = 0.5f;

        float iridescence_factor = 0.f;

        float iridescence_ior = 1.3f;

        uint32_t padding[1];

        // range of thin-film thickness in nanometers (nm)
        glm::vec2 iridescence_thickness_range = {100.f, 400.f};

        uint32_t base_texture_index = 0;

        uint32_t texture_type_flags = 0;
    };

    struct alignas(16) indexed_indirect_command_t
    {
        VkDrawIndexedIndirectCommand vk_draw;// size: 5

        VkDrawMeshTasksIndirectCommandNV vk_mesh_draw;// size: 2

        uint32_t object_index = 0;
        uint32_t visible = false;
        uint32_t base_meshlet = 0;
        uint32_t count_buffer_offset = 0;
        uint32_t first_draw_index = 0;
        glm::vec4 sphere_bounds = {};
    };

    struct indirect_draw_bundle_t
    {
        //! number of array-elements in 'draws_in'
        uint32_t num_draws = 0;

        //! host-visible array of indexed_indirect_command_t
        vierkant::BufferPtr draws_in;

        //! device array of indexed_indirect_command_t
        vierkant::BufferPtr draws_out;

        //! device array of uint32_t
        vierkant::BufferPtr draws_counts_out;
    };

    //! define syntax for a culling-delegate
    using indirect_draw_delegate_t = std::function<void(indirect_draw_bundle_t&)>;

    /**
     * @brief   drawable_t groups all necessary information for a drawable object.
     */
    struct drawable_t
    {
        MeshConstPtr mesh;

        uint32_t entry_index = 0;

        graphics_pipeline_info_t pipeline_format = {};

        matrix_struct_t matrices = {};

        std::optional<matrix_struct_t> last_matrices;

        material_struct_t material = {};

        //! a descriptormap
        descriptor_map_t descriptors;

        //! optional descriptor-set-layout
        DescriptorSetLayoutPtr descriptor_set_layout;

        //! binary blob for push-constants
        std::vector<uint8_t> push_constants;

        uint32_t base_index = 0;
        uint32_t num_indices = 0;

        int32_t vertex_offset = 0;
        uint32_t num_vertices = 0;

        uint32_t morph_vertex_offset = 0;
        std::vector<float> morph_weights;

        uint32_t base_meshlet = 0;
        uint32_t num_meshlets = 0;

        bool use_own_buffers = false;
    };

    struct create_info_t
    {
        VkViewport viewport = {.x = 0.f, .y = 0.f, .width = 1.f, .height = 1.f, .minDepth = 0.f, .maxDepth = 1.f};
        VkRect2D scissor = {.offset = {0, 0}, .extent = {0, 0}};
        uint32_t num_frames_in_flight = 1;
        VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
        vierkant::PipelineCachePtr pipeline_cache = nullptr;
        bool indirect_draw = true;
        bool enable_mesh_shader = false;
        vierkant::CommandPoolPtr command_pool = nullptr;
        vierkant::DescriptorPoolPtr descriptor_pool = nullptr;
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t random_seed = 0;
    };

    /**
     * @brief   Factory to create drawables from a provided mesh.
     *
     * @param   mesh        a mesh object containing entries with vertex information.
     * @return  an array of drawables for the mesh-entries.
     */
    static std::vector<drawable_t>
    create_drawables(const MeshConstPtr &mesh,
                     const glm::mat4 &model_view = glm::mat4(1),
                     const std::function<bool(const Mesh::entry_t &entry)> &entry_filter = {});

    //! Viewport parameters currently used.
    VkViewport viewport = {.x = 0.f, .y = 0.f, .width = 1.f, .height = 1.f, .minDepth = 0.f, .maxDepth = 1.f};

    //! Scissor parameters currently used.
    VkRect2D scissor = {.offset = {0, 0}, .extent = {0, 0}};

    //! option to disable colors from materials.
    bool disable_material = false;

    //! option to use indirect drawing
    bool indirect_draw = false;

    //! option to use a meshlet-based pipeline
    bool mesh_shader = false;

    //! optional cull-delegate
    indirect_draw_delegate_t draw_indirect_delegate;

    Renderer() = default;

    /**
     * @brief   Construct a new Renderer object
     * @param   device          handle for the vk::Device to create the Renderer
     * @param   create_info     a create_info_t object
     */
    Renderer(DevicePtr device, const create_info_t &create_info);

    Renderer(Renderer &&other) noexcept;

    Renderer(const Renderer &) = delete;

    Renderer &operator=(Renderer other);

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
    VkCommandBuffer render(const vierkant::Framebuffer &framebuffer,
                           bool recycle_commands = false);

    /**
     * @return  the current frame-index.
     */
    [[nodiscard]] uint32_t current_index() const{ return m_current_index; }

    /**
     * @return  the number of concurrent (in-flight) frames.
     */
    [[nodiscard]] uint32_t num_concurrent_frames() const{ return m_frame_assets.size(); }

    /**
     * @return  last measured frame's millisecond-duration
     */
    [[nodiscard]] frame_millisecond_t last_frame_ms() const { return m_frame_assets[m_current_index].frame_time; }

    /**
     * @brief   Release all cached rendering assets.
     */
    void reset();

    [[nodiscard]] const vierkant::DevicePtr &device() const{ return m_device; }

    friend void swap(Renderer &lhs, Renderer &rhs) noexcept;

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
    };

    struct descriptor_set_key_t
    {
        vierkant::MeshConstPtr mesh;
        descriptor_map_t descriptors;

        bool operator==(const descriptor_set_key_t &other) const;
    };

    struct descriptor_set_key_hash_t
    {
        size_t operator()(const descriptor_set_key_t &key) const;
    };

    using descriptor_set_map_t = std::unordered_map<descriptor_set_key_t, vierkant::DescriptorSetPtr, descriptor_set_key_hash_t>;

    struct frame_assets_t
    {
        std::unordered_map<descriptor_map_t, DescriptorSetLayoutPtr> descriptor_set_layouts;
        descriptor_set_map_t descriptor_sets;

        // SSBOs containing everything
        vierkant::BufferPtr matrix_buffer;
        vierkant::BufferPtr matrix_history_buffer;
        vierkant::BufferPtr material_buffer;

        // draw-indirect buffers
        indirect_draw_bundle_t indirect_bundle;
        indirect_draw_bundle_t indirect_indexed_bundle;

        std::vector<drawable_t> drawables;
        vierkant::CommandBuffer command_buffer;

        // used for gpu timestamps
        vierkant::QueryPoolPtr query_pool;
        frame_millisecond_t frame_time;
    };

    void set_function_pointers();

    //! update the combined uniform buffers
    void update_buffers(const std::vector<drawable_t> &drawables, frame_assets_t &frame_asset);

    //! update the combined uniform buffers
    void resize_draw_indirect_buffers(uint32_t num_drawables,
                                      frame_assets_t &frame_asset);

    //! helper routine to find and move assets
    DescriptorSetLayoutPtr find_set_layout(descriptor_map_t descriptors,
                                           frame_assets_t &current,
                                           frame_assets_t &next);

    DescriptorSetPtr find_set(const vierkant::MeshConstPtr &mesh,
                              const DescriptorSetLayoutPtr &set_layout,
                              const descriptor_map_t &descriptors,
                              frame_assets_t &current,
                              frame_assets_t &next,
                              bool variable_count);

    DevicePtr m_device;

    VkSampleCountFlagBits m_sample_count = VK_SAMPLE_COUNT_1_BIT;

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

    //! function pointers for optional mesh-shader support
    PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNV = nullptr;
    PFN_vkCmdDrawMeshTasksIndirectNV vkCmdDrawMeshTasksIndirectNV = nullptr;
    PFN_vkCmdDrawMeshTasksIndirectCountNV vkCmdDrawMeshTasksIndirectCountNV = nullptr;
};

}//namespace vierkant