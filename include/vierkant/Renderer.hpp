//
// Created by crocdialer on 03/22/19.
//

#pragma once

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
        BINDING_MATRIX = 0,
        BINDING_PREVIOUS_MATRIX = 1,
        BINDING_MATERIAL = 2,
        BINDING_TEXTURES = 3,
        BINDING_BONES = 4,
        BINDING_PREVIOUS_BONES = 5,
        BINDING_JITTER_OFFSET = 6,
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

        uint32_t base_texture_index = 0;

        uint32_t texture_type_flags = 0;
    };

//    struct lightsource_t
//    {
//        glm::vec3 position;
//        int type;
//        glm::vec3 color;
//        float intensity;
//        glm::vec3 direction;
//        float radius;
//        float spot_cos_cutoff;
//        float spot_exponent;
//        float quadratic_attenuation;
//        int padding[1];
//    };

    //! define syntax for a culling-delegate
    using indirect_draw_cull_delegate_t = std::function<void(const vierkant::BufferPtr &draws_in,
                                                             vierkant::BufferPtr &draws_out,
                                                             uint32_t num_draws)>;
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
        vierkant::CommandPoolPtr command_pool = nullptr;
        vierkant::DescriptorPoolPtr descriptor_pool = nullptr;
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
                     std::function<bool(const Mesh::entry_t &entry)> entry_filter = {});

    //! Viewport parameters currently used.
    VkViewport viewport = {.x = 0.f, .y = 0.f, .width = 1.f, .height = 1.f, .minDepth = 0.f, .maxDepth = 1.f};

    //! Scissor parameters currently used.
    VkRect2D scissor = {.offset = {0, 0}, .extent = {0, 0}};

    //! option to disable colors from materials.
    bool disable_material = false;

    //! option to use indirect drawing
    bool indirect_draw = true;

    //! optional cull-delegate
    indirect_draw_cull_delegate_t cull_delegate;

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
    VkCommandBuffer render(const vierkant::Framebuffer &framebuffer);

    /**
     * @return  the current swapchain index.
     */
    [[nodiscard]] uint32_t current_index() const{ return m_current_index; }

    /**
     * @return  the number of swapchain indices.
     */
    [[nodiscard]] uint32_t num_indices() const{ return m_render_assets.size(); }

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
        vierkant::BufferPtr indirect_draw_buffer, indirect_culled;
        vierkant::BufferPtr indexed_indirect_draw_buffer, indexed_indirect_culled;

        std::vector<drawable_t> drawables;
        vierkant::CommandBuffer command_buffer;
    };

    //! update the combined uniform buffers
    void update_buffers(const std::vector<drawable_t> &drawables, frame_assets_t &frame_asset);

    //! update the combined uniform buffers
    void resize_draw_indirect_buffers(frame_assets_t &frame_asset, uint32_t num_drawables);

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

    vierkant::CommandPoolPtr m_command_pool;

    vierkant::DescriptorPoolPtr m_descriptor_pool;

    std::vector<std::vector<drawable_t>> m_staged_drawables;

    std::vector<frame_assets_t> m_render_assets;

    std::mutex m_staging_mutex;

    uint32_t m_current_index = 0;

    VkPushConstantRange m_push_constant_range = {};

    std::chrono::steady_clock::time_point m_start_time = std::chrono::steady_clock::now();
};

}//namespace vierkant