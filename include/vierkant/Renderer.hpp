//
// Created by crocdialer on 03/22/19.
//

#pragma once

#include <deque>
#include "crocore/Area.hpp"
#include "vierkant/Mesh.hpp"
#include "vierkant/Framebuffer.hpp"
#include "vierkant/PipelineCache.hpp"
#include "vierkant/Pipeline.hpp"
#include "vierkant/Camera.hpp"
#include "vierkant/Material.hpp"

namespace vierkant {

class Renderer
{
public:

    enum DescriptorSlot
    {
        SLOT_MATRIX = 0, SLOT_TEXTURES, MIN_NUM_DESCRIPTORS
    };

    struct matrix_struct_t
    {
        glm::mat4 model = glm::mat4(1);
        glm::mat4 view = glm::mat4(1);
        glm::mat4 projection = glm::mat4(1);
        glm::mat4 texture = glm::mat4(1);
    };

    /**
     * @brief   drawable_t groups all necessary information for a drawable object.
     */
    struct drawable_t
    {
        MeshPtr mesh;
        Pipeline::Format pipeline_format = {};
        matrix_struct_t matrices = {};

        // descriptors -> layout
        std::vector<descriptor_t> descriptors;
        DescriptorSetLayoutPtr descriptor_set_layout;

        uint32_t base_index = 0;
        uint32_t num_indices = 0;

        uint32_t base_vertex = 0;
        uint32_t num_vertices = 0;
    };

    /**
     * @brief   Factory to create a drawable_t from provided mesh and material.
     *
     * @param   device      handle for the vk::Device to create any vulkan assets with.
     * @param   mesh        a mesh object containing vertex information
     * @param   material    a material object
     * @return  a newly constructed drawable_t
     */
    static drawable_t create_drawable(const vierkant::DevicePtr &device, const MeshPtr &mesh,
                                      const MaterialPtr &material);

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
     * @param   device  handle for the vk::Device to create the Renderer
     */
    Renderer(DevicePtr device, const std::vector<vierkant::Framebuffer> &framebuffers,
             vierkant::PipelineCachePtr pipeline_cache = nullptr);

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
    VkCommandBuffer render(VkCommandBufferInheritanceInfo *inheritance);

    /**
     * @return  the current swapchain index.
     */
    uint32_t current_index() const { return m_current_index; }

    /**
     * @return  the number of swapchain indices.
     */
    uint32_t num_indices() const { return m_render_assets.size(); }

    /**
     * @brief   Release all cached rendering assets.
     */
    void reset();

    vierkant::DevicePtr device() const { return m_device; }

    friend void swap(Renderer &lhs, Renderer &rhs) noexcept;

private:

    struct render_asset_t
    {
        vierkant::BufferPtr uniform_buffer;
        vierkant::DescriptorSetPtr descriptor_set;
    };

    struct asset_key_t
    {
        vierkant::MeshPtr mesh;
        std::vector<vierkant::descriptor_t> descriptors;

        bool operator==(const asset_key_t &other) const;
    };

    struct asset_key_hash_t
    {
        size_t operator()(const asset_key_t &key) const;
    };

    using asset_map_t = std::unordered_map<asset_key_t, std::deque<render_asset_t>, asset_key_hash_t>;

    struct frame_assets_t
    {
        asset_map_t render_assets;
        std::vector<drawable_t> drawables;
        vierkant::CommandBuffer command_buffer;
    };
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
};

}//namespace vierkant