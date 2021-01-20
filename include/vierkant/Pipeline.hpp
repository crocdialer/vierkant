//
// Created by crocdialer on 11/14/18.
//

#pragma once

#include "vierkant/Device.hpp"
#include "vierkant/pipeline_formats.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(Pipeline)

class Pipeline
{
public:

    /**
     * @brief   Construct a new Pipeline object
     *
     * @param   device  handle for the vk::Device to create the Pipeline
     * @param   format  the desired Pipeline::Format
     */
    static PipelinePtr create(DevicePtr device, graphics_pipeline_info_t format);

    Pipeline(Pipeline &&other) noexcept = delete;

    Pipeline(const Pipeline &) = delete;

    ~Pipeline();

    Pipeline &operator=(Pipeline other) = delete;

    /**
     * @brief
     * @param   command_buffer
     */
    void bind(VkCommandBuffer command_buffer);

    /**
     * @return  handle for the managed VkPipeline
     */
    VkPipeline handle() const{ return m_pipeline; }

    /**
     * @return  handle for the managed pipeline-layout
     */
    VkPipelineLayout layout() const{ return m_pipeline_layout; }

private:

    Pipeline(DevicePtr device, graphics_pipeline_info_t format);

    DevicePtr m_device;

    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;

    VkPipelineBindPoint m_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}//namespace vierkant