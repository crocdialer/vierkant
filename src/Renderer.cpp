//
// Created by crocdialer on 3/22/19.
//

#include <crocore/Area.hpp>
#include "vierkant/Renderer.hpp"

namespace vierkant {

///////////////////////////////////////////////////////////////////////////////////////////////////

Renderer::Renderer(DevicePtr device, const std::vector<vierkant::Framebuffer> &framebuffers) :
        m_device(std::move(device))
{
    for(auto &fb : framebuffers)
    {
        for(const auto &attach_pair : fb.attachments())
        {
            for(const auto &img : attach_pair.second)
            {
                m_sample_count = std::max(m_sample_count, img->format().sample_count);
            }
        }
        m_renderpass = fb.renderpass();
    }
    m_frame_assets.resize(framebuffers.size());

    // we also need a DescriptorPool ...
    vierkant::descriptor_count_t descriptor_counts = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024},
                                                      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1024}};
    m_descriptor_pool = vierkant::create_descriptor_pool(m_device, descriptor_counts, 128);
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

void swap(Renderer &lhs, Renderer &rhs)
{
    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_renderpass, rhs.m_renderpass);
    std::swap(lhs.m_sample_count, rhs.m_sample_count);
    std::swap(lhs.m_shader_stage_cache, rhs.m_shader_stage_cache);
    std::swap(lhs.m_pipelines, rhs.m_pipelines);
    std::swap(lhs.m_descriptor_pool, rhs.m_descriptor_pool);
    std::swap(lhs.m_frame_assets, rhs.m_frame_assets);
    std::swap(lhs.m_current_index, rhs.m_current_index);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::set_current_index(uint32_t i)
{
    i = crocore::clamp<uint32_t>(i, 0, m_frame_assets.size());
    m_current_index = i;

    // flush descriptor sets
    m_frame_assets[m_current_index].clear();
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

    // search/create descriptor set
    vierkant::DescriptorSetPtr descriptor_set;
    auto asset_it = m_frame_assets[m_current_index].find(drawable.mesh);

    if(asset_it == m_frame_assets[m_current_index].end())
    {
        render_asset_t render_asset = {};
        descriptor_set = vierkant::create_descriptor_set(m_device, m_descriptor_pool, drawable.mesh);
        render_asset.descriptor_set = descriptor_set;
        m_frame_assets[m_current_index][drawable.mesh] = render_asset;
    }else{ descriptor_set = asset_it->second.descriptor_set; }

    // bind descriptor sets (uniforms, samplers)
    VkDescriptorSet descriptor_handle = descriptor_set.get();
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout(),
                            0, 1, &descriptor_handle, 0, nullptr);

    // push-constants
//    vkCmdPushConstants(command_buffer, pipeline.layout());

    // issue (indexed) drawing command
    if(drawable.mesh->index_buffer){ vkCmdDrawIndexed(command_buffer, drawable.mesh->num_elements, 1, 0, 0, 0); }
    else{ vkCmdDraw(command_buffer, drawable.mesh->num_elements, 1, 0, 0); }
}

void Renderer::draw_image(VkCommandBuffer command_buffer, const vierkant::ImagePtr &image,
                          const crocore::Area_<float> &area)
{
    auto draw_it = m_drawable_cache.find(DrawableType::IMAGE);

    // need to create drawable
    if(draw_it == m_drawable_cache.end())
    {
        // create plane-geometry
        auto plane = Geometry::Plane();
        for(auto &v : plane.vertices){ v.xy += glm::vec2(.5f, -.5f); }

        auto mesh = create_mesh_from_geometry(m_device, plane);

        Pipeline::Format fmt = {};
        fmt.blending = true;
        fmt.depth_test = false;
        fmt.depth_write = false;
        fmt.binding_descriptions = vierkant::binding_descriptions(mesh);
        fmt.attribute_descriptions = vierkant::attribute_descriptions(mesh);
        fmt.primitive_topology = mesh->topology;

        auto uniform_buf = vierkant::Buffer::create(m_device, nullptr, sizeof(matrix_struct_t),
                                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                    VMA_MEMORY_USAGE_CPU_ONLY);
        // descriptors
        vierkant::Mesh::Descriptor desc_ubo;
        desc_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_ubo.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        desc_ubo.binding = 0;
        desc_ubo.buffer = uniform_buf;

        vierkant::Mesh::Descriptor desc_texture;
        desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        desc_texture.binding = 1;
        desc_texture.image_samplers = {image};

        mesh->descriptors = {desc_ubo, desc_texture};
        mesh->descriptor_set_layout = vierkant::create_descriptor_set_layout(m_device, mesh);
        drawable_t new_drawable = {mesh, fmt};

        // insert new drawable in map, update iterator
        draw_it = m_drawable_cache.insert(std::make_pair(DrawableType::IMAGE, std::move(new_drawable))).first;
    }

    glm::vec2 scale = glm::vec2(area.width, area.height) / glm::vec2(viewport.width, viewport.height);

    matrix_struct_t matrix_ubo;
    matrix_ubo.projection = glm::orthoRH(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    matrix_ubo.projection[1][1] *= -1;
    matrix_ubo.model = glm::scale(glm::mat4(1), glm::vec3(scale, 1));
    matrix_ubo.model[3] = glm::vec4(area.x / viewport.width, -area.y / viewport.height, 0, 1);

    // update image-drawable
    auto &drawable = draw_it->second;
    drawable.mesh->descriptors[0].buffer->set_data(&matrix_ubo, sizeof(matrix_struct_t));

    if(drawable.mesh->descriptors[1].image_samplers[0] != image)
    {
        drawable.mesh->descriptors[1].image_samplers[0] = image;
    }

    // transition image layout
    image->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, command_buffer);

    // issue generic draw-command
    draw(command_buffer, drawable);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

}//namespace vierkant