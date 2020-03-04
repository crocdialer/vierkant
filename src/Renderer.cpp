//
// Created by crocdialer on 3/22/19.
//

#include <crocore/Area.hpp>
#include "vierkant/Renderer.hpp"

namespace vierkant
{

///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<Renderer::drawable_t> Renderer::create_drawables(const vierkant::DevicePtr &device,
                                                             const MeshPtr &mesh,
                                                             const std::vector<MaterialPtr> &materials)
{
    std::vector<Renderer::drawable_t> ret;

    for(const auto &entry : mesh->entries)
    {
        // wonky
        auto material = materials[entry.material_index];

        // copy mesh-drawable
        Renderer::drawable_t drawable = {};
        drawable.mesh = mesh;

        // matrices tmp!?
        drawable.matrices.model = mesh->global_transform();

        // material params
        drawable.material.color = material->color;

        drawable.base_index = entry.base_index;
        drawable.num_indices = entry.num_indices;
        drawable.base_vertex = entry.base_vertex;
        drawable.num_vertices = entry.num_vertices;

        drawable.pipeline_format.binding_descriptions = mesh->binding_descriptions();
        drawable.pipeline_format.attribute_descriptions = mesh->attribute_descriptions();
        drawable.pipeline_format.primitive_topology = entry.primitive_type;
        drawable.pipeline_format.shader_stages = vierkant::create_shader_stages(device, material->shader_type);
        drawable.pipeline_format.blend_state.blendEnable = material->blending;
        drawable.pipeline_format.depth_test = true;
        drawable.pipeline_format.depth_write = !material->blending;

        // descriptors
        vierkant::descriptor_t desc_matrices = {};
        desc_matrices.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_matrices.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        desc_matrices.binding = SLOT_MATRIX;
        drawable.descriptors.push_back(desc_matrices);

        vierkant::descriptor_t desc_material = {};
        desc_material.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_material.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        desc_material.binding = SLOT_MATERIAL;
        drawable.descriptors.push_back(desc_material);

        // textures
        if(!material->images.empty())
        {
            vierkant::descriptor_t desc_texture = {};
            desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
            desc_texture.binding = SLOT_TEXTURES;
            desc_texture.image_samplers = material->images;
            drawable.descriptors.push_back(desc_texture);
        }

        uint32_t binding = MIN_NUM_DESCRIPTORS;

        // custom ubos
        for(auto &ubo : material->ubos)
        {
            vierkant::descriptor_t custom_desc = {};
            custom_desc.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            custom_desc.stage_flags = VK_SHADER_STAGE_ALL;
            custom_desc.binding = binding++;
            custom_desc.buffer = vierkant::Buffer::create(device, ubo, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                          VMA_MEMORY_USAGE_CPU_TO_GPU);
            drawable.descriptors.push_back(custom_desc);
        }

        drawable.descriptor_set_layout = vierkant::create_descriptor_set_layout(device, drawable.descriptors);
        drawable.pipeline_format.descriptor_set_layouts = {drawable.descriptor_set_layout.get()};

        ret.push_back(std::move(drawable));
    }
    return ret;
}

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

    m_command_pool = vierkant::create_command_pool(m_device, vierkant::Device::Queue::GRAPHICS,
                                                   VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for(auto &render_asset : m_render_assets)
    {
        render_asset.command_buffer = vierkant::CommandBuffer(m_device, m_command_pool.get(),
                                                              VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    }

    // we also need a DescriptorPool ...
    vierkant::descriptor_count_t descriptor_counts = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024},
                                                      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1024}};
    m_descriptor_pool = vierkant::create_descriptor_pool(m_device, descriptor_counts, 4096);

    if(pipeline_cache){ m_pipeline_cache = std::move(pipeline_cache); }
    else{ m_pipeline_cache = vierkant::PipelineCache::create(m_device); }

    // query physical-device features
    vkGetPhysicalDeviceProperties(m_device->physical_device(), &m_physical_device_properties);
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
    std::swap(lhs.viewport, rhs.viewport);
    std::swap(lhs.scissor, rhs.scissor);
    std::swap(lhs.m_renderpass, rhs.m_renderpass);
    std::swap(lhs.m_sample_count, rhs.m_sample_count);
    std::swap(lhs.m_pipeline_cache, rhs.m_pipeline_cache);
    std::swap(lhs.m_command_pool, rhs.m_command_pool);
    std::swap(lhs.m_descriptor_pool, rhs.m_descriptor_pool);
    std::swap(lhs.m_staged_drawables, rhs.m_staged_drawables);
    std::swap(lhs.m_render_assets, rhs.m_render_assets);
    std::swap(lhs.m_current_index, rhs.m_current_index);
    std::swap(lhs.m_physical_device_properties, rhs.m_physical_device_properties);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::stage_drawable(drawable_t drawable)
{
    auto &fmt = drawable.pipeline_format;
    fmt.renderpass = m_renderpass.get();
    fmt.viewport = viewport;
    fmt.sample_count = m_sample_count;

    VkPushConstantRange push_constant_range = {};
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(push_constants_t);
    push_constant_range.stageFlags = VK_SHADER_STAGE_ALL;
    fmt.push_constant_ranges = {push_constant_range};

    std::lock_guard<std::mutex> lock_guard(m_staging_mutex);
    m_staged_drawables[m_current_index].push_back(std::move(drawable));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

VkCommandBuffer Renderer::render(VkCommandBufferInheritanceInfo *inheritance)
{
    uint32_t current_index;
    {
        std::lock_guard<std::mutex> lock_guard(m_staging_mutex);
        current_index = m_current_index;
        m_current_index = (m_current_index + 1) % m_staged_drawables.size();
        m_render_assets[current_index].drawables = std::move(m_staged_drawables[current_index]);
    }
    auto &current_assets = m_render_assets[current_index];
    frame_assets_t next_assets = {};

    // fetch and start commandbuffer
    next_assets.command_buffer = std::move(current_assets.command_buffer);
    auto &command_buffer = next_assets.command_buffer;
    command_buffer.begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, inheritance);

    // update uniform buffers
    update_uniform_buffers(current_assets.drawables, next_assets);

    // sort by pipelines
    struct indexed_drawable_t
    {
        uint32_t matrix_index = 0;
        uint32_t material_index = 0;
        drawable_t *drawable = nullptr;
    };
    std::unordered_map<Pipeline::Format, std::vector<indexed_drawable_t>> pipelines;

    for(uint32_t i = 0; i < current_assets.drawables.size(); i++)
    {
        pipelines[current_assets.drawables[i].pipeline_format].push_back({i, i, &current_assets.drawables[i]});
    }

    // push constants
    push_constants_t push_constants = {};

    // grouped by pipelines
    for(auto &[pipe_fmt, indexed_drawables] : pipelines)
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

        // keep track of current mesh, fallback instead of iterating sorted-by-mesh to respect order of drawcalls
        vierkant::MeshPtr current_mesh;

        for(auto &indexed_drawable : indexed_drawables)
        {
            auto &drawable = indexed_drawable.drawable;

            if(current_mesh != drawable->mesh)
            {
                current_mesh = drawable->mesh;

                // bind vertex- and index-buffers
                current_mesh->bind_buffers(command_buffer.handle());
            }

            if(dynamic_scissor)
            {
                // set dynamic scissor
                vkCmdSetScissor(command_buffer.handle(), 0, 1, &drawable->pipeline_format.scissor);
            }

            // update/create descriptor set
            auto &descriptors = drawable->descriptors;

            // search/create descriptor set
            asset_key_t key = {current_mesh, drawable->descriptors};
            auto descriptor_it = current_assets.render_assets.find(key);

            vierkant::DescriptorSetPtr descriptor_set;

            // not found or empty queue
            if(descriptor_it == current_assets.render_assets.end() || descriptor_it->second.empty())
            {
                descriptors[SLOT_MATRIX].buffer = next_assets.matrix_buffers[0];
                descriptors[SLOT_MATERIAL].buffer = next_assets.material_buffers[0];

                // transition image layouts
                for(auto &desc : descriptors)
                {
                    for(auto &img : desc.image_samplers)
                    {
                        img->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, command_buffer.handle());
                    }
                }

                // create a new descriptor set
                descriptor_set = vierkant::create_descriptor_set(m_device, m_descriptor_pool,
                                                                 drawable->descriptor_set_layout);

                // update the newly created descriptor set
                vierkant::update_descriptor_set(m_device, descriptor_set, descriptors);

                // insert all created assets and store in map
                next_assets.render_assets[key].push_back(descriptor_set);
            }
            else
            {
                // use existing set
                descriptor_set = std::move(descriptor_it->second.front());
                descriptor_it->second.pop_front();

                // update existing descriptor set
                descriptors[SLOT_MATRIX].buffer = next_assets.matrix_buffers[0];
                descriptors[SLOT_MATERIAL].buffer = next_assets.material_buffers[0];
                vierkant::update_descriptor_set(m_device, descriptor_set, descriptors);

                next_assets.render_assets[key].push_back(descriptor_set);
            }

            // bind descriptor sets (uniforms, samplers)
            VkDescriptorSet descriptor_handle = descriptor_set.get();
            vkCmdBindDescriptorSets(command_buffer.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout(),
                                    0, 1, &descriptor_handle, 0, nullptr);

            // update push_constants for each draw call
            push_constants.matrix_index = indexed_drawable.matrix_index;
            push_constants.material_index = indexed_drawable.material_index;
            vkCmdPushConstants(command_buffer.handle(), pipeline->layout(), VK_SHADER_STAGE_ALL, 0,
                               sizeof(push_constants_t), &push_constants);

            // issue (indexed) drawing command
            if(drawable->mesh->index_buffer)
            {
                vkCmdDrawIndexed(command_buffer.handle(), drawable->num_indices, 1, drawable->base_index,
                                 drawable->base_vertex, 0);
            }
            else{ vkCmdDraw(command_buffer.handle(), drawable->num_vertices, 1, drawable->base_vertex, 0); }
        }
    }

    // keep the stuff in use
    current_assets = std::move(next_assets);

    // end and return commandbuffer
    current_assets.command_buffer.end();
    return current_assets.command_buffer.handle();
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

void Renderer::update_uniform_buffers(const std::vector<drawable_t> &drawables, Renderer::frame_assets_t &frame_asset)
{
    // joined drawable buffers
    std::vector<matrix_struct_t> matrix_data(drawables.size());
    std::vector<material_struct_t> material_data(drawables.size());

    for(uint32_t i = 0; i < drawables.size(); i++)
    {
        matrix_data[i] = drawables[i].matrices;
        material_data[i] = drawables[i].material;
    }

    // define a copy-utility
    auto max_num_bytes = m_physical_device_properties.limits.maxUniformBufferRange;
    auto copy_to_uniform_buffers = [&device = m_device, max_num_bytes](const auto &array,
                                                                       std::vector<vierkant::BufferPtr> &out_buffers)
    {
        if(array.empty()){ return; }

        // tame template nastyness
        using elem_t = typename std::decay<decltype(array)>::type::value_type;
        size_t elem_size = sizeof(elem_t);

        size_t num_bytes = elem_size * array.size();
        size_t num_buffers = 1 + num_bytes / max_num_bytes;

        // grow if necessary
        if(out_buffers.size() < num_buffers){ out_buffers.resize(num_buffers); }

        // init values
        const size_t max_num_elems = max_num_bytes / elem_size;
        size_t num_elems = array.size();
        const elem_t *data_start = array.data();

        for(uint32_t i = 0; i < num_buffers; ++i)
        {
            size_t num_buffer_elems = std::min(num_elems, max_num_elems);
            num_elems -= num_buffer_elems;

            // create/upload joined buffers
            if(!out_buffers[i])
            {
                out_buffers[i] = vierkant::Buffer::create(device, data_start, elem_size * num_buffer_elems,
                                                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                          VMA_MEMORY_USAGE_CPU_TO_GPU);
            }
            else{ out_buffers[i]->set_data(data_start, elem_size * num_buffer_elems); }

            // advance data ptr
            data_start += num_buffer_elems;
        }
    };

    // create/upload joined buffers
    copy_to_uniform_buffers(matrix_data, frame_asset.matrix_buffers);
    copy_to_uniform_buffers(material_data, frame_asset.material_buffers);

//    // create/upload joined buffers
//    if(!frame_asset.matrix_buffers[0])
//    {
//        frame_asset.matrix_buffers[0] = vierkant::Buffer::create(m_device, matrix_data.data(),
//                                                                 sizeof(matrix_struct_t) * matrix_data.size(),
//                                                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//                                                                 VMA_MEMORY_USAGE_CPU_TO_GPU);
//    }
//    else{ frame_asset.matrix_buffers[0]->set_data(matrix_data); }
//
//    if(!frame_asset.material_buffers[0])
//    {
//        frame_asset.material_buffers[0] = vierkant::Buffer::create(m_device, material_data.data(),
//                                                                   sizeof(material_struct_t) * material_data.size(),
//                                                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//                                                                   VMA_MEMORY_USAGE_CPU_TO_GPU);
//    }
//    else{ frame_asset.material_buffers[0]->set_data(material_data); }
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