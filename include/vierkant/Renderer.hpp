//
// Created by crocdialer on 03/22/19.
//

#pragma once

#include <mutex>
#include "crocore/Area.hpp"

#include "vierkant/Mesh.hpp"
#include "vierkant/Framebuffer.hpp"
#include "vierkant/PipelineCache.hpp"
#include "vierkant/Pipeline.hpp"
#include "vierkant/Camera.hpp"
#include "vierkant/Material.hpp"

namespace vierkant
{

class Renderer
{
public:

    enum DescriptorBinding
    {
        BINDING_MATRIX = 0,
        BINDING_MATERIAL = 1,
        BINDING_TEXTURES = 2,
        BINDING_BONES = 3,
        BINDING_MAX_RANGE
    };

    struct matrix_struct_t
    {
        glm::mat4 modelview = glm::mat4(1);
        glm::mat4 projection = glm::mat4(1);
        glm::mat4 normal = glm::mat4(1);
        glm::mat4 texture = glm::mat4(1);
    };

    struct material_struct_t
    {
        glm::vec4 color = glm::vec4(1);

        glm::vec4 emission = glm::vec4(0);

        float metalness = 0.f;

        float roughness = 1.f;

        float occlusion = 0.f;

        int padding[1];
    };

    struct lightsource_t
    {
        glm::vec3 position;
        int type;
        glm::vec4 diffuse;
        glm::vec4 ambient;
        glm::vec3 direction;
        float intensity;
        float radius;
        float spot_cos_cutoff;
        float spot_exponent;
        float quadratic_attenuation;
    };

    struct push_constants_t
    {
        int matrix_index = 0;
        int material_index = 0;
        glm::vec2 size;
        float gamma;
        float time;
    };

    /**
     * @brief   drawable_t groups all necessary information for a drawable object.
     */
    struct drawable_t
    {
        MeshPtr mesh;

        uint32_t entry_index = 0;

        Pipeline::Format pipeline_format = {};

        matrix_struct_t matrices = {};

        material_struct_t material = {};

        //! a descriptormap
        descriptor_map_t descriptors;

        //! optional descriptor-set-layout
        DescriptorSetLayoutPtr descriptor_set_layout;

        uint32_t base_index = 0;
        uint32_t num_indices = 0;

        uint32_t base_vertex = 0;
        uint32_t num_vertices = 0;

        bool use_own_buffers = false;
    };

    struct create_info_t
    {
        VkViewport viewport = {};
        uint32_t num_frames_in_flight = 0;
        vierkant::RenderPassPtr renderpass;
        VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
        vierkant::PipelineCachePtr pipeline_cache = nullptr;
    };

    /**
     * @brief   Factory to create drawables from a provided mesh.
     *
     * @param   mesh        a mesh object containing entries with vertex information.
     * @return  an array of drawables for the mesh-entries.
     */
    static std::vector<drawable_t> create_drawables(const MeshPtr &mesh);
    
    /**
     * @brief   Viewport parameters currently used.
     */
    VkViewport viewport = {.x = 0.f, .y = 0.f, .width = 1.f, .height = 1.f, .minDepth = 0.f, .maxDepth = 1.f};

    /**
     * @brief   Scissor parameters currently used.
     */
    VkRect2D scissor = {.offset = {0, 0}, .extent = {0, 0}};

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
     * @brief   Stage a drawable object to be rendered.
     *
     * @param   drawable    a drawable_t object.
     */
    void stage_drawable(drawable_t drawable);

    /**
     * @brief   Creates a secondary VkCommandBuffer, that will render all staged drawables.
     *
     * @param   inheritance pointer to a VkCommandBufferInheritanceInfo that contains information about the
     *          current renderpass and framebuffer.
     * @return  handle to the recorded VkCommandBuffer.
     */
    VkCommandBuffer render(const vierkant::Framebuffer &framebuffer);

    /**
     * @return  the current swapchain index.
     */
    uint32_t current_index() const{ return m_current_index; }

    /**
     * @return  the number of swapchain indices.
     */
    uint32_t num_indices() const{ return m_render_assets.size(); }

    /**
     * @brief   Release all cached rendering assets.
     */
    void reset();

    const vierkant::DevicePtr &device() const{ return m_device; }

    friend void swap(Renderer &lhs, Renderer &rhs) noexcept;

private:

    struct render_asset_t
    {
        vierkant::BufferPtr bone_buffer;
        vierkant::DescriptorSetPtr descriptor_set;
    };

    struct asset_key_t
    {
        vierkant::MeshPtr mesh;
        uint32_t matrix_buffer_index = 0;
        uint32_t material_buffer_index = 0;
        descriptor_map_t descriptors;

        bool operator==(const asset_key_t &other) const;
    };

    struct asset_key_hash_t
    {
        size_t operator()(const asset_key_t &key) const;
    };

    using asset_map_t = std::unordered_map<asset_key_t, render_asset_t, asset_key_hash_t>;

    struct frame_assets_t
    {
        std::unordered_map<descriptor_map_t, DescriptorSetLayoutPtr> descriptor_set_layouts;
        asset_map_t render_assets;
        std::vector<vierkant::BufferPtr> matrix_buffers;
        std::vector<vierkant::BufferPtr> material_buffers;
        std::vector<drawable_t> drawables;
        vierkant::CommandBuffer command_buffer;
    };

    //! update the combined uniform buffers
    void update_uniform_buffers(const std::vector<drawable_t> &drawables, frame_assets_t &frame_asset);

    //! create bone data and update uniform buffer
    void update_bone_uniform_buffer(const vierkant::MeshConstPtr &mesh, vierkant::BufferPtr &out_buffer);

    //! helper routine to find and move assets
    DescriptorSetLayoutPtr find_set_layout(descriptor_map_t descriptors,
                                           frame_assets_t &current,
                                           frame_assets_t &next);

    DevicePtr m_device;

    vierkant::RenderPassPtr m_renderpass;

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