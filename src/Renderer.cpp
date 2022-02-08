//
// Created by crocdialer on 3/22/19.
//

#include <unordered_set>
#include <crocore/Area.hpp>
#include <vierkant/Pipeline.hpp>
#include "vierkant/Renderer.hpp"

namespace vierkant
{

using std::chrono::steady_clock;
using std::chrono::duration_cast;
using duration_t = std::chrono::duration<float>;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct mesh_index_key_t
{
    vierkant::MeshConstPtr mesh;
    std::vector<vierkant::ImagePtr> textures;

    inline bool operator==(const mesh_index_key_t &other) const
    {
        return mesh == other.mesh && textures == other.textures;
    }
};

struct mesh_index_hash_t
{
    size_t operator()(mesh_index_key_t const &key) const
    {
        size_t h = 0;
        crocore::hash_combine(h, key.mesh);
        for(const auto &tex : key.textures){ crocore::hash_combine(h, tex); }
        return h;
    }
};

using mesh_index_map_t = std::unordered_map<mesh_index_key_t, size_t, mesh_index_hash_t>;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<Renderer::drawable_t> Renderer::create_drawables(const MeshConstPtr &mesh,
                                                             const glm::mat4 &model_view,
                                                             std::function<bool(
                                                                     const Mesh::entry_t &entry)> entry_filter)
{
    if(!mesh){ return {}; }

    // reserve space for one drawable per mesh-entry
    std::vector<Renderer::drawable_t> ret;
    ret.reserve(mesh->entries.size());

    // same for all entries
    auto binding_descriptions = mesh->binding_descriptions();
    auto attribute_descriptions = mesh->attribute_descriptions();

    // default filters disabled entries
    if(!entry_filter)
    {
        entry_filter = [](const Mesh::entry_t &entry) -> bool{ return entry.enabled; };
    }

    for(uint32_t i = 0; i < mesh->entries.size(); ++i)
    {
        const auto &entry = mesh->entries[i];

        // filter disabled entries, sanity check material-index
        if(!entry_filter(entry)){ continue; }
        if(entry.material_index >= mesh->materials.size()){ continue; }

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
        drawable.material.emission = glm::vec4(material->emission, 0.f);
        drawable.material.ambient = material->occlusion;
        drawable.material.roughness = material->roughness;
        drawable.material.metalness = material->metalness;
        drawable.material.blend_mode = static_cast<uint32_t>(material->blend_mode);
        drawable.material.alpha_cutoff = material->alpha_cutoff;

        drawable.base_index = entry.base_index;
        drawable.num_indices = entry.num_indices;
        drawable.vertex_offset = entry.vertex_offset;
        drawable.num_vertices = entry.num_vertices;

        drawable.pipeline_format.binding_descriptions = binding_descriptions;
        drawable.pipeline_format.attribute_descriptions = attribute_descriptions;
        drawable.pipeline_format.primitive_topology = entry.primitive_type;
        drawable.pipeline_format.blend_state.blendEnable = material->blend_mode == vierkant::Material::BlendMode::Blend;
        drawable.pipeline_format.depth_test = material->depth_test;
        drawable.pipeline_format.depth_write = material->depth_write;
        drawable.pipeline_format.cull_mode = material->two_sided ? VK_CULL_MODE_NONE : material->cull_mode;

        // descriptors
        vierkant::descriptor_t desc_matrices = {};
        desc_matrices.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_matrices.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        drawable.descriptors[BINDING_MATRIX] = desc_matrices;

        vierkant::descriptor_t desc_material = {};
        desc_material.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
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

        // push drawable to vector
        ret.push_back(std::move(drawable));
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
    vierkant::descriptor_count_t descriptor_counts = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096},
                                                      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         4096},
                                                      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         4096}};
    m_descriptor_pool = vierkant::create_descriptor_pool(m_device, descriptor_counts, 4096);

    if(create_info.pipeline_cache){ m_pipeline_cache = create_info.pipeline_cache; }
    else{ m_pipeline_cache = vierkant::PipelineCache::create(m_device); }

    // push constant range
    m_push_constant_range.offset = 0;
    m_push_constant_range.size = sizeof(push_constants_t);
    m_push_constant_range.stageFlags = VK_SHADER_STAGE_ALL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Renderer::Renderer(Renderer &&other) noexcept:
        Renderer()
{
    swap(*this, other);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Renderer &Renderer::operator=(Renderer other)
{
    swap(*this, other);
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::stage_drawable(drawable_t drawable)
{
    std::lock_guard<std::mutex> lock_guard(m_staging_mutex);
    m_staged_drawables[m_current_index].push_back(std::move(drawable));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::stage_drawables(std::vector<drawable_t> drawables)
{
    std::lock_guard<std::mutex> lock_guard(m_staging_mutex);
    std::move(drawables.begin(), drawables.end(), std::back_inserter(m_staged_drawables[m_current_index]));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
    next_assets.matrix_buffer = std::move(current_assets.matrix_buffer);
    next_assets.material_buffer = std::move(current_assets.material_buffer);

    VkCommandBufferInheritanceInfo inheritance = {};
    inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritance.framebuffer = framebuffer.handle();
    inheritance.renderPass = framebuffer.renderpass().get();

    // fetch and start commandbuffer
    next_assets.command_buffer = std::move(current_assets.command_buffer);
    auto &command_buffer = next_assets.command_buffer;
    command_buffer.begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, &inheritance);

    // sort by pipelines
    struct indexed_drawable_t
    {
        uint32_t object_index = 0;
        vierkant::DescriptorSetLayoutPtr descriptor_set_layout = nullptr;
        drawable_t *drawable = nullptr;
    };
    std::unordered_map<graphics_pipeline_info_t, std::vector<indexed_drawable_t>> pipelines;

    std::vector<vierkant::ImagePtr> textures;
    std::unordered_map<vierkant::ImagePtr, size_t> texture_index_map = {{nullptr, 0}};

    auto create_mesh_key = [](const drawable_t &drawable) -> mesh_index_key_t
    {
        auto it = drawable.descriptors.find(BINDING_TEXTURES);
        if(it == drawable.descriptors.end() || it->second.image_samplers.empty()){ return {drawable.mesh, {}}; }

        const auto &drawable_textures = it->second.image_samplers;
        return {drawable.mesh, drawable_textures};
    };

    mesh_index_map_t texture_base_index_map;

    // swoop all texture-indices
    for(const auto &drawable : current_assets.drawables)
    {
        auto it = drawable.descriptors.find(BINDING_TEXTURES);
        if(it == drawable.descriptors.end() || it->second.image_samplers.empty()){ continue; }

        const auto &drawable_textures = it->second.image_samplers;

            // insert other textures from drawables
            mesh_index_key_t key = {drawable.mesh, drawable_textures};

            if(!texture_base_index_map.count(key))
            {
                texture_base_index_map[key] = textures.size();

                for(const auto &tex : drawable_textures)
                {
                        texture_index_map[tex] = textures.size();
                        textures.push_back(tex);
                }
            }
    }

    // transition image-layouts
    for(const auto &tex : textures)
    {
        tex->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, command_buffer.handle());
    }

    vierkant::descriptor_map_t bindless_texture_desc;

    vierkant::descriptor_t desc_texture = {};
    desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_texture.image_samplers = textures;
    bindless_texture_desc[BINDING_TEXTURES] = desc_texture;

    auto bindless_texture_layout = find_set_layout(bindless_texture_desc,
                                                   current_assets,
                                                   next_assets,
                                                   true);

    // preprocess drawables
    for(uint32_t i = 0; i < current_assets.drawables.size(); i++)
    {
        auto &pipeline_format = current_assets.drawables[i].pipeline_format;
        auto &drawable = current_assets.drawables[i];
        pipeline_format.renderpass = framebuffer.renderpass().get();
        pipeline_format.viewport = viewport;
        pipeline_format.sample_count = m_sample_count;
        pipeline_format.push_constant_ranges = {m_push_constant_range};

        auto baseIndex = texture_base_index_map[create_mesh_key(drawable)];
        LOG_TRACE << "baseIndex: " << baseIndex << " - num_textures: " << textures.size();

        // adjust baseTextureIndex
        drawable.material.baseTextureIndex = baseIndex;

        indexed_drawable_t indexed_drawable = {};
        indexed_drawable.object_index = i;
        indexed_drawable.drawable = &drawable;

        if(!current_assets.drawables[i].descriptor_set_layout)
        {
            indexed_drawable.descriptor_set_layout = find_set_layout(drawable.descriptors,
                                                                     current_assets,
                                                                     next_assets,
                                                                     false);
            pipeline_format.descriptor_set_layouts = {indexed_drawable.descriptor_set_layout.get()};
        }
        else{ indexed_drawable.descriptor_set_layout = std::move(drawable.descriptor_set_layout); }

        // bindless texture-array
        pipeline_format.descriptor_set_layouts.push_back(bindless_texture_layout.get());

        pipelines[pipeline_format].push_back(indexed_drawable);
    }

    // update uniform buffers
    update_storage_buffers(current_assets.drawables, next_assets);

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

        std::vector<std::pair<vierkant::MeshConstPtr, std::vector<indexed_drawable_t>>> indirect_draws;

        // gather indirect drawing commands into buffers
        for(auto &indexed_drawable : indexed_drawables)
        {
            auto &drawable = indexed_drawable.drawable;

            if(indirect_draws.empty() || indirect_draws.back().first != drawable->mesh)
            {
                indirect_draws.push_back({drawable->mesh, {}});
            }
            indirect_draws.back().second.push_back(indexed_drawable);

            if(drawable->mesh && drawable->mesh->index_buffer)
            {
                auto draw_command =
                        static_cast<VkDrawIndexedIndirectCommand *>(next_assets.indexed_indirect_draw_buffer->map()) +
                        indexed_drawable.object_index;

                draw_command->firstIndex = drawable->base_index;
                draw_command->indexCount = drawable->num_indices;
                draw_command->vertexOffset = drawable->vertex_offset;
                draw_command->firstInstance = indexed_drawable.object_index;
                draw_command->instanceCount = 1;
            }
            else
            {
                auto draw_command = static_cast<VkDrawIndirectCommand *>(next_assets.indirect_draw_buffer->map()) +
                                    indexed_drawable.object_index;

                draw_command->vertexCount = drawable->num_vertices;
                draw_command->instanceCount = 1;
                draw_command->firstVertex = drawable->vertex_offset;
                draw_command->firstInstance = indexed_drawable.object_index;
            }
        }

        for(auto &[mesh, drawables] : indirect_draws)
        {
            const auto &indexed_drawable = drawables.front();
            auto drawable = indexed_drawable.drawable;

            if(drawables.size() > 1){ LOG_TRACE << "batching " << drawables.size() << " drawcalls"; }

            if(mesh){ mesh->bind_buffers(command_buffer.handle()); }

            if(dynamic_scissor)
            {
                // set dynamic scissor
                vkCmdSetScissor(command_buffer.handle(), 0, 1, &drawable->pipeline_format.scissor);
            }

            // update/create descriptor set
            auto &descriptors = drawable->descriptors;

            // predefined buffers
            if(!drawable->use_own_buffers)
            {
                descriptors[BINDING_MATRIX].buffers = {next_assets.matrix_buffer};
                descriptors[BINDING_MATERIAL].buffers = {next_assets.material_buffer};

                if(descriptors.count(BINDING_PREVIOUS_MATRIX) && next_assets.matrix_history_buffer)
                {
                    descriptors[BINDING_PREVIOUS_MATRIX].buffers = {next_assets.matrix_history_buffer};
                }
            }

            auto descriptor_set = find_set(mesh, indexed_drawable.descriptor_set_layout, descriptors, current_assets,
                                           next_assets, false);

            auto bindless_texture_set = find_set(mesh, bindless_texture_layout, bindless_texture_desc, current_assets,
                                                 next_assets, true);

            std::vector<VkDescriptorSet> descriptor_set_handles = {descriptor_set.get(), bindless_texture_set.get()};

            // bind descriptor sets (uniforms, samplers)
            vkCmdBindDescriptorSets(command_buffer.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline->layout(),
                                    0,
                                    descriptor_set_handles.size(),
                                    descriptor_set_handles.data(),
                                    0,
                                    nullptr);

            // update push_constants for each draw call
            push_constants.clipping = clipping_distances(indexed_drawable.drawable->matrices.projection);
            vkCmdPushConstants(command_buffer.handle(), pipeline->layout(), VK_SHADER_STAGE_ALL, 0,
                               sizeof(push_constants_t), &push_constants);

            // issue (indexed) drawing command
            if(mesh && mesh->index_buffer)
            {
                size_t indexed_indirect_cmd_stride = sizeof(VkDrawIndexedIndirectCommand);

                vkCmdDrawIndexedIndirect(command_buffer.handle(),
                                         next_assets.indexed_indirect_draw_buffer->handle(),
                                         indexed_indirect_cmd_stride * indexed_drawable.object_index,
                                         drawables.size(),
                                         indexed_indirect_cmd_stride);
            }
            else
            {
                size_t indirect_cmd_stride = sizeof(VkDrawIndirectCommand);

                vkCmdDrawIndirect(command_buffer.handle(),
                                  next_assets.indirect_draw_buffer->handle(),
                                  indirect_cmd_stride * indexed_drawable.object_index,
                                  drawables.size(),
                                  indirect_cmd_stride);
            }
        }
    }

    // keep the stuff in use
    current_assets = std::move(next_assets);

    // end and return commandbuffer
    current_assets.command_buffer.end();
    return current_assets.command_buffer.handle();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::reset()
{
    std::lock_guard<std::mutex> lock_guard(m_staging_mutex);
    m_current_index = 0;
    m_staged_drawables.clear();
    for(auto &frame_asset : m_render_assets){ frame_asset = {}; }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::update_storage_buffers(const std::vector<drawable_t> &drawables, Renderer::frame_assets_t &frame_asset)
{
    // joined drawable buffers
    std::vector<matrix_struct_t> matrix_data(drawables.size());
    std::vector<matrix_struct_t> matrix_history_data(drawables.size());
    std::vector<material_struct_t> material_data(drawables.size());

    for(uint32_t i = 0; i < drawables.size(); i++)
    {
        matrix_data[i] = drawables[i].matrices;
        material_data[i] = drawables[i].material;

        if(drawables[i].last_matrices){ matrix_history_data[i] = *drawables[i].last_matrices; }
        else{ matrix_history_data[i] = drawables[i].matrices; }
    }

    auto copy_to_buffer = [&device = m_device](const auto &array, vierkant::BufferPtr &out_buffer)
    {
        // create/upload joined buffers
        if(!out_buffer)
        {
            out_buffer = vierkant::Buffer::create(device, array,
                                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                  VMA_MEMORY_USAGE_CPU_TO_GPU);
        }
        else{ out_buffer->set_data(array); }
    };

    // create/upload joined buffers
    copy_to_buffer(matrix_data, frame_asset.matrix_buffer);
    copy_to_buffer(material_data, frame_asset.material_buffer);
    copy_to_buffer(matrix_history_data, frame_asset.matrix_history_buffer);

    // reserve space for indirect drawing-commands
    size_t indexed_indirect_size = drawables.size() * sizeof(VkDrawIndexedIndirectCommand);

    if(!frame_asset.indexed_indirect_draw_buffer)
    {
        frame_asset.indexed_indirect_draw_buffer = vierkant::Buffer::create(m_device, nullptr,
                                                                            indexed_indirect_size,
                                                                            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                                            VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
    else{ frame_asset.indexed_indirect_draw_buffer->set_data(nullptr, indexed_indirect_size); }

    size_t indirect_size = drawables.size() * sizeof(VkDrawIndirectCommand);

    if(!frame_asset.indirect_draw_buffer)
    {
        frame_asset.indirect_draw_buffer = vierkant::Buffer::create(m_device, nullptr,
                                                                    indirect_size,
                                                                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                                    VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
    else{ frame_asset.indirect_draw_buffer->set_data(nullptr, indirect_size); }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorSetLayoutPtr Renderer::find_set_layout(descriptor_map_t descriptors,
                                                 frame_assets_t &current,
                                                 frame_assets_t &next,
                                                 bool variable_count)
{
    // clean descriptor-map to enable sharing
    for(auto &[binding, descriptor] : descriptors)
    {
        for(auto &img : descriptor.image_samplers){ img.reset(); }
        for(auto &buf : descriptor.buffers){ buf.reset(); }
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
        auto new_set = vierkant::create_descriptor_set_layout(m_device, descriptors, variable_count);
        set_it = next.descriptor_set_layouts.insert(
                std::make_pair(std::move(descriptors), std::move(new_set))).first;
    }
    return set_it->second;
}

DescriptorSetPtr Renderer::find_set(const vierkant::MeshConstPtr &mesh,
                                    const DescriptorSetLayoutPtr &set_layout,
                                    const descriptor_map_t &descriptors,
                                    frame_assets_t &current,
                                    frame_assets_t &next,
                                    bool variable_count)
{
    // handle for a descriptor-set
    DescriptorSetPtr ret;

    // search/create descriptor set
    descriptor_set_key_t key = {};
    key.mesh = mesh;
    key.descriptors = descriptors;

    // start searching in next_assets
    auto descriptor_set_it = next.descriptor_sets.find(key);

    // not found in next assets
    if(descriptor_set_it == next.descriptor_sets.end())
    {
        // search in current assets (might already been processed for this frame)
        auto current_assets_it = current.descriptor_sets.find(key);

        // not found in current assets
        if(current_assets_it == current.descriptor_sets.end())
        {

            // create a new descriptor set
            auto descriptor_set = vierkant::create_descriptor_set(m_device, m_descriptor_pool, set_layout,
                                                                  variable_count);

            // keep handle
            ret = descriptor_set;

            // update the newly created descriptor set
            vierkant::update_descriptor_set(m_device, descriptor_set, descriptors);

            // insert all created assets and store in map
            next.descriptor_sets[key] = std::move(descriptor_set);
        }
        else
        {
            // use existing descriptor set
            auto descriptor_set = std::move(current_assets_it->second);
            current.descriptor_sets.erase(current_assets_it);

            // keep handle
            ret = descriptor_set;

            // update existing descriptor set
            vierkant::update_descriptor_set(m_device, descriptor_set, descriptors);

            next.descriptor_sets[key] = std::move(descriptor_set);
        }
    }
    else{ ret = descriptor_set_it->second; }
    return ret;
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Renderer::descriptor_set_key_t::operator==(const Renderer::descriptor_set_key_t &other) const
{
    if(mesh != other.mesh){ return false; }
    if(descriptors != other.descriptors){ return false; }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

size_t Renderer::descriptor_set_key_hash_t::operator()(const Renderer::descriptor_set_key_t &key) const
{
    size_t h = 0;
    crocore::hash_combine(h, key.mesh);

    for(const auto &pair : key.descriptors)
    {
        crocore::hash_combine(h, pair.first);
        crocore::hash_combine(h, pair.second);
    }
    return h;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


}//namespace vierkant