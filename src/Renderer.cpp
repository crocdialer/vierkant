//
// Created by crocdialer on 3/22/19.
//

#include <crocore/Area.hpp>
#include "vierkant/Renderer.hpp"

namespace vierkant {

///////////////////////////////////////////////////////////////////////////////////////////////////

Renderer::Renderer(DevicePtr device, const std::vector<vierkant::Framebuffer> &framebuffers,
                   vierkant::PipelineCachePtr pipeline_cache) :
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
    m_staged_drawables.resize(framebuffers.size());
    m_render_assets.resize(framebuffers.size());

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

    for(auto &render_asset : m_render_assets)
    {
        render_asset.command_buffer = vierkant::CommandBuffer(m_device, command_pool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    }

    // we also need a DescriptorPool ...
    vierkant::descriptor_count_t descriptor_counts = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024},
                                                      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1024}};
    m_descriptor_pool = vierkant::create_descriptor_pool(m_device, descriptor_counts, 512);

    if(pipeline_cache){ m_pipeline_cache = std::move(pipeline_cache); }
    else{ m_pipeline_cache = vierkant::PipelineCache::create(m_device); }
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

void swap(Renderer &lhs, Renderer &rhs) noexcept
{
    if(&lhs == &rhs){ return; }
    std::lock(lhs.m_staging_mutex, rhs.m_staging_mutex);
    std::lock_guard<std::mutex> lock_lhs(lhs.m_staging_mutex, std::adopt_lock);
    std::lock_guard<std::mutex> lock_rhs(rhs.m_staging_mutex, std::adopt_lock);

    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_renderpass, rhs.m_renderpass);
    std::swap(lhs.m_sample_count, rhs.m_sample_count);
    std::swap(lhs.m_shader_stage_cache, rhs.m_shader_stage_cache);
    std::swap(lhs.m_pipeline_cache, rhs.m_pipeline_cache);
    std::swap(lhs.m_command_pool, rhs.m_command_pool);
    std::swap(lhs.m_descriptor_pool, rhs.m_descriptor_pool);
    std::swap(lhs.m_staged_drawables, rhs.m_staged_drawables);
    std::swap(lhs.m_render_assets, rhs.m_render_assets);
    std::swap(lhs.m_current_index, rhs.m_current_index);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::stage_drawable(const drawable_t &drawable)
{
    auto drawable_copy = drawable;
    auto &fmt = drawable_copy.pipeline_format;
    fmt.renderpass = m_renderpass.get();
    fmt.viewport = viewport;
    fmt.sample_count = m_sample_count;

    std::lock_guard<std::mutex> lock_guard(m_staging_mutex);
    m_staged_drawables[m_current_index].push_back(std::move(drawable_copy));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

VkCommandBuffer Renderer::render(VkCommandBufferInheritanceInfo *inheritance)
{
    uint32_t last_index;
    {
        std::lock_guard<std::mutex> lock_guard(m_staging_mutex);
        last_index = m_current_index;
        m_current_index = (m_current_index + 1) % m_staged_drawables.size();
        m_render_assets[last_index].drawables = std::move(m_staged_drawables[last_index]);
    }
    auto &last_assets = m_render_assets[last_index];
    frame_assets_t current_assets = {};

    // fetch and start commandbuffer
    current_assets.command_buffer = std::move(last_assets.command_buffer);
    auto &command_buffer = current_assets.command_buffer;
    command_buffer.begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, inheritance);

    // sort by pipelines
    std::unordered_map<Pipeline::Format, std::vector<drawable_t *>> pipelines;
    for(auto &drawable : last_assets.drawables){ pipelines[drawable.pipeline_format].push_back(&drawable); }

    // grouped by pipelines
    for(auto &[pipe_fmt, drawables] : pipelines)
    {
        // select/create pipeline
        auto pipeline = m_pipeline_cache->get(pipe_fmt);

        // bind pipeline
        pipeline->bind(command_buffer.handle());

        bool dynamic_scissor = crocore::contains(pipe_fmt.dynamic_states, VK_DYNAMIC_STATE_SCISSOR);

        if(crocore::contains(pipe_fmt.dynamic_states, VK_DYNAMIC_STATE_VIEWPORT))
        {
            // set dynamic viewport
            vkCmdSetViewport(command_buffer.handle(), 0, 1, &viewport);
        }

        // sort by meshes
        std::unordered_map<vierkant::MeshPtr, std::vector<drawable_t *>> meshes;
        for(auto &d : drawables){ meshes[d->mesh].push_back(d); }

        // grouped by meshes
        for(auto &[mesh, drawables] : meshes)
        {
            // bind vertex- and index-buffers
            vierkant::bind_buffers(command_buffer.handle(), mesh);

            for(auto drawable : drawables)
            {
                if(dynamic_scissor)
                {
                    // set dynamic scissor
                    vkCmdSetScissor(command_buffer.handle(), 0, 1, &drawable->pipeline_format.scissor);
                }

                // update/create descriptor set
                auto &descriptors = drawable->descriptors;

                // search/create descriptor set
                asset_key_t key = {mesh, drawable->descriptors};
                auto descriptor_it = last_assets.render_assets.find(key);

                vierkant::DescriptorSetPtr descriptor_set;

                // not found or empty queue
                if(descriptor_it == last_assets.render_assets.end() || descriptor_it->second.empty())
                {
                    render_asset_t render_asset = {};

                    // create new uniform-buffer for matrices
                    auto uniform_buf = vierkant::Buffer::create(m_device, &drawable->matrices, sizeof(matrix_struct_t),
                                                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                VMA_MEMORY_USAGE_CPU_TO_GPU);

                    // transition image layouts
                    for(auto &desc : descriptors)
                    {
                        for(auto &img : desc.image_samplers)
                        {
                            img->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, command_buffer.handle());
                        }
                    }
                    descriptors[SLOT_MATRIX].buffer = uniform_buf;

                    // create a new descriptor set
                    descriptor_set = vierkant::create_descriptor_set(m_device, m_descriptor_pool,
                                                                     drawable->descriptor_set_layout);

                    // update the newly created descriptor set
                    vierkant::update_descriptor_set(m_device, descriptor_set, descriptors);

                    // insert all created assets and store in map
                    render_asset.uniform_buffer = uniform_buf;
                    render_asset.descriptor_set = descriptor_set;
                    current_assets.render_assets[key].push_back(std::move(render_asset));
                }else
                {
                    auto render_asset = std::move(descriptor_it->second.front());
                    descriptor_it->second.pop_front();

                    // use existing set
                    descriptor_set = render_asset.descriptor_set;

                    // update data in existing uniform-buffer
                    render_asset.uniform_buffer->set_data(&drawable->matrices, sizeof(matrix_struct_t));

                    // update existing descriptor set
                    descriptors[0].buffer = render_asset.uniform_buffer;
                    vierkant::update_descriptor_set(m_device, descriptor_set, descriptors);

                    current_assets.render_assets[key].push_back(std::move(render_asset));
                }

                // bind descriptor sets (uniforms, samplers)
                VkDescriptorSet descriptor_handle = descriptor_set.get();
                vkCmdBindDescriptorSets(command_buffer.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout(),
                                        0, 1, &descriptor_handle, 0, nullptr);

                // issue (indexed) drawing command
                if(drawable->mesh->index_buffer)
                {
                    vkCmdDrawIndexed(command_buffer.handle(), drawable->num_indices, 1, drawable->base_index, 0, 0);
                }else{ vkCmdDraw(command_buffer.handle(), drawable->num_indices, 1, drawable->base_index, 0); }
            }
        }
    }

    // keep the stuff in use
    last_assets = std::move(current_assets);

    // end and return commandbuffer
    last_assets.command_buffer.end();
    return last_assets.command_buffer.handle();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::stage_image(const vierkant::ImagePtr &image, const crocore::Area_<int> &area)
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
        new_drawable.num_indices = mesh->num_elements;
        new_drawable.descriptors = {desc_ubo, desc_texture};
        new_drawable.descriptor_set_layout = vierkant::create_descriptor_set_layout(m_device, new_drawable.descriptors);
        fmt.descriptor_set_layouts = {new_drawable.descriptor_set_layout.get()};
        new_drawable.pipeline_format = fmt;

        // insert new drawable in map, update iterator
        draw_it = m_drawable_cache.insert(std::make_pair(DrawableType::IMAGE, std::move(new_drawable))).first;
    }
    float w = area.width ? area.width : viewport.width;
    float h = area.height ? area.height : viewport.height;
    glm::vec2 scale = glm::vec2(w, h) / glm::vec2(viewport.width, viewport.height);

    // copy image-drawable
    auto drawable = draw_it->second;
    drawable.matrices.projection = glm::orthoRH(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    drawable.matrices.projection[1][1] *= -1;
    drawable.matrices.model = glm::scale(glm::mat4(1), glm::vec3(scale, 1));
    drawable.matrices.model[3] = glm::vec4(area.x / viewport.width, -area.y / viewport.height, 0, 1);

    // set image
    drawable.descriptors[SLOT_TEXTURES].image_samplers = {image};

    // stage image drawable
    stage_drawable(drawable);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::reset()
{
    std::lock_guard<std::mutex> lock_guard(m_staging_mutex);
    m_current_index = 0;
    m_staged_drawables.clear();
    for(auto &frame_asset : m_render_assets){ frame_asset = {}; }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Renderer::asset_key_t::operator==(const Renderer::asset_key_t &other) const
{
    if(mesh != other.mesh){ return false; }
    if(descriptors != other.descriptors){ return false; }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

size_t Renderer::asset_key_hash_t::operator()(const Renderer::asset_key_t &key) const
{
    size_t h = 0;
    crocore::hash_combine(h, key.mesh);
    for(const auto &descriptor : key.descriptors){ crocore::hash_combine(h, descriptor); }
    return h;
}

///////////////////////////////////////////////////////////////////////////////////////////////////


}//namespace vierkant