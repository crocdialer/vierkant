//
// Created by crocdialer on 3/22/19.
//

#include <crocore/Area.hpp>
#include <vierkant/Pipeline.hpp>
#include "vierkant/Renderer.hpp"

namespace vierkant
{

using std::chrono::steady_clock;
using std::chrono::duration_cast;
using duration_t = std::chrono::duration<float>;

///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<Renderer::drawable_t> Renderer::create_drawables(const MeshConstPtr &mesh, const glm::mat4 &model_view)
{
    if(!mesh){ return {}; }

    // reserve space for one drawable per mesh-entry
    std::vector<Renderer::drawable_t> ret;
    ret.reserve(mesh->entries.size());

    // same for all entries
    auto binding_descriptions = mesh->binding_descriptions();
    auto attribute_descriptions = mesh->attribute_descriptions();

    for(uint32_t i = 0; i < mesh->entries.size(); ++i)
    {
        const auto &entry = mesh->entries[i];

        // skip disabled entries
        if(!entry.enabled){ continue; }

        // wonky
        const auto &material = mesh->materials[entry.material_index];

        // aquire ref for mesh-drawable
        Renderer::drawable_t drawable = {};
        drawable.mesh = mesh;
        drawable.entry_index = i;

        // combine mesh- with entry-transform
        drawable.matrices.modelview = model_view * entry.transform;
        drawable.matrices.normal = glm::inverseTranspose(drawable.matrices.modelview);

        // material params
        drawable.material.color = material->color;
        drawable.material.emission = material->emission;
        drawable.material.ambient = material->ambient;
        drawable.material.roughness = material->roughness;
        drawable.material.metalness = material->metalness;
        drawable.material.blend_mode = static_cast<uint32_t>(material->blend_mode);
        drawable.material.alpha_cutoff = material->alpha_cutoff;

        drawable.base_index = entry.base_index;
        drawable.num_indices = entry.num_indices;
        drawable.base_vertex = entry.base_vertex;
        drawable.num_vertices = entry.num_vertices;

        drawable.pipeline_format.binding_descriptions = binding_descriptions;
        drawable.pipeline_format.attribute_descriptions = attribute_descriptions;
        drawable.pipeline_format.primitive_topology = entry.primitive_type;
        drawable.pipeline_format.blend_state.blendEnable = material->blend_mode == vierkant::Material::BlendMode::Blend;
        drawable.pipeline_format.depth_test = material->depth_test;
        drawable.pipeline_format.depth_write = material->depth_write;
        drawable.pipeline_format.cull_mode = material->cull_mode;

        // descriptors
        vierkant::descriptor_t desc_matrices = {};
        desc_matrices.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_matrices.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        drawable.descriptors[BINDING_MATRIX] = desc_matrices;

        vierkant::descriptor_t desc_material = {};
        desc_material.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_material.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        drawable.descriptors[BINDING_MATERIAL] = desc_material;

        // textures
        if(!material->textures.empty())
        {
            vierkant::descriptor_t desc_texture = {};
            desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
            for(auto &p : material->textures){ desc_texture.image_samplers.push_back(p.second); };
            drawable.descriptors[BINDING_TEXTURES] = desc_texture;
        }

        // bone matrices
        if(mesh->root_bone)
        {
            vierkant::descriptor_t desc_bones = {};
            desc_bones.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            desc_bones.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
            drawable.descriptors[BINDING_BONES] = desc_bones;
        }

        // push drawable to vector
        ret.push_back(std::move(drawable));
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Renderer::Renderer(DevicePtr device, const create_info_t &create_info) :
        m_device(std::move(device))
{
    if(!create_info.num_frames_in_flight)
    {
        throw std::runtime_error("could not create vierkant::Renderer");
    }

    m_sample_count = create_info.sample_count;
    viewport = create_info.viewport;

    m_staged_drawables.resize(create_info.num_frames_in_flight);
    m_render_assets.resize(create_info.num_frames_in_flight);

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

    if(create_info.pipeline_cache){ m_pipeline_cache = create_info.pipeline_cache; }
    else{ m_pipeline_cache = vierkant::PipelineCache::create(m_device); }

    // push constant range
    m_push_constant_range.offset = 0;
    m_push_constant_range.size = sizeof(push_constants_t);
    m_push_constant_range.stageFlags = VK_SHADER_STAGE_ALL;
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

    std::swap(lhs.viewport, rhs.viewport);
    std::swap(lhs.scissor, rhs.scissor);
    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_sample_count, rhs.m_sample_count);
    std::swap(lhs.m_pipeline_cache, rhs.m_pipeline_cache);
    std::swap(lhs.m_command_pool, rhs.m_command_pool);
    std::swap(lhs.m_descriptor_pool, rhs.m_descriptor_pool);
    std::swap(lhs.m_staged_drawables, rhs.m_staged_drawables);
    std::swap(lhs.m_render_assets, rhs.m_render_assets);
    std::swap(lhs.m_current_index, rhs.m_current_index);
    std::swap(lhs.m_push_constant_range, rhs.m_push_constant_range);
    std::swap(lhs.m_start_time, rhs.m_start_time);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::stage_drawable(drawable_t drawable)
{
    std::lock_guard<std::mutex> lock_guard(m_staging_mutex);
    m_staged_drawables[m_current_index].push_back(std::move(drawable));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

VkCommandBuffer Renderer::render(const vierkant::Framebuffer &framebuffer)
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

    // keep uniform buffers
    next_assets.matrix_buffers = std::move(current_assets.matrix_buffers);
    next_assets.material_buffers = std::move(current_assets.material_buffers);

    VkCommandBufferInheritanceInfo inheritance = {};
    inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritance.framebuffer = framebuffer.handle();
    inheritance.renderPass = framebuffer.renderpass().get();

    // fetch and start commandbuffer
    next_assets.command_buffer = std::move(current_assets.command_buffer);
    auto &command_buffer = next_assets.command_buffer;
    command_buffer.begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, &inheritance);

    // update uniform buffers
    update_uniform_buffers(current_assets.drawables, next_assets);

    // sort by pipelines
    struct indexed_drawable_t
    {
        uint32_t matrix_index = 0;
        uint32_t material_index = 0;
        uint32_t matrix_buffer_index = 0;
        uint32_t material_buffer_index = 0;
        vierkant::DescriptorSetLayoutPtr descriptor_set_layout = nullptr;
        drawable_t *drawable = nullptr;
    };
    std::unordered_map<graphics_pipeline_info_t, std::vector<indexed_drawable_t>> pipelines;
    size_t max_num_uniform_bytes = m_device->properties().limits.maxUniformBufferRange;

    // preprocess drawables
    for(uint32_t i = 0; i < current_assets.drawables.size(); i++)
    {
        auto &pipeline_format = current_assets.drawables[i].pipeline_format;
        pipeline_format.renderpass = framebuffer.renderpass().get();
        pipeline_format.viewport = viewport;
        pipeline_format.sample_count = m_sample_count;
        pipeline_format.push_constant_ranges = {m_push_constant_range};

        indexed_drawable_t indexed_drawable = {};
        indexed_drawable.matrix_buffer_index = i * sizeof(matrix_struct_t) / max_num_uniform_bytes;
        indexed_drawable.material_buffer_index = i * sizeof(material_struct_t) / max_num_uniform_bytes;

        indexed_drawable.matrix_index = i % (max_num_uniform_bytes / sizeof(matrix_struct_t));
        indexed_drawable.material_index = i % (max_num_uniform_bytes / sizeof(material_struct_t));

        indexed_drawable.drawable = &current_assets.drawables[i];

        if(!current_assets.drawables[i].descriptor_set_layout)
        {
            indexed_drawable.descriptor_set_layout = find_set_layout(current_assets.drawables[i].descriptors,
                                                                     current_assets, next_assets);
            pipeline_format.descriptor_set_layouts = {indexed_drawable.descriptor_set_layout.get()};
        }
        else{ indexed_drawable.descriptor_set_layout = std::move(current_assets.drawables[i].descriptor_set_layout); }

        pipelines[pipeline_format].push_back(indexed_drawable);
    }

    // push constants
    push_constants_t push_constants = {};
    push_constants.size = {viewport.width, viewport.height};
    push_constants.time = duration_cast<duration_t>(steady_clock::now() - m_start_time).count();
    push_constants.disable_material = disable_material;

    // grouped by pipelines
    for(auto &[pipe_fmt, indexed_drawables] : pipelines)
    {
        // select/create pipeline
        auto pipeline = m_pipeline_cache->pipeline(pipe_fmt);

        // bind pipeline
        pipeline->bind(command_buffer.handle());

        bool dynamic_scissor = crocore::contains(pipe_fmt.dynamic_states, VK_DYNAMIC_STATE_SCISSOR);

        if(crocore::contains(pipe_fmt.dynamic_states, VK_DYNAMIC_STATE_VIEWPORT))
        {
            // set dynamic viewport
            vkCmdSetViewport(command_buffer.handle(), 0, 1, &viewport);
        }

        // keep track of current mesh, fallback instead of iterating sorted-by-mesh to respect order of drawcalls
        vierkant::MeshConstPtr current_mesh;

        for(auto &indexed_drawable : indexed_drawables)
        {
            auto &drawable = indexed_drawable.drawable;

            if(drawable->mesh && current_mesh != drawable->mesh)
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

            if(!drawable->use_own_buffers)
            {
                descriptors[BINDING_MATRIX].buffers = {
                        next_assets.matrix_buffers[indexed_drawable.matrix_buffer_index]};
                descriptors[BINDING_MATERIAL].buffers = {
                        next_assets.material_buffers[indexed_drawable.material_buffer_index]};
            }

            // search/create descriptor set
            asset_key_t key = {};
            key.mesh = current_mesh;
            key.descriptors = drawable->descriptors;
            key.matrix_buffer_index = indexed_drawable.matrix_buffer_index;
            key.material_buffer_index = indexed_drawable.material_buffer_index;

            // start searching in
            auto render_asset_it = next_assets.render_assets.find(key);

            // transition image layouts
            for(auto &pair : descriptors)
            {
                auto &desc = pair.second;
                for(auto &img : desc.image_samplers)
                {
                    img->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                           command_buffer.handle());
                }
            }

            // handle for a descriptor-set
            VkDescriptorSet descriptor_set_handle = VK_NULL_HANDLE;

            // not found in next assets
            if(render_asset_it == next_assets.render_assets.end())
            {
                // search in current assets (might already been processed for this frame)
                auto current_assets_it = current_assets.render_assets.find(key);

                // not found in current assets
                if(current_assets_it == current_assets.render_assets.end())
                {
                    // create a new render_asset
                    render_asset_t new_render_asset = {};

                    // create a new descriptor set
                    new_render_asset.descriptor_set = vierkant::create_descriptor_set(m_device, m_descriptor_pool,
                                                                                      indexed_drawable.descriptor_set_layout);

                    // update bone buffers, if necessary
                    if(!drawable->use_own_buffers && current_mesh && current_mesh->root_bone)
                    {
                        update_bone_uniform_buffer(current_mesh, new_render_asset.bone_buffer);
                        descriptors[BINDING_BONES].buffers = {new_render_asset.bone_buffer};
                    }

                    // keep handle
                    descriptor_set_handle = new_render_asset.descriptor_set.get();

                    // update the newly created descriptor set
                    vierkant::update_descriptor_set(m_device, new_render_asset.descriptor_set, descriptors);

                    // insert all created assets and store in map
                    next_assets.render_assets[key] = std::move(new_render_asset);
                }
                else
                {

                    // use existing render_asset
                    render_asset_t render_asset = std::move(current_assets_it->second);
                    current_assets.render_assets.erase(current_assets_it);

                    // update bone buffers, if necessary
                    if(!drawable->use_own_buffers && current_mesh->root_bone)
                    {
                        update_bone_uniform_buffer(current_mesh, render_asset.bone_buffer);
                        descriptors[BINDING_BONES].buffers = {render_asset.bone_buffer};
                    }

                    // keep handle
                    descriptor_set_handle = render_asset.descriptor_set.get();

                    // update existing descriptor set
                    vierkant::update_descriptor_set(m_device, render_asset.descriptor_set, descriptors);

                    next_assets.render_assets[key] = std::move(render_asset);
                }

            }
            else
            {
                descriptor_set_handle = render_asset_it->second.descriptor_set.get();
            }

            // bind descriptor sets (uniforms, samplers)
            vkCmdBindDescriptorSets(command_buffer.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout(),
                                    0, 1, &descriptor_set_handle, 0, nullptr);

            // update push_constants for each draw call
            push_constants.matrix_index = indexed_drawable.matrix_index;
            push_constants.material_index = indexed_drawable.material_index;
            push_constants.clipping = clipping_distances(indexed_drawable.drawable->matrices.projection);
            vkCmdPushConstants(command_buffer.handle(), pipeline->layout(), VK_SHADER_STAGE_ALL, 0,
                               sizeof(push_constants_t), &push_constants);

            // issue (indexed) drawing command
            if(drawable->mesh && drawable->mesh->index_buffer)
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

//    // calculate required alignment based on minimum device offset alignment
//    size_t minUboAlignment = m_device->properties().limits.minUniformBufferOffsetAlignment;
//    size_t dynamic_alignment = sizeof(glm::mat4);
//
//    if(minUboAlignment > 0)
//    {
//        dynamic_alignment = (dynamic_alignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
//    }

    // define a copy-utility
    auto max_num_bytes = m_device->properties().limits.maxUniformBufferRange;
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
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::update_bone_uniform_buffer(const vierkant::MeshConstPtr &mesh, vierkant::BufferPtr &out_buffer)
{
    if(mesh && mesh->root_bone && mesh->animation_index < mesh->node_animations.size())
    {
        std::vector<glm::mat4> bones_matrices;
        vierkant::nodes::build_node_matrices(mesh->root_bone, mesh->node_animations[mesh->animation_index],
                                             bones_matrices);

        if(!out_buffer)
        {
            out_buffer = vierkant::Buffer::create(m_device, bones_matrices.data(),
                                                  bones_matrices.size() * sizeof(glm::mat4),
                                                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                  VMA_MEMORY_USAGE_CPU_TO_GPU);
        }
        else{ out_buffer->set_data(bones_matrices); }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorSetLayoutPtr Renderer::find_set_layout(descriptor_map_t descriptors,
                                                 frame_assets_t &current,
                                                 frame_assets_t &next)
{
    // clean descriptor-map to enable sharing
    for(auto &[binding, descriptor] : descriptors)
    {
        for(auto &img : descriptor.image_samplers){ img.reset(); };
        for(auto &buf : descriptor.buffers){ buf.reset(); };
    }

    // retrieve set-layout
    auto set_it = current.descriptor_set_layouts.find(descriptors);

    if(set_it != current.descriptor_set_layouts.end())
    {
        auto new_it = next.descriptor_set_layouts.insert(std::move(*set_it)).first;
        current.descriptor_set_layouts.erase(set_it);
        set_it = new_it;
    }
    else{ set_it = next.descriptor_set_layouts.find(descriptors); }

    // not found -> create and insert descriptor-set layout
    if(set_it == next.descriptor_set_layouts.end())
    {
        auto new_set = vierkant::create_descriptor_set_layout(m_device, descriptors);
        set_it = next.descriptor_set_layouts.insert(
                std::make_pair(std::move(descriptors), std::move(new_set))).first;
    }
    return set_it->second;
}

glm::vec2 Renderer::clipping_distances(const glm::mat4 &projection)
{
    glm::vec2 ret;
    auto &c = projection[2][2]; // zFar / (zNear - zFar);
    auto &d = projection[3][2]; // -(zFar * zNear) / (zFar - zNear);

    // n = near clip plane distance
    ret.x = d / c;

    // f  = far clip plane distance
    ret.y = d / (c + 1.f);

    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Renderer::asset_key_t::operator==(const Renderer::asset_key_t &other) const
{
    if(mesh != other.mesh){ return false; }
    if(matrix_buffer_index != other.matrix_buffer_index){ return false; }
    if(material_buffer_index != other.material_buffer_index){ return false; }
    if(descriptors != other.descriptors){ return false; }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

size_t Renderer::asset_key_hash_t::operator()(const Renderer::asset_key_t &key) const
{
    size_t h = 0;
    crocore::hash_combine(h, key.mesh);
    crocore::hash_combine(h, key.matrix_buffer_index);
    crocore::hash_combine(h, key.material_buffer_index);

    for(const auto &pair : key.descriptors)
    {
        crocore::hash_combine(h, pair.first);
        crocore::hash_combine(h, pair.second);
    }
    return h;
}

///////////////////////////////////////////////////////////////////////////////////////////////////


}//namespace vierkant