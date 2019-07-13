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
        viewport = {0.f, 0.f, static_cast<float>(fb.extent().width), static_cast<float>(fb.extent().height), 0.f,
                    static_cast<float>(fb.extent().depth)};
    }
    m_frame_assets.resize(framebuffers.size());

    // command pool
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    // transient command pool -> graphics queue
    auto queue_indices = m_device->queue_family_indices();
    pool_info.queueFamilyIndex = static_cast<uint32_t>(queue_indices[Device::Queue::GRAPHICS].index);
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool command_pool;
    vkCheck(vkCreateCommandPool(m_device->handle(), &pool_info, nullptr, &command_pool),
            "failed to create command pool!");

    m_command_pool = vierkant::CommandPoolPtr(command_pool, [device{m_device}](VkCommandPool pool)
    {
        vkDestroyCommandPool(device->handle(), pool, nullptr);
    });

    // we also need a DescriptorPool ...
    vierkant::descriptor_count_t descriptor_counts = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024},
                                                      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1024}};
    m_descriptor_pool = vierkant::create_descriptor_pool(m_device, descriptor_counts, 512);

    m_pipeline_cache = vierkant::PipelineCache::create(m_device);
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
    std::swap(lhs.m_pipeline_cache, rhs.m_pipeline_cache);
    std::swap(lhs.m_command_pool, rhs.m_command_pool);
    std::swap(lhs.m_descriptor_pool, rhs.m_descriptor_pool);
    std::swap(lhs.m_frame_assets, rhs.m_frame_assets);
    std::swap(lhs.m_current_index, rhs.m_current_index);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::set_current_index(uint32_t image_index)
{
    image_index = crocore::clamp<uint32_t>(image_index, 0, m_frame_assets.size());
    m_current_index = image_index;

    // flush assets
    m_frame_assets[m_current_index] = {};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::stage_drawable(const drawable_t &drawable)
{
    m_frame_assets[m_current_index].drawables.push_back(drawable);
}

void Renderer::render(VkCommandBuffer command_buffer)
{
    std::unordered_map<Pipeline::Format, std::vector<drawable_t>> pipelines;

    for(auto &drawable : m_frame_assets[m_current_index].drawables)
    {
        pipelines[drawable.pipeline_format].push_back(std::move(drawable));
    }

    // grouped by pipelines
    for(auto &[pipe_fmt, drawables] : pipelines)
    {
        // select/create pipeline
        auto pipeline = m_pipeline_cache->get(pipe_fmt);

        // bind pipeline
        pipeline->bind(command_buffer);

        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    }
}

void Renderer::draw(VkCommandBuffer command_buffer, const drawable_t &drawable)
{
    auto fmt = drawable.pipeline_format;
    fmt.renderpass = m_renderpass.get();
    fmt.viewport = viewport;
    fmt.sample_count = m_sample_count;

    // select/create pipeline
    auto pipeline = m_pipeline_cache->get(fmt);

    // bind pipeline
    pipeline->bind(command_buffer);

    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    // bind vertex- and index-buffers
    vierkant::bind_buffers(command_buffer, drawable.mesh);

    // search/create descriptor set
    vierkant::DescriptorSetPtr descriptor_set;
    auto descriptor_it = m_frame_assets[m_current_index].render_assets.find(drawable.mesh);

    if(descriptor_it == m_frame_assets[m_current_index].render_assets.end())
    {
        render_asset_t render_asset = {};

        // create new uniform-buffer for matrices
        auto uniform_buf = vierkant::Buffer::create(m_device, &drawable.matrices, sizeof(matrix_struct_t),
                                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                    VMA_MEMORY_USAGE_CPU_ONLY);

        // update descriptors with ref to newly created buffer
        auto descriptors = drawable.descriptors;
        descriptors[0].buffer = uniform_buf;

        // create a new descriptor set
        descriptor_set = vierkant::create_descriptor_set(m_device, m_descriptor_pool,
                                                         drawable.descriptor_set_layout,
                                                         descriptors);

        // insert all created assets and store in map
        render_asset.uniform_buffer = uniform_buf;
        render_asset.descriptor_set = descriptor_set;
        m_frame_assets[m_current_index].render_assets[drawable.mesh] = render_asset;
    }else
    {
        // use existing set
        descriptor_set = descriptor_it->second.descriptor_set;

        // update data in existing uniform-buffer
        descriptor_it->second.uniform_buffer->set_data(&drawable.matrices, sizeof(matrix_struct_t));
    }

    // bind descriptor sets (uniforms, samplers)
    VkDescriptorSet descriptor_handle = descriptor_set.get();
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout(),
                            0, 1, &descriptor_handle, 0, nullptr);

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
        plane->normals.clear();
        plane->tangents.clear();
        for(auto &v : plane->vertices){ v.xy += glm::vec2(.5f, -.5f); }

        auto mesh = create_mesh_from_geometry(m_device, plane);

        Pipeline::Format fmt = {};
        fmt.blending = true;
        fmt.depth_test = false;
        fmt.depth_write = false;
        fmt.shader_stages = vierkant::shader_stages(m_device, vierkant::ShaderType::UNLIT_TEXTURE);
        fmt.binding_descriptions = vierkant::binding_descriptions(mesh);
        fmt.attribute_descriptions = vierkant::attribute_descriptions(mesh);
        fmt.primitive_topology = mesh->topology;

        // descriptors
        vierkant::descriptor_t desc_ubo = {};
        desc_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_ubo.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        desc_ubo.binding = 0;

        vierkant::descriptor_t desc_texture = {};
        desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        desc_texture.binding = 1;

        drawable_t new_drawable = {};
        new_drawable.mesh = mesh;
        new_drawable.descriptors = {desc_ubo, desc_texture};
        new_drawable.descriptor_set_layout = vierkant::create_descriptor_set_layout(m_device, new_drawable.descriptors);
        fmt.descriptor_set_layouts = {new_drawable.descriptor_set_layout.get()};
        new_drawable.pipeline_format = fmt;

        // insert new drawable in map, update iterator
        draw_it = m_drawable_cache.insert(std::make_pair(DrawableType::IMAGE, std::move(new_drawable))).first;
    }

    glm::vec2 scale = glm::vec2(area.width, area.height) / glm::vec2(viewport.width, viewport.height);

    // copy image-drawable
    auto drawable = draw_it->second;
    drawable.matrices.projection = glm::orthoRH(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    drawable.matrices.projection[1][1] *= -1;
    drawable.matrices.model = glm::scale(glm::mat4(1), glm::vec3(scale, 1));
    drawable.matrices.model[3] = glm::vec4(area.x / viewport.width, -area.y / viewport.height, 0, 1);

    // set image
    drawable.descriptors[1].image_samplers = {image};

    // transition image layout
    image->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, command_buffer);

    // issue generic draw-command
    draw(command_buffer, drawable);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

}//namespace vierkant