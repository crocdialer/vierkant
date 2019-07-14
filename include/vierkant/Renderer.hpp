//
// Created by crocdialer on 03/22/19.
//

#pragma once

#include "crocore/Area.hpp"
#include "vierkant/Mesh.hpp"
#include "vierkant/Framebuffer.hpp"
#include "vierkant/PipelineCache.hpp"
#include "Pipeline.hpp"

namespace vierkant {

class Renderer
{
public:

    struct matrix_struct_t
    {
        glm::mat4 model = glm::mat4(1);
        glm::mat4 view = glm::mat4(1);
        glm::mat4 projection = glm::mat4(1);
        glm::mat4 texture = glm::mat4(1);
    };

    struct drawable_t
    {
        MeshPtr mesh;
        Pipeline::Format pipeline_format = {};
        matrix_struct_t matrices = {};

        // descriptors -> layout
        std::vector<descriptor_t> descriptors;
        DescriptorSetLayoutPtr descriptor_set_layout;
    };

    VkViewport viewport = {.x = 0.f, .y = 0.f, .width = 1.f, .height = 1.f, .minDepth = 0.f, .maxDepth = 1.f};

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

//    void set_current_index(uint32_t image_index);

    void stage_drawable(const drawable_t &drawable);

    void stage_image(const vierkant::ImagePtr &image, const crocore::Area_<float> &area = {});

    void render(VkCommandBuffer command_buffer);

//    void draw(VkCommandBuffer command_buffer, const drawable_t &drawable);

    friend void swap(Renderer &lhs, Renderer &rhs) noexcept;

private:

    enum class DrawableType
    {
        IMAGE
    };

    struct render_asset_t
    {
        vierkant::BufferPtr uniform_buffer;
        vierkant::DescriptorSetPtr descriptor_set;
    };
    using asset_map_t = std::unordered_map<vierkant::MeshPtr, render_asset_t>;

    struct frame_assets_t
    {
        asset_map_t render_assets;
        std::vector<drawable_t> drawables;
    };
    DevicePtr m_device;

    vierkant::RenderPassPtr m_renderpass;

    VkSampleCountFlagBits m_sample_count = VK_SAMPLE_COUNT_1_BIT;

    std::map<vierkant::ShaderType, shader_stage_map_t> m_shader_stage_cache;

    std::unordered_map<DrawableType, drawable_t> m_drawable_cache;

    vierkant::PipelineCachePtr m_pipeline_cache;

    vierkant::CommandPoolPtr m_command_pool;

    vierkant::DescriptorPoolPtr m_descriptor_pool;

    std::vector<std::vector<drawable_t>> m_staged_drawables;
    std::vector<frame_assets_t> m_render_assets;

    std::mutex m_staging_mutex;

    uint32_t m_current_index = 0;
};

}//namespace vierkant