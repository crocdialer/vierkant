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
     * @brief   Create a shared graphics-pipeline
     *
     * @param   device  handle for the vk::Device to create the Pipeline
     * @param   format  a provided graphics_pipeline_info_t
     */
    static PipelinePtr create(DevicePtr device, vierkant::graphics_pipeline_info_t format);

    /**
     * @brief   Create a shared compute-pipeline
     *
     * @param   device  handle for the vk::Device to create the Pipeline
     * @param   format  a provided compute_pipeline_info_t
     */
    static PipelinePtr create(DevicePtr device, vierkant::compute_pipeline_info_t compute_info);

    /**
     * @brief   Create a shared raytracing-pipeline
     *
     * @param   device  handle for the vk::Device to create the Pipeline
     * @param   format  a provided raytracing_pipeline_info_t
     */
    static PipelinePtr create(DevicePtr device, vierkant::raytracing_pipeline_info_t raytracing_info);

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

    Pipeline(DevicePtr device, VkPipelineLayout pipeline_layout, VkPipelineBindPoint bind_point, VkPipeline pipeline);

    DevicePtr m_device;

    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;

    VkPipelineBindPoint m_bind_point = VK_PIPELINE_BIND_POINT_MAX_ENUM;

    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}//namespace vierkant