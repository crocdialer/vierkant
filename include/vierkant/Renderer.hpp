//
// Created by crocdialer on 03/22/19.
//

#pragma once

#include <unordered_map>
#include "crocore/Area.hpp"
#include "vierkant/Mesh.hpp"
#include "vierkant/Framebuffer.hpp"
#include "vierkant/Pipeline.hpp"

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
    };

    VkViewport viewport = {0.f, 0.f, 1.f, 1.f, 0.f, 1.f};

    VkRect2D scissor = {{0, 0},
                        {0, 0}};

    Renderer() = default;

    /**
     * @brief   Construct a new Renderer object
     * @param   device  handle for the vk::Device to create the Renderer
     */
    Renderer(DevicePtr device, const std::vector<vierkant::Framebuffer> &framebuffers);

    Renderer(Renderer &&other) noexcept;

    Renderer(const Renderer &) = delete;

    Renderer &operator=(Renderer other);

    friend void swap(Renderer &lhs, Renderer &rhs);

    void set_current_index(uint32_t i);

    void draw(VkCommandBuffer command_buffer, const drawable_t &drawable);

    void draw_image(VkCommandBuffer command_buffer, const vierkant::ImagePtr &image,
                    const crocore::Area_<float> &area = {});

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

    DevicePtr m_device;

    vierkant::RenderPassPtr m_renderpass;

    VkSampleCountFlagBits m_sample_count = VK_SAMPLE_COUNT_1_BIT;

    std::map<vierkant::ShaderType, shader_stage_map_t> m_shader_stage_cache;

    std::unordered_map<DrawableType, drawable_t> m_drawable_cache;

    std::unordered_map<Pipeline::Format, Pipeline> m_pipelines;

    vierkant::DescriptorPoolPtr m_descriptor_pool;

    std::vector<asset_map_t> m_frame_assets;

    uint32_t m_current_index = 0;
};

}//namespace vierkant