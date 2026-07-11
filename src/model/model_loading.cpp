#include <meshoptimizer.h>
#include <spdlog/spdlog.h>
#include <vierkant/hash.hpp>
#include <vierkant/model/gltf.hpp>
#include <vierkant/model/model_loading.hpp>
#include <vierkant/model/wavefront_obj.hpp>
#include <vierkant/vertex_splicer.hpp>

namespace vierkant::model
{

VkFormat vk_format(const crocore::ImagePtr &img)
{
    VkFormat ret = VK_FORMAT_UNDEFINED;

    switch(img->num_components())
    {
        case 1: ret = VK_FORMAT_R8_UNORM; break;
        case 2: ret = VK_FORMAT_R8G8_UNORM; break;
        case 3: ret = VK_FORMAT_R8G8B8_UNORM; break;
        case 4: ret = VK_FORMAT_R8G8B8A8_UNORM; break;
    }
    return ret;
}

VkSamplerAddressMode vk_sampler_address_mode(const vierkant::texture_sampler_t::AddressMode &address_mode)
{
    switch(address_mode)
    {
        case texture_sampler_t::AddressMode::MIRRORED_REPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case texture_sampler_t::AddressMode::CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case texture_sampler_t::AddressMode::CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case texture_sampler_t::AddressMode::MIRROR_CLAMP_TO_EDGE:
        case texture_sampler_t::AddressMode::REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        default: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    }
}

VkFilter vk_filter(const vierkant::texture_sampler_t::Filter &filter)
{
    switch(filter)
    {

        case texture_sampler_t::Filter::NEAREST: return VK_FILTER_NEAREST;
        case texture_sampler_t::Filter::LINEAR: return VK_FILTER_LINEAR;
        case texture_sampler_t::Filter::CUBIC: return VK_FILTER_CUBIC_EXT;
        default: break;
    }
    return VK_FILTER_NEAREST;
}

vierkant::VkSamplerPtr create_sampler(const vierkant::DevicePtr &device, const vierkant::texture_sampler_t &ts,
                                      uint32_t num_mips)
{
    VkSamplerCreateInfo sampler_create_info = {};
    sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_create_info.magFilter = vk_filter(ts.mag_filter);
    sampler_create_info.minFilter = vk_filter(ts.min_filter);
    sampler_create_info.addressModeU = vk_sampler_address_mode(ts.address_mode_u);
    sampler_create_info.addressModeV = vk_sampler_address_mode(ts.address_mode_v);
    sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    sampler_create_info.anisotropyEnable = true;
    sampler_create_info.maxAnisotropy = device->properties().core.limits.maxSamplerAnisotropy;

    sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_create_info.unnormalizedCoordinates = false;
    sampler_create_info.compareEnable = VK_FALSE;
    sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_create_info.mipLodBias = 0.0f;
    sampler_create_info.minLod = 0.0f;
    sampler_create_info.maxLod = static_cast<float>(num_mips);

    VkSampler sampler;
    vkCheck(vkCreateSampler(device->handle(), &sampler_create_info, nullptr, &sampler),
            "failed to create texture sampler!");
    return {sampler, [device](VkSampler s) { vkDestroySampler(device->handle(), s, nullptr); }};
}

bool compress_textures(vierkant::model::model_assets_t &mesh_assets, crocore::ThreadPoolClassic *pool)
{
    std::chrono::milliseconds compress_total_duration(0);
    size_t num_pixels = 0;

    auto check_normal_map = [&mesh_assets](TextureId tex_id) -> bool {
        return std::ranges::any_of(mesh_assets.materials.begin(), mesh_assets.materials.end(),
                                   [&tex_id](const auto &m) -> bool

                                   {
                                       auto it = m.texture_data.find(TextureType::Normal);
                                       return it != m.texture_data.end() && it->second.texture_id == tex_id;
                                   });
    };

    for(auto &[tex_id, texture_variant]: mesh_assets.textures)
    {
        bool is_normal_map = check_normal_map(tex_id);

        try
        {
            texture_variant = std::visit(
                    [pool, is_normal_map, &compress_total_duration, &num_pixels](auto &&img) -> texture_variant_t {
                        using T = std::decay_t<decltype(img)>;

                        if constexpr(std::is_same_v<T, crocore::ImagePtr>)
                        {
                            if(img)
                            {
                                bcn::compress_info_t compress_info = {};
                                compress_info.image = img;
                                compress_info.mode = is_normal_map ? bcn::BC5 : bcn::BC7;
                                compress_info.generate_mipmaps = true;
                                if(pool)
                                {
                                    compress_info.delegate_fn = [pool](auto fn) { return pool->post(fn); };
                                }
                                auto compressed_img = bcn::compress(compress_info);
                                compress_total_duration += compressed_img.duration;
                                num_pixels += img->width() * img->height();
                                return compressed_img;
                            }
                        }

                        // variant was already compressed
                        return img;
                    },
                    texture_variant);
        } catch(std::bad_variant_access &e)
        {
            spdlog::error("could not compress textures: {}", e.what());
            return false;
        }
    }
    float mpx_per_sec =
            1.e-6f * static_cast<float>(num_pixels) / std::chrono::duration<float>(compress_total_duration).count();
    spdlog::debug("compressed {} images in {} ms - avg. {:03.2f} Mpx/s", mesh_assets.textures.size(),
                  compress_total_duration.count(), mpx_per_sec);
    return true;
}

std::vector<mesh_omm_data_t> generate_omm_data(const model_assets_t &mesh_assets,
                                               const vierkant::mesh_buffer_bundle_t &bundle,
                                               const omm_gen_params_t &params)
{
    std::vector<mesh_omm_data_t> ret;

    if(bundle.vertex_stride != sizeof(vierkant::packed_vertex_t))
    {
        spdlog::warn("generate_omm_data: non-packed vertex stride {}, skipping", bundle.vertex_stride);
        return ret;
    }

    const auto &materials = mesh_assets.materials;
    const auto &textures = mesh_assets.textures;

    const auto *vertex_base = reinterpret_cast<const vierkant::packed_vertex_t *>(bundle.vertex_buffer.data());

    for(uint32_t entry_idx = 0; entry_idx < bundle.entries.size(); ++entry_idx)
    {
        const auto &entry = bundle.entries[entry_idx];
        if(entry.lods.empty() || entry.material_index >= materials.size()) { continue; }
        const auto &lod_0 = entry.lods.front();
        const auto &material = materials[entry.material_index];

        // opacity micromaps encode an alpha-test; only meaningful for mask materials
        if(material.blend_mode != vierkant::BlendMode::Mask) { continue; }

        auto color_it = material.texture_data.find(vierkant::TextureType::Color);
        if(color_it == material.texture_data.end()) { continue; }
        const vierkant::TextureId color_texture_id = color_it->second.texture_id;

        auto tex_it = textures.find(color_texture_id);
        if(tex_it == textures.end()) { continue; }

        const auto *cpu_img = std::get_if<crocore::ImagePtr>(&tex_it->second);
        if(!cpu_img || !*cpu_img) { continue; }  // BCN-only, no CPU data

        const auto &img = *cpu_img;
        const uint32_t num_components = img->num_components();
        if(num_components < 2 || num_components == 3) { continue; }  // no alpha channel
        const uint32_t alpha_offset = num_components - 1;

        const uint32_t num_triangles = lod_0.num_indices / 3;
        const uint32_t num_vertices = entry.num_vertices;

        // extract float2 UVs from packed vertex buffer (fp16 → float)
        std::vector<float> uvs(num_vertices * 2);
        const auto *verts = vertex_base + entry.vertex_offset;
        for(uint32_t v = 0; v < num_vertices; ++v)
        {
            uvs[2 * v + 0] = meshopt_dequantizeHalf(verts[v].texcoord_x);
            uvs[2 * v + 1] = meshopt_dequantizeHalf(verts[v].texcoord_y);
        }

        const uint32_t *indices = bundle.index_buffer.data() + lod_0.base_index;

        std::vector<unsigned char> levels(num_triangles);
        std::vector<unsigned int> sources(num_triangles);
        std::vector<int> omm_indices(num_triangles);

        size_t omm_count = meshopt_opacityMapMeasure(
                levels.data(), sources.data(), omm_indices.data(), indices, lod_0.num_indices, uvs.data(),
                num_vertices, 2 * sizeof(float), img->width(), img->height(), params.max_level,
                params.target_edge);

        if(omm_count == 0) { continue; }

        // compute per-entry sizes and offsets
        std::vector<unsigned int> offsets(omm_count);
        size_t total_data_size = 0;
        for(size_t i = 0; i < omm_count; ++i)
        {
            offsets[i] = static_cast<unsigned int>(total_data_size);
            total_data_size += meshopt_opacityMapEntrySize(levels[i], params.states);
        }

        std::vector<unsigned char> omm_data(total_data_size);
        const auto *tex_data = static_cast<const unsigned char *>(img->data()) + alpha_offset;
        const size_t tex_stride = num_components;
        const size_t tex_pitch = static_cast<size_t>(img->width()) * num_components;

        for(size_t i = 0; i < omm_count; ++i)
        {
            const uint32_t src = sources[i];
            const float uv0[2] = {uvs[indices[src * 3 + 0] * 2 + 0], uvs[indices[src * 3 + 0] * 2 + 1]};
            const float uv1[2] = {uvs[indices[src * 3 + 1] * 2 + 0], uvs[indices[src * 3 + 1] * 2 + 1]};
            const float uv2[2] = {uvs[indices[src * 3 + 2] * 2 + 0], uvs[indices[src * 3 + 2] * 2 + 1]};
            meshopt_opacityMapRasterize(omm_data.data() + offsets[i], levels[i], params.states, uv0, uv1, uv2,
                                        tex_data, tex_stride, tex_pitch, img->width(), img->height());
        }

        size_t new_omm_count = meshopt_opacityMapCompact(omm_data.data(), omm_data.size(), levels.data(),
                                                          offsets.data(), omm_count, omm_indices.data(),
                                                          num_triangles, params.states);
        if(new_omm_count == 0) { continue; }

        // trim data buffer to actual used size
        const size_t final_data_size =
                offsets[new_omm_count - 1] + meshopt_opacityMapEntrySize(levels[new_omm_count - 1], params.states);
        omm_data.resize(final_data_size);

        const uint16_t vk_format = static_cast<uint16_t>(
                params.states == 2 ? VK_OPACITY_MICROMAP_FORMAT_2_STATE_EXT
                                   : VK_OPACITY_MICROMAP_FORMAT_4_STATE_EXT);

        std::vector<VkMicromapTriangleEXT> triangles(new_omm_count);
        for(size_t i = 0; i < new_omm_count; ++i)
        {
            triangles[i].dataOffset = offsets[i];
            triangles[i].subdivisionLevel = levels[i];
            triangles[i].format = vk_format;
        }

        mesh_omm_entry_t omm_entry;
        omm_entry.data = std::move(omm_data);
        omm_entry.triangles = std::move(triangles);
        omm_entry.indices.assign(omm_indices.begin(), omm_indices.end());

        ret.push_back({entry_idx, color_texture_id, std::move(omm_entry)});
    }
    return ret;
}

model::load_mesh_result_t load_mesh(const load_mesh_params_t &params,
                                    const vierkant::model::model_assets_t &mesh_assets)
{
    model::load_mesh_result_t ret;

    assert(params.device);
    std::vector<vierkant::BufferPtr> staging_buffers;

    // command pool for background transfer
    auto command_pool = vierkant::create_command_pool(params.device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    auto cmd_buf = vierkant::CommandBuffer(params.device, command_pool.get());

    auto mesh_staging_buf = vierkant::Buffer::create(params.device, nullptr, 1U << 20, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                     VMA_MEMORY_USAGE_CPU_ONLY);

    auto create_texture = [device = params.device, cmd_buf_handle = cmd_buf.handle(),
                           &staging_buffers](const crocore::ImagePtr &img) -> vierkant::ImagePtr {
        if(!img) { return nullptr; }

        vierkant::Image::Format fmt;
        fmt.format = vk_format(img);
        fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        fmt.extent = {img->width(), img->height(), 1};
        fmt.address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        fmt.address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        fmt.use_mipmap = true;
        fmt.initial_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        fmt.initial_cmd_buffer = cmd_buf_handle;

        auto vk_img = vierkant::Image::create(device, fmt);
        auto buf = vierkant::Buffer::create(device, img->data(), img->num_bytes(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VMA_MEMORY_USAGE_CPU_ONLY);
        vk_img->copy_from(buf, cmd_buf_handle);
        staging_buffers.push_back(std::move(buf));
        return vk_img;
    };

    cmd_buf.begin();

    vierkant::Mesh::create_info_t mesh_create_info = {};
    mesh_create_info.buffer_usage_flags = params.buffer_flags;
    mesh_create_info.mesh_buffer_params = params.mesh_buffers_params;
    mesh_create_info.command_buffer = cmd_buf.handle();
    mesh_create_info.staging_buffer = mesh_staging_buf;
    ret.mesh = std::visit(
            [&mesh_create_info, &device = params.device](auto &&geometry_data) -> vierkant::MeshPtr {
                using T = std::decay_t<decltype(geometry_data)>;

                if constexpr(std::is_same_v<T, std::vector<vierkant::Mesh::entry_create_info_t>>)
                {
                    return vierkant::Mesh::create_with_entries(device, geometry_data, mesh_create_info);
                }
                else if constexpr(std::is_same_v<T, vierkant::mesh_buffer_bundle_t>)
                {
                    return vierkant::Mesh::create_from_bundle(device, geometry_data, mesh_create_info);
                }
            },
            mesh_assets.geometry_data);

    // skin + bones
    ret.mesh->root_bone = mesh_assets.root_bone;

    // node hierarchy
    ret.mesh->root_node = mesh_assets.root_node;

    // node animations
    ret.mesh->node_animations = mesh_assets.node_animations;

    // lightsource-assets + placed instances
    for(const auto &l: mesh_assets.lights) { ret.lights[l.id] = l; }
    ret.light_instances = mesh_assets.light_instances;

    // generate base-textures under their default-sampler key {texture_id, nil}
    for(const auto &[id, tex_variant]: mesh_assets.textures)
    {
        std::visit(
                [&params, tex_id = id, &ret, &create_texture](auto &&img) {
                    using T = std::decay_t<decltype(img)>;
                    texture_key_t key = {tex_id, vierkant::SamplerId::nil()};

                    if constexpr(std::is_same_v<T, crocore::ImagePtr>) { ret.textures[key] = create_texture(img); }
                    else if constexpr(std::is_same_v<T, vierkant::bcn::compress_result_t>)
                    {
                        vierkant::Image::Format fmt;
                        fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                        fmt.max_anisotropy = params.device->properties().core.limits.maxSamplerAnisotropy;
                        ret.textures[key] = create_compressed_texture(params.device, img, fmt, params.load_queue);
                    }
                },
                tex_variant);
    }

    ret.mesh->material_ids.resize(mesh_assets.materials.size());

    for(uint32_t i = 0; i < mesh_assets.materials.size(); ++i)
    {
        const auto &asset_mat = mesh_assets.materials[i];
        ret.mesh->material_ids[i] = asset_mat.id;

        // no material-mutation: ids stay stable, drawable resolves textures by {texture_id, sampler_id}
        ret.materials[asset_mat.id] = asset_mat;

        for(const auto &[tex_type, tex_data]: asset_mat.texture_data)
        {
            // sampler_id nil -> base image under {texture_id, nil} already exists
            if(!tex_data.sampler_id) { continue; }

            texture_key_t key = {tex_data.texture_id, tex_data.sampler_id};
            if(ret.textures.contains(key)) { continue; }

            auto base_it = ret.textures.find({tex_data.texture_id, vierkant::SamplerId::nil()});
            if(base_it == ret.textures.end()) { continue; }
            auto base_img = base_it->second;

            // get-or-create one VkSampler per SamplerId
            vierkant::VkSamplerPtr vk_sampler;
            if(auto sampler_it = ret.samplers.find(tex_data.sampler_id); sampler_it != ret.samplers.end())
            {
                vk_sampler = sampler_it->second;
            }
            else if(auto desc_it = mesh_assets.texture_samplers.find(tex_data.sampler_id);
                    desc_it != mesh_assets.texture_samplers.end())
            {
                vk_sampler = create_sampler(params.device, desc_it->second, base_img->num_mip_levels());
                ret.samplers[tex_data.sampler_id] = vk_sampler;
            }
            else
            {
                spdlog::warn("material '{}' references sampler '{}', but could not find in bundle", asset_mat.name,
                             tex_data.sampler_id.str());
                continue;
            }

            // realize the sampled permutation under the composite key
            auto vk_img = base_img->clone();
            vk_img->set_sampler(vk_sampler);
            ret.textures[key] = vk_img;
        }
    }

    // OMM: adopt pre-baked bundle data if present (survives texture-compression), else live-bake
    // from CPU images while they are still alive. Either way, stamp the runtime mesh-id here.
    if(!mesh_assets.omm_data.empty())
    {
        for(const auto &d: mesh_assets.omm_data)
        {
            ret.omm_cache[{ret.mesh->id, d.entry_index, d.color_texture_id}] = d.entry;
        }
    }
    else if(params.omm_params)
    {
        if(const auto *bundle = std::get_if<vierkant::mesh_buffer_bundle_t>(&mesh_assets.geometry_data))
        {
            for(auto &d: generate_omm_data(mesh_assets, *bundle, *params.omm_params))
            {
                ret.omm_cache[{ret.mesh->id, d.entry_index, d.color_texture_id}] = std::move(d.entry);
            }
        }
    }

    // submit transfer and sync
    cmd_buf.submit(params.load_queue ? params.load_queue : params.device->queue(), true);
    return ret;
}

vierkant::ImagePtr create_texture(const vierkant::DevicePtr &device, const crocore::ImagePtr &img,
                                  vierkant::Image::Format fmt, VkQueue load_queue)
{
    if(!img) { return nullptr; }

    // adhoc using global pool
    auto pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                              VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    auto command_buffer = vierkant::CommandBuffer(device, pool.get());
    command_buffer.begin();

    fmt.format = vk_format(img);
    fmt.usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    fmt.extent = {img->width(), img->height(), 1};
    fmt.address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    fmt.address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    fmt.use_mipmap = true;
    fmt.initial_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    fmt.initial_cmd_buffer = command_buffer.handle();

    auto vk_img = vierkant::Image::create(device, img->data(), fmt);

    // submit and sync
    command_buffer.submit(load_queue, true);
    return vk_img;
}

// TODO: fix unnecessary blocking, rework with commandbuffer-handle and staging-buffer!?
vierkant::ImagePtr create_compressed_texture(const vierkant::DevicePtr &device,
                                             const vierkant::bcn::compress_result_t &compression_result,
                                             vierkant::Image::Format format, VkQueue load_queue)
{
    // adhoc using global pool
    auto pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                              VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    auto command_buffer = vierkant::CommandBuffer(device, pool.get());
    command_buffer.begin();

    format.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    format.format = compression_result.mode == bcn::BC7 ? VK_FORMAT_BC7_UNORM_BLOCK : VK_FORMAT_BC5_UNORM_BLOCK;
    format.extent = {compression_result.base_width, compression_result.base_height, 1};
    format.address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    format.address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    format.use_mipmap = compression_result.levels.size() > 1;
    format.autogenerate_mipmaps = false;
    format.initial_layout_transition = false;

    auto compressed_img = vierkant::Image::create(device, format);
    std::vector<vierkant::BufferPtr> level_buffers(compression_result.levels.size());
    compressed_img->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, command_buffer.handle());

    for(uint32_t lvl = 0; lvl < compression_result.levels.size(); ++lvl)
    {
        level_buffers[lvl] = vierkant::Buffer::create(device, compression_result.levels[lvl],
                                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
        compressed_img->copy_from(level_buffers[lvl], command_buffer.handle(), 0, {}, {}, 0, lvl);
    }
    compressed_img->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, command_buffer.handle());

    // submit and sync
    command_buffer.submit(load_queue, true);
    return compressed_img;
}

std::optional<model_assets_t> load_model(const std::filesystem::path &path, crocore::ThreadPoolClassic *pool,
                                         const std::string &id_seed)
{
    auto ext_str = path.extension().string();
    std::transform(ext_str.begin(), ext_str.end(), ext_str.begin(), ::tolower);
    if(ext_str == ".gltf" || ext_str == ".glb") { return gltf(path, pool, id_seed); }
    else if(ext_str == ".obj") { return wavefront_obj(path, pool, id_seed); }
    return {};
}

}// namespace vierkant::model