//
// Created by crocdialer on 3/22/19.
//

#define VK_NO_PROTOTYPES
#include <volk.h>

#include <crocore/Area.hpp>
#include <unordered_set>
#include <vierkant/Pipeline.hpp>
#include <vierkant/Rasterizer.hpp>
#include <vierkant/staging_copy.hpp>

namespace vierkant
{

using std::chrono::duration_cast;
using std::chrono::steady_clock;
using duration_t = std::chrono::duration<float>;

//! number of gpu-queries (currently only start-/end-timestamps)
constexpr uint32_t query_count = 2;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct texture_index_key_t
{
    vierkant::MeshConstPtr mesh;
    uint64_t texture_hash;

    inline bool operator==(const texture_index_key_t &other) const
    {
        return mesh == other.mesh && texture_hash == other.texture_hash;
    }
};

struct texture_index_hash_t
{
    size_t operator()(texture_index_key_t const &key) const
    {
        size_t h = 0;
        vierkant::hash_combine(h, key.mesh);
        vierkant::hash_combine(h, key.texture_hash);
        return h;
    }
};

using texture_index_map_t = std::unordered_map<texture_index_key_t, size_t, texture_index_hash_t>;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Rasterizer::Rasterizer(DevicePtr device, const create_info_t &create_info)
    : m_device(std::move(device)), m_random_engine(create_info.random_seed)
{
    if(!create_info.num_frames_in_flight) { throw std::runtime_error("could not create vierkant::Renderer"); }

    // VK_EXT_mesh_shading properties
    m_mesh_shader_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
    {
        VkPhysicalDeviceProperties2 props = {};
        props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props.pNext = &m_mesh_shader_properties;
        vkGetPhysicalDeviceProperties2(m_device->physical_device(), &props);
    }
    use_mesh_shader = create_info.enable_mesh_shader && vkCmdDrawMeshTasksEXT;
    m_mesh_task_count = create_info.mesh_task_count;

    viewport = create_info.viewport;
    scissor = create_info.scissor;
    indirect_draw = create_info.indirect_draw;
    sample_count = create_info.sample_count;

    m_staged_drawables.resize(create_info.num_frames_in_flight);
    m_frame_assets.resize(create_info.num_frames_in_flight);

    m_queue = create_info.queue;

    m_command_pool = create_info.command_pool
                             ? create_info.command_pool
                             : vierkant::create_command_pool(m_device, vierkant::Device::Queue::GRAPHICS,
                                                             VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                                     VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for(auto &render_asset: m_frame_assets)
    {
        render_asset.staging_command_buffer =
                vierkant::CommandBuffer(m_device, m_command_pool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        render_asset.command_buffer =
                vierkant::CommandBuffer(m_device, m_command_pool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        render_asset.query_pool = vierkant::create_query_pool(m_device, query_count, VK_QUERY_TYPE_TIMESTAMP);
    }

    if(create_info.descriptor_pool) { m_descriptor_pool = create_info.descriptor_pool; }
    else
    {
        vierkant::descriptor_count_t descriptor_counts = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512},
                                                          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256},
                                                          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256}};
        m_descriptor_pool = vierkant::create_descriptor_pool(m_device, descriptor_counts, 256);
    }

    m_pipeline_cache =
            create_info.pipeline_cache ? create_info.pipeline_cache : vierkant::PipelineCache::create(m_device);

    debug_label = create_info.debug_label;

    // push constant range
    m_push_constant_range.offset = 0;
    m_push_constant_range.size = sizeof(push_constants_t);
    m_push_constant_range.stageFlags = VK_SHADER_STAGE_ALL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Rasterizer::Rasterizer(Rasterizer &&other) noexcept : Rasterizer() { swap(*this, other); }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Rasterizer &Rasterizer::operator=(Rasterizer other)
{
    swap(*this, other);
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void swap(Rasterizer &lhs, Rasterizer &rhs) noexcept
{
    if(&lhs == &rhs) { return; }
    std::lock(lhs.m_staging_mutex, rhs.m_staging_mutex);
    std::lock_guard<std::mutex> lock_lhs(lhs.m_staging_mutex, std::adopt_lock);
    std::lock_guard<std::mutex> lock_rhs(rhs.m_staging_mutex, std::adopt_lock);

    std::swap(lhs.viewport, rhs.viewport);
    std::swap(lhs.scissor, rhs.scissor);
    std::swap(lhs.disable_material, rhs.disable_material);
    std::swap(lhs.debug_draw_ids, rhs.debug_draw_ids);
    std::swap(lhs.debug_label, rhs.debug_label);
    std::swap(lhs.indirect_draw, rhs.indirect_draw);
    std::swap(lhs.draw_indirect_delegate, rhs.draw_indirect_delegate);
    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.sample_count, rhs.sample_count);
    std::swap(lhs.m_pipeline_cache, rhs.m_pipeline_cache);
    std::swap(lhs.m_queue, rhs.m_queue);
    std::swap(lhs.m_command_pool, rhs.m_command_pool);
    std::swap(lhs.m_descriptor_pool, rhs.m_descriptor_pool);
    std::swap(lhs.m_staged_drawables, rhs.m_staged_drawables);
    std::swap(lhs.m_frame_assets, rhs.m_frame_assets);
    std::swap(lhs.m_current_index, rhs.m_current_index);
    std::swap(lhs.m_push_constant_range, rhs.m_push_constant_range);
    std::swap(lhs.m_start_time, rhs.m_start_time);

    std::swap(lhs.use_mesh_shader, rhs.use_mesh_shader);
    std::swap(lhs.m_mesh_task_count, rhs.m_mesh_task_count);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Rasterizer::stage_drawable(drawable_t drawable)
{
    std::lock_guard<std::mutex> lock_guard(m_staging_mutex);
    m_staged_drawables[m_current_index].push_back(std::move(drawable));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Rasterizer::stage_drawables(std::vector<drawable_t> drawables)
{
    std::lock_guard<std::mutex> lock_guard(m_staging_mutex);
    std::move(drawables.begin(), drawables.end(), std::back_inserter(m_staged_drawables[m_current_index]));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VkCommandBuffer Rasterizer::render(const vierkant::Framebuffer &framebuffer, bool recycle_commands)
{
    // increment asset-index, retrieve current assets
    auto &frame_assets = next_frame();

    // re-use prior assets and command-buffer, run delegate for buffer-updates
    if(recycle_commands && frame_assets.command_buffer)
    {
        // invoke delegate
        if(draw_indirect_delegate) { draw_indirect_delegate(frame_assets.indirect_indexed_bundle); }
        return frame_assets.command_buffer.handle();
    }

    // inject renderpass-handle
    for(auto &drawable: frame_assets.drawables)
    {
        auto &pipeline_format = drawable.pipeline_format;
        pipeline_format.renderpass = framebuffer.renderpass().get();
    }

    // (re-)create secondary command-buffer
    VkCommandBufferInheritanceInfo inheritance = {};
    inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritance.framebuffer = framebuffer.handle();
    inheritance.renderPass = framebuffer.renderpass().get();

    // begin secondaruy command-buffer
    auto &command_buffer = frame_assets.command_buffer;
    command_buffer.begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, &inheritance);

    // asset-creation and rendering
    render(frame_assets.command_buffer.handle(), frame_assets);

    // end and return commandbuffer
    frame_assets.command_buffer.end();
    return frame_assets.command_buffer.handle();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Rasterizer::render(const rendering_info_t &rendering_info)
{
    // increment asset-index, retrieve current assets
    auto &frame_assets = next_frame();

    // inject direct-rendering info
    for(auto &drawable: frame_assets.drawables)
    {
        auto &pipeline_format = drawable.pipeline_format;
        pipeline_format.renderpass = nullptr;
        pipeline_format.view_mask = rendering_info.view_mask;
        pipeline_format.color_attachment_formats = rendering_info.color_attachment_formats;
        pipeline_format.depth_attachment_format = rendering_info.depth_attachment_format;
        pipeline_format.stencil_attachment_format = rendering_info.stencil_attachment_format;
    }

    // asset-creation and rendering
    render(rendering_info.command_buffer, frame_assets);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Rasterizer::render(VkCommandBuffer command_buffer, frame_assets_t &frame_assets)
{
    // (re-)create assets and commands
    frame_assets.indirect_bundle.num_draws = frame_assets.indirect_indexed_bundle.num_draws = 0;
    std::unordered_map<descriptor_map_t, DescriptorSetLayoutPtr> next_set_layouts;
    descriptor_set_map_t next_descriptor_sets;

    struct indirect_draw_asset_t
    {
        uint32_t count_buffer_offset = 0;
        uint32_t num_draws = 0;
        uint32_t first_draw_index = 0;
        uint32_t first_indexed_draw_index = 0;
        std::vector<VkDescriptorSet> descriptor_set_handles;
        VkRect2D scissor = {};
    };
    using draw_batch_t = std::vector<std::pair<vierkant::MeshConstPtr, indirect_draw_asset_t>>;
    std::unordered_map<graphics_pipeline_info_t, draw_batch_t> pipelines;

    std::vector<vierkant::ImagePtr> textures;

    auto create_texture_hash = [](const std::vector<vierkant::ImagePtr> &textures) -> uint64_t {
        uint64_t texture_hash = 0;
        for(const auto &tex: textures) { vierkant::hash_combine(texture_hash, tex); }
        return texture_hash;
    };

    auto create_mesh_key = [create_texture_hash](const drawable_t &drawable) -> texture_index_key_t {
        auto it = drawable.descriptors.find(BINDING_TEXTURES);
        if(it == drawable.descriptors.end() || it->second.images.empty()) { return {drawable.mesh, {}}; }
        const auto &drawable_textures = it->second.images;
        return {drawable.mesh, create_texture_hash(drawable_textures)};
    };

    texture_index_map_t texture_base_index_map;

    // swoop all texture-indices
    for(const auto &drawable: frame_assets.drawables)
    {
        auto it = drawable.descriptors.find(BINDING_TEXTURES);
        if(it == drawable.descriptors.end() || it->second.images.empty()) { continue; }

        const auto &drawable_textures = it->second.images;

        // insert other textures from drawables
        texture_index_key_t key = {drawable.mesh, create_texture_hash(drawable_textures)};

        if(!texture_base_index_map.count(key))
        {
            texture_base_index_map[key] = textures.size();
            textures.insert(textures.end(), drawable_textures.begin(), drawable_textures.end());
        }
    }

    vierkant::descriptor_map_t bindless_texture_desc;

    vierkant::descriptor_t desc_texture = {};
    desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    //    desc_texture.variable_count = true;
    desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_texture.images = textures;
    bindless_texture_desc[BINDING_TEXTURES] = desc_texture;

    auto bindless_texture_layout = vierkant::find_or_create_set_layout(
            m_device, bindless_texture_desc, frame_assets.descriptor_set_layouts, next_set_layouts);

    // create/resize draw_indirect buffers
    resize_draw_indirect_buffers(frame_assets.drawables.size(), frame_assets);

    for(auto &drawable: frame_assets.drawables)
    {
        // adjust baseTextureIndex
        drawable.material.base_texture_index = texture_base_index_map[create_mesh_key(drawable)];
    }

    // update uniform buffers
    update_buffers(frame_assets.drawables, frame_assets);

    // sort by pipelines
    struct indexed_drawable_t
    {
        uint32_t object_index = 0;
        vierkant::DescriptorSetLayoutPtr descriptor_set_layout = nullptr;
        drawable_t *drawable = nullptr;
    };
    std::unordered_map<graphics_pipeline_info_t, std::vector<indexed_drawable_t>> pipeline_drawables;

    // preprocess drawables
    for(uint32_t i = 0; i < frame_assets.drawables.size(); i++)
    {
        auto &pipeline_format = frame_assets.drawables[i].pipeline_format;
        auto &drawable = frame_assets.drawables[i];
        pipeline_format.viewport = viewport;
        pipeline_format.sample_count = sample_count;
        pipeline_format.push_constant_ranges = {m_push_constant_range};

        indexed_drawable_t indexed_drawable = {};
        indexed_drawable.object_index = i;
        indexed_drawable.drawable = &drawable;

        if(!drawable.descriptor_set_layout)
        {
            // TODO: improve condition here. idea is to only provide a global texture-array
            if(indirect_draw) { drawable.descriptors.erase(BINDING_TEXTURES); }

            indexed_drawable.descriptor_set_layout = vierkant::find_or_create_set_layout(
                    m_device, drawable.descriptors, frame_assets.descriptor_set_layouts, next_set_layouts);
            pipeline_format.descriptor_set_layouts = {indexed_drawable.descriptor_set_layout.get()};
        }
        else { indexed_drawable.descriptor_set_layout = std::move(drawable.descriptor_set_layout); }

        // bindless texture-array
        pipeline_format.descriptor_set_layouts.push_back(bindless_texture_layout.get());

        // push intermediate struct
        pipeline_drawables[pipeline_format].push_back(indexed_drawable);
    }

    vierkant::BufferPtr draw_buffer = frame_assets.indirect_bundle.draws_in;
    vierkant::BufferPtr draw_buffer_indexed = frame_assets.indirect_indexed_bundle.draws_in;

    // hook up GPU frustum/occlusion/distance culling here
    if(indirect_draw && draw_indirect_delegate)
    {
        draw_buffer = frame_assets.indirect_bundle.draws_out;
        draw_buffer_indexed = frame_assets.indirect_indexed_bundle.draws_out;
    }

    // batch/pipeline index
    uint32_t count_buffer_offset = 0;

    // fill up indirect draw buffers
    for(const auto &[pipe_fmt, indexed_drawables]: pipeline_drawables)
    {
        auto &indirect_draws = pipelines[pipe_fmt];

        // gather indirect drawing commands into buffers
        for(auto &indexed_drawable: indexed_drawables)
        {
            auto &drawable = indexed_drawable.drawable;

            // create new indirect-draw batch
            if(!indirect_draw || indirect_draws.empty() || indirect_draws.back().first != drawable->mesh)
            {
                indirect_draw_asset_t new_draw = {};
                new_draw.count_buffer_offset = count_buffer_offset++;
                new_draw.first_draw_index = frame_assets.indirect_bundle.num_draws;
                new_draw.first_indexed_draw_index = frame_assets.indirect_indexed_bundle.num_draws;
                new_draw.scissor = drawable->pipeline_format.scissor;

                // predefined buffers
                if(!drawable->use_own_buffers)
                {
                    drawable->descriptors[BINDING_MESH_DRAWS].buffers = {frame_assets.mesh_draw_buffer};
                    drawable->descriptors[BINDING_MATERIAL].buffers = {frame_assets.material_buffer};

                    if(drawable->descriptors.count(BINDING_DRAW_COMMANDS) && draw_buffer_indexed)
                    {
                        drawable->descriptors[BINDING_DRAW_COMMANDS].buffers = {draw_buffer_indexed};
                    }
                }

                auto descriptor_set = vierkant::find_or_create_descriptor_set(
                        m_device, indexed_drawable.descriptor_set_layout, drawable->descriptors, m_descriptor_pool,
                        frame_assets.descriptor_sets, next_descriptor_sets, false);
                auto bindless_texture_set = vierkant::find_or_create_descriptor_set(
                        m_device, bindless_texture_layout, bindless_texture_desc, m_descriptor_pool,
                        frame_assets.descriptor_sets, next_descriptor_sets, false);

                new_draw.descriptor_set_handles = {descriptor_set.get(), bindless_texture_set.get()};

                indirect_draws.emplace_back(drawable->mesh, std::move(new_draw));
            }
            auto &indirect_draw_asset = indirect_draws.back().second;
            indirect_draw_asset.num_draws++;

            if(drawable->mesh && drawable->mesh->index_buffer)
            {
                auto draw_command = static_cast<indexed_indirect_command_t *>(
                                            frame_assets.indirect_indexed_bundle.draws_in->map()) +
                                    frame_assets.indirect_indexed_bundle.num_draws++;

                //! VkDrawIndexedIndirectCommand
                *draw_command = {};
                draw_command->vk_draw.firstIndex = drawable->base_index;
                draw_command->vk_draw.indexCount = drawable->num_indices;
                draw_command->vk_draw.vertexOffset = drawable->vertex_offset;
                draw_command->vk_draw.firstInstance = indexed_drawable.object_index;
                draw_command->vk_draw.instanceCount = 1;

                draw_command->count_buffer_offset = indirect_draw_asset.count_buffer_offset;
                draw_command->first_draw_index = indirect_draw_asset.first_indexed_draw_index;
                draw_command->object_index = indexed_drawable.object_index;
                draw_command->visible = false;

                draw_command->base_meshlet = drawable->base_meshlet;
                draw_command->num_meshlets = drawable->num_meshlets;

                //! VkDrawMeshTasksIndirectCommandEXT
                draw_command->vk_mesh_draw.groupCountX =
                        (m_mesh_task_count + drawable->num_meshlets - 1) / m_mesh_task_count;
                draw_command->vk_mesh_draw.groupCountY = draw_command->vk_mesh_draw.groupCountZ = 1;
            }
            else
            {
                auto draw_command = static_cast<VkDrawIndirectCommand *>(frame_assets.indirect_bundle.draws_in->map()) +
                                    frame_assets.indirect_bundle.num_draws++;

                draw_command->vertexCount = drawable->num_vertices;
                draw_command->instanceCount = 1;
                draw_command->firstVertex = drawable->vertex_offset;
                draw_command->firstInstance = indexed_drawable.object_index;
            }
        }
    }

    // hook up GPU frustum/occlusion/distance culling here
    if(indirect_draw && draw_indirect_delegate)
    {
        // invoke delegate
        draw_indirect_delegate(frame_assets.indirect_indexed_bundle);
    }

    // push constants
    push_constants_t push_constants = {};
    push_constants.size = {viewport.width, viewport.height};
    push_constants.time = duration_cast<duration_t>(steady_clock::now() - m_start_time).count();
    push_constants.random_seed = m_random_engine();
    push_constants.disable_material = disable_material;
    push_constants.debug_draw_ids = debug_draw_ids;

    // record start-timestamp
    if(debug_label) { vierkant::begin_label(command_buffer, *debug_label); }
    vkCmdWriteTimestamp2(command_buffer, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, frame_assets.query_pool.get(), 0);

    // grouped by pipelines
    for(auto &[pipe_fmt, indirect_draws]: pipelines)
    {
        // select/create pipeline
        auto pipeline = m_pipeline_cache->pipeline(pipe_fmt);

        // bind pipeline
        pipeline->bind(command_buffer);

        bool dynamic_scissor = crocore::contains(pipe_fmt.dynamic_states, VK_DYNAMIC_STATE_SCISSOR);

        if(crocore::contains(pipe_fmt.dynamic_states, VK_DYNAMIC_STATE_VIEWPORT))
        {
            // set dynamic viewport
            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        }

        vierkant::MeshConstPtr current_mesh;

        for(auto &[mesh, draw_asset]: indirect_draws)
        {
            push_constants.base_draw_index = draw_asset.first_indexed_draw_index;
            vkCmdPushConstants(command_buffer, pipeline->layout(), VK_SHADER_STAGE_ALL, 0, sizeof(push_constants_t),
                               &push_constants);

            // feature enabled/available, mesh exists and contains a meshlet-buffer. skinning/morphing not supported
            bool use_meshlets = vkCmdDrawMeshTasksEXT && use_mesh_shader && mesh && mesh->meshlets &&
                                !mesh->root_bone && !mesh->morph_buffer;

            if(mesh && current_mesh != mesh)
            {
                current_mesh = mesh;
                if(!use_meshlets) { mesh->bind_buffers(command_buffer); }
            }

            // bind descriptor sets (uniforms, samplers)
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout(), 0,
                                    draw_asset.descriptor_set_handles.size(), draw_asset.descriptor_set_handles.data(),
                                    0, nullptr);

            if(dynamic_scissor)
            {
                // set dynamic scissor
                vkCmdSetScissor(command_buffer, 0, 1, &draw_asset.scissor);
            }

            if(indirect_draw)
            {
                constexpr size_t indexed_indirect_cmd_stride = sizeof(indexed_indirect_command_t);
                constexpr uint32_t mesh_draw_cmd_offset = offsetof(indexed_indirect_command_t, vk_mesh_draw);

                if(mesh && mesh->index_buffer)
                {
                    const indirect_draw_bundle_t &draw_params = frame_assets.indirect_indexed_bundle;

                    // issue (indexed) drawing command
                    if(draw_params.draws_counts_out)
                    {
                        if(use_meshlets)
                        {
                            vkCmdDrawMeshTasksIndirectCountEXT(command_buffer, draw_buffer_indexed->handle(),
                                                               mesh_draw_cmd_offset +
                                                                       indexed_indirect_cmd_stride *
                                                                               draw_asset.first_indexed_draw_index,
                                                               draw_params.draws_counts_out->handle(),
                                                               draw_asset.count_buffer_offset * sizeof(uint32_t),
                                                               draw_asset.num_draws, indexed_indirect_cmd_stride);
                        }
                        else
                        {
                            vkCmdDrawIndexedIndirectCount(command_buffer, draw_buffer_indexed->handle(),
                                                          indexed_indirect_cmd_stride *
                                                                  draw_asset.first_indexed_draw_index,
                                                          draw_params.draws_counts_out->handle(),
                                                          draw_asset.count_buffer_offset * sizeof(uint32_t),
                                                          draw_asset.num_draws, indexed_indirect_cmd_stride);
                        }
                    }
                    else
                    {
                        if(use_meshlets)
                        {
                            vkCmdDrawMeshTasksIndirectEXT(command_buffer, draw_buffer_indexed->handle(),
                                                          mesh_draw_cmd_offset +
                                                                  indexed_indirect_cmd_stride *
                                                                          draw_asset.first_indexed_draw_index,
                                                          draw_asset.num_draws, indexed_indirect_cmd_stride);
                        }
                        else
                        {
                            vkCmdDrawIndexedIndirect(command_buffer, draw_buffer_indexed->handle(),
                                                     indexed_indirect_cmd_stride * draw_asset.first_indexed_draw_index,
                                                     draw_asset.num_draws, indexed_indirect_cmd_stride);
                        }
                    }
                }
                else
                {
                    constexpr size_t indirect_cmd_stride = sizeof(VkDrawIndirectCommand);

                    vkCmdDrawIndirect(command_buffer, draw_buffer->handle(),
                                      indirect_cmd_stride * draw_asset.first_draw_index, draw_asset.num_draws,
                                      indirect_cmd_stride);
                }
            }
            else
            {
                if(use_meshlets)
                {
                    for(uint32_t i = 0; i < draw_asset.num_draws; ++i)
                    {
                        auto cmd = static_cast<indexed_indirect_command_t *>(
                                           frame_assets.indirect_indexed_bundle.draws_in->map()) +
                                   draw_asset.first_indexed_draw_index + i;

                        vkCmdDrawMeshTasksEXT(command_buffer, cmd->vk_mesh_draw.groupCountX,
                                              cmd->vk_mesh_draw.groupCountY, cmd->vk_mesh_draw.groupCountZ);
                    }
                }
                else if(mesh && mesh->index_buffer)
                {
                    for(uint32_t i = 0; i < draw_asset.num_draws; ++i)
                    {
                        auto cmd = static_cast<indexed_indirect_command_t *>(
                                           frame_assets.indirect_indexed_bundle.draws_in->map()) +
                                   draw_asset.first_indexed_draw_index + i;

                        vkCmdDrawIndexed(command_buffer, cmd->vk_draw.indexCount, cmd->vk_draw.instanceCount,
                                         cmd->vk_draw.firstIndex, cmd->vk_draw.vertexOffset,
                                         cmd->vk_draw.firstInstance);
                    }
                }
                else
                {
                    for(uint32_t i = 0; i < draw_asset.num_draws; ++i)
                    {
                        auto cmd = static_cast<VkDrawIndirectCommand *>(frame_assets.indirect_bundle.draws_in->map()) +
                                   draw_asset.first_draw_index + i;

                        vkCmdDraw(command_buffer, cmd->vertexCount, cmd->instanceCount, cmd->firstVertex,
                                  cmd->firstInstance);
                    }
                }
            }
        }
    }

    // record end-timestamp
    vkCmdWriteTimestamp2(command_buffer, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, frame_assets.query_pool.get(), 1);
    if(debug_label) { vierkant::end_label(command_buffer); }

    // keep the stuff in use
    frame_assets.descriptor_set_layouts = std::move(next_set_layouts);
    frame_assets.descriptor_sets = std::move(next_descriptor_sets);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Rasterizer::reset()
{
    std::lock_guard<std::mutex> lock_guard(m_staging_mutex);
    m_current_index = 0;
    m_staged_drawables.clear();
    for(auto &frame_asset: m_frame_assets) { frame_asset = {}; }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Rasterizer::update_buffers(const std::vector<drawable_t> &drawables, Rasterizer::frame_assets_t &frame_asset)
{
    std::vector<mesh_entry_t> mesh_entries;
    std::map<std::pair<vierkant::MeshConstPtr, uint32_t>, uint32_t> mesh_entry_map;

    // maps -> material-index
    std::unordered_map<vierkant::MaterialConstPtr, uint32_t> material_index_map;

    // joined drawable buffers
    std::vector<mesh_draw_t> mesh_draws(drawables.size());
    std::vector<material_struct_t> material_data;

    for(uint32_t i = 0; i < drawables.size(); i++)
    {
        uint32_t mesh_index = 0;
        vierkant::MaterialConstPtr mat;

        if(drawables[i].mesh && !drawables[i].mesh->entries.empty())
        {
            auto mesh_entry_it = mesh_entry_map.find({drawables[i].mesh, drawables[i].entry_index});
            if(mesh_entry_it == mesh_entry_map.end())
            {
                mesh_index = mesh_entries.size();
                mesh_entry_map[{drawables[i].mesh, drawables[i].entry_index}] = mesh_index;
                const auto &e = drawables[i].mesh->entries[drawables[i].entry_index];
                mesh_entry_t mesh_entry = {};
                mesh_entry.vertex_offset = e.vertex_offset;
                mesh_entry.vertex_count = e.num_vertices;
                mesh_entry.lod_count = e.lods.size();
                memcpy(mesh_entry.lods, e.lods.data(),
                       std::min(sizeof(mesh_entry.lods), e.lods.size() * sizeof(Mesh::lod_t)));
                mesh_entry.center = e.bounding_sphere.center;
                mesh_entry.radius = e.bounding_sphere.radius;
                mesh_entries.push_back(mesh_entry);
            }
            else { mesh_index = mesh_entry_it->second; }

            mat = drawables[i].mesh->materials[drawables[i].mesh->entries[drawables[i].entry_index].material_index];

            if(!material_index_map.contains(mat))
            {
                material_index_map[mat] = material_data.size();
                material_data.push_back(drawables[i].material);
            }
        }
        else { material_data.push_back(drawables[i].material); }

        mesh_draws[i].current_matrices = drawables[i].matrices;
        mesh_draws[i].mesh_index = mesh_index;
        mesh_draws[i].material_index = material_index_map[mat];

        if(drawables[i].last_matrices) { mesh_draws[i].last_matrices = *drawables[i].last_matrices; }
        else { mesh_draws[i].last_matrices = drawables[i].matrices; }
    }

    auto copy_to_buffer = [&device = m_device](const auto &array, vierkant::BufferPtr &out_buffer) {
        // create/upload joined buffers
        if(!out_buffer)
        {
            out_buffer = vierkant::Buffer::create(device, array,
                                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                  VMA_MEMORY_USAGE_CPU_TO_GPU);
        }
        else { out_buffer->set_data(array); }
    };

    std::vector<staging_copy_info_t> staging_copies;

    auto add_staging_copy = [&staging_copies, device = m_device](const auto &array, vierkant::BufferPtr &outbuffer,
                                                                 VkPipelineStageFlags2 dst_stage,
                                                                 VkAccessFlags2 dst_access) {
        using elem_t = typename std::decay<decltype(array)>::type::value_type;
        size_t num_bytes = array.size() * sizeof(elem_t);

        if(!outbuffer)
        {
            outbuffer = vierkant::Buffer::create(device, nullptr, num_bytes,
                                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                 VMA_MEMORY_USAGE_GPU_ONLY);
        }
        else { outbuffer->set_data(nullptr, num_bytes); }

        vierkant::staging_copy_info_t info = {};
        info.num_bytes = num_bytes;
        info.data = array.data();
        info.dst_buffer = outbuffer;
        info.dst_stage = dst_stage;
        info.dst_access = dst_access;
        staging_copies.push_back(std::move(info));
    };

    if(m_queue)
    {
        if(!frame_asset.staging_buffer)
        {
            frame_asset.staging_buffer = vierkant::Buffer::create(
                    m_device, nullptr, 1UL << 20, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
        }
        frame_asset.staging_command_buffer.begin();

        add_staging_copy(mesh_entries, frame_asset.mesh_entry_buffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                         VK_ACCESS_2_SHADER_READ_BIT);
        add_staging_copy(mesh_draws, frame_asset.mesh_draw_buffer,
                         VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                         VK_ACCESS_2_SHADER_READ_BIT);
        add_staging_copy(material_data, frame_asset.material_buffer, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                         VK_ACCESS_2_SHADER_READ_BIT);

        vierkant::staging_copy_context_t staging_context = {};
        staging_context.command_buffer = frame_asset.staging_command_buffer.handle();
        staging_context.staging_buffer = frame_asset.staging_buffer;
        vierkant::staging_copy(staging_context, staging_copies);

        frame_asset.staging_command_buffer.submit(m_queue);
    }
    else
    {
        // create/upload joined buffers
        copy_to_buffer(mesh_entries, frame_asset.mesh_entry_buffer);
        copy_to_buffer(mesh_draws, frame_asset.mesh_draw_buffer);
        copy_to_buffer(material_data, frame_asset.material_buffer);
    }

    frame_asset.indirect_indexed_bundle.mesh_draws = frame_asset.mesh_draw_buffer;
    frame_asset.indirect_indexed_bundle.materials = frame_asset.material_buffer;
    frame_asset.indirect_indexed_bundle.mesh_entries = frame_asset.mesh_entry_buffer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Rasterizer::resize_draw_indirect_buffers(uint32_t num_drawables, frame_assets_t &frame_asset)
{
    // reserve space for indirect drawing-commands
    size_t num_bytes_indexed = std::max<size_t>(num_drawables * sizeof(indexed_indirect_command_t), 1UL << 20);
    size_t num_bytes = std::max<size_t>(num_drawables * sizeof(VkDrawIndirectCommand), 1UL << 20);

    if(!frame_asset.indirect_indexed_bundle.draws_in ||
       frame_asset.indirect_indexed_bundle.draws_in->num_bytes() < num_bytes_indexed)
    {
        frame_asset.indirect_indexed_bundle.draws_in =
                vierkant::Buffer::create(m_device, nullptr, num_bytes_indexed,
                                         VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
    else { frame_asset.indirect_indexed_bundle.draws_in->set_data(nullptr, num_bytes_indexed); }

    if(!frame_asset.indirect_bundle.draws_in || frame_asset.indirect_bundle.draws_in->num_bytes() < num_bytes)
    {
        frame_asset.indirect_bundle.draws_in = vierkant::Buffer::create(
                m_device, nullptr, num_bytes, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
    else { frame_asset.indirect_bundle.draws_in->set_data(nullptr, num_bytes); }

    //////////////////////////// indirect-draw GPU buffers /////////////////////////////////////////////////////////////

    if(indirect_draw)
    {
        if(!frame_asset.indirect_indexed_bundle.draws_out ||
           frame_asset.indirect_indexed_bundle.draws_out->num_bytes() < num_bytes_indexed)
        {
            frame_asset.indirect_indexed_bundle.draws_out = vierkant::Buffer::create(
                    m_device, nullptr, num_bytes_indexed,
                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY);
        }
        else { frame_asset.indirect_indexed_bundle.draws_out->set_data(nullptr, num_bytes_indexed); }

        if(!frame_asset.indirect_bundle.draws_out || frame_asset.indirect_bundle.draws_out->num_bytes() < num_bytes)
        {
            frame_asset.indirect_bundle.draws_out = vierkant::Buffer::create(
                    m_device, nullptr, num_bytes,
                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY);
        }
        else { frame_asset.indirect_bundle.draws_out->set_data(nullptr, num_bytes); }
    }
}

Rasterizer::frame_assets_t &Rasterizer::next_frame()
{
    uint32_t current_index;
    {
        std::lock_guard<std::mutex> lock_guard(m_staging_mutex);
        current_index = m_current_index;
        m_current_index = (m_current_index + 1) % m_frame_assets.size();
        m_frame_assets[current_index].drawables = std::move(m_staged_drawables[current_index]);
    }
    auto &frame_assets = m_frame_assets[current_index];

    // retrieve last frame-timestamps for this index
    uint64_t timestamps[query_count] = {};

    auto query_result = vkGetQueryPoolResults(m_device->handle(), frame_assets.query_pool.get(), 0, query_count,
                                              sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

    if(query_result == VK_SUCCESS)
    {
        // calculate last gpu-frametime
        auto frame_ns = std::chrono::nanoseconds(static_cast<uint64_t>(double(timestamps[1] - timestamps[0]) *
                                                                       m_device->properties().limits.timestampPeriod));
        frame_assets.frame_time = std::chrono::duration_cast<double_millisecond_t>(frame_ns);

        // reset query-pool
        vkResetQueryPool(m_device->handle(), frame_assets.query_pool.get(), 0, query_count);
    }
    return frame_assets;
}

}//namespace vierkant