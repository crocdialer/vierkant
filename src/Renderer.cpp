//
// Created by crocdialer on 3/22/19.
//

#include "vierkant/Renderer.hpp"

namespace vierkant {

///////////////////////////////////////////////////////////////////////////////////////////////////

Renderer::Renderer(DevicePtr device, const vierkant::Framebuffer &framebuffer) :
        m_device(std::move(device)),
        m_renderpass(framebuffer.renderpass())
{
    for(const auto &attach_pair : framebuffer.attachments())
    {
        for(const auto &img : attach_pair.second)
        {
            m_sample_count = std::max(m_sample_count, img->format().sample_count);
        }
    }

//    // we also need a DescriptorPool ...
//    vierkant::descriptor_count_t descriptor_counts;
//    vk::add_descriptor_counts(m_mesh, descriptor_counts);
//    m_descriptor_pool = vierkant::create_descriptor_pool(m_device, descriptor_counts, 3);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Renderer::Renderer(Renderer &&other) noexcept:
        Renderer()
{
    swap(*this, other);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Renderer &Renderer::operator=(Renderer other)
{
    swap(*this, other);
    return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::draw(VkCommandBuffer command_buffer, const drawable_t &drawable)
{
    // select/create pipeline
    auto pipe_it = m_pipelines.find(drawable.pipeline_format);

    if(pipe_it == m_pipelines.end())
    {
        auto fmt = drawable.pipeline_format;

        if(fmt.shader_stages.empty())
        {
            auto shader_type = vierkant::ShaderType::UNLIT_TEXTURE;
            auto stage_it = m_shader_stage_cache.find(shader_type);

            if(stage_it == m_shader_stage_cache.end())
            {
                auto pair = std::make_pair(shader_type, vierkant::shader_stages(m_device, shader_type));
                stage_it = m_shader_stage_cache.insert(pair).first;
            }
            fmt.shader_stages = stage_it->second;
        }
        fmt.binding_descriptions = vierkant::binding_descriptions(drawable.mesh);
        fmt.attribute_descriptions = vierkant::attribute_descriptions(drawable.mesh);
        fmt.primitive_topology = drawable.mesh->topology;

        fmt.renderpass = m_renderpass.get();
        fmt.viewport = viewport;
        fmt.sample_count = m_sample_count;

        fmt.descriptor_set_layouts = {drawable.mesh->descriptor_set_layout.get()};

        // not found -> create pipeline
        auto new_pipeline = Pipeline(m_device, fmt);
        pipe_it = m_pipelines.insert(std::make_pair(drawable.pipeline_format, std::move(new_pipeline))).first;
    }
    auto &pipeline = pipe_it->second;

    // bind pipeline
    pipeline.bind(command_buffer);

    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    vierkant::bind_buffers(command_buffer, drawable.mesh);

    // bind descriptor sets (uniforms, samplers)
    VkDescriptorSet descriptor_set = drawable.descriptor_set.get();
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout(),
                            0, 1, &descriptor_set, 0, nullptr);

    // issue (indexed) drawing command
    if(drawable.mesh->index_buffer){ vkCmdDrawIndexed(command_buffer, drawable.mesh->num_elements, 1, 0, 0, 0); }
    else{ vkCmdDraw(command_buffer, drawable.mesh->num_elements, 1, 0, 0); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void swap(Renderer &lhs, Renderer &rhs)
{
    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_renderpass, rhs.m_renderpass);
    std::swap(lhs.m_sample_count, rhs.m_sample_count);
    std::swap(lhs.m_pipelines, rhs.m_pipelines);
}

}//namespace vierkant