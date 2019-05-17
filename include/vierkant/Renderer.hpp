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
        glm::mat4 model_view;
        glm::mat4 projection;
    };

    struct drawable_t
    {
        MeshConstPtr mesh;
        Pipeline::Format pipeline_format;
        DescriptorSetPtr descriptor_set;
        glm::mat4 transform;
    };

    VkViewport viewport = {0.f, 0.f, 0.f, 0.f, 0.f, 1.f};

    VkRect2D scissor = {{0, 0},
                        {0, 0}};

    Renderer() = default;

    /**
     * @brief   Construct a new Renderer object
     * @param   device  handle for the vk::Device to create the Renderer
     */
    Renderer(DevicePtr device, const vierkant::Framebuffer &framebuffer);

    Renderer(Renderer &&other) noexcept;

    Renderer(const Renderer &) = delete;

    Renderer &operator=(Renderer other);

    friend void swap(Renderer &lhs, Renderer &rhs);

    void draw(VkCommandBuffer command_buffer, const drawable_t &drawable);

    void draw_image(VkCommandBuffer command_buffer, const vierkant::ImagePtr &image, const crocore::Area_<float> &area);

private:

    enum class DrawableType{ IMAGE };

    DevicePtr m_device;

    vierkant::RenderPassPtr m_renderpass;

    VkSampleCountFlagBits m_sample_count = VK_SAMPLE_COUNT_1_BIT;

    std::map<vierkant::ShaderType, shader_stage_map_t> m_shader_stage_cache;

    std::unordered_map<DrawableType, drawable_t> m_drawable_cache;

    std::unordered_map<Pipeline::Format, Pipeline> m_pipelines;

//    vierkant::DescriptorPoolPtr m_descriptor_pool;
};

}//namespace vierkant