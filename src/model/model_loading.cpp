#include <set>
#include <vierkant/model/gltf.hpp>
#include <vierkant/model/model_loading.hpp>
#include <vierkant/model/wavefront_obj.hpp>

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

    // generate textures with default samplers
    for(const auto &[id, tex_variant]: mesh_assets.textures)
    {
        std::visit(
                [&params, tex_id = id, &ret, &create_texture](auto &&img) {
                    using T = std::decay_t<decltype(img)>;

                    if constexpr(std::is_same_v<T, crocore::ImagePtr>) { ret.textures[tex_id] = create_texture(img); }
                    else if constexpr(std::is_same_v<T, vierkant::bcn::compress_result_t>)
                    {
                        vierkant::Image::Format fmt;
                        fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                        fmt.max_anisotropy = params.device->properties().core.limits.maxSamplerAnisotropy;
                        ret.textures[tex_id] = create_compressed_texture(params.device, img, fmt, params.load_queue);
                    }
                },
                tex_variant);
    }
    ret.mesh->materials.resize(std::max<size_t>(1, mesh_assets.materials.size()));

    for(uint32_t i = 0; i < mesh_assets.materials.size(); ++i)
    {
        const auto &asset_mat = mesh_assets.materials[i];

        auto &material = ret.mesh->materials[i];
        material = vierkant::Material::create();
        material->m = asset_mat;
        material->hash = std::hash<vierkant::material_t>()(asset_mat);

        for(const auto &[tex_type, tex_data]: asset_mat.texture_data)
        {
            auto vk_img = ret.textures[tex_data.texture_id];

            // optional sampler-override
            if(tex_data.sampler_id)
            {
                // clone img
                vk_img = ret.textures[tex_data.texture_id]->clone();
                auto sampler_it = mesh_assets.texture_samplers.find(tex_data.sampler_id);

                if(sampler_it != mesh_assets.texture_samplers.end())
                {
                    auto vk_sampler = create_sampler(params.device, sampler_it->second, vk_img->num_mip_levels());
                    ret.samplers[tex_data.sampler_id] = vk_sampler;
                    vk_img->set_sampler(vk_sampler);
                }
                else
                {
                    spdlog::warn("material '{}' references sampler '{}', but could not find in bundle", asset_mat.name,
                                 tex_data.sampler_id.str());
                }
            }
            material->textures[tex_type] = vk_img;
        }
    }

    // submit transfer and sync
    cmd_buf.submit(params.load_queue ? params.load_queue : params.device->queue(), true);
    return ret;
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

std::optional<model_assets_t> load_model(const std::filesystem::path &path, crocore::ThreadPoolClassic *pool)
{
    auto ext_str = path.extension().string();
    std::transform(ext_str.begin(), ext_str.end(), ext_str.begin(), ::tolower);
    if(ext_str == ".gltf" || ext_str == ".glb") { return gltf(path, pool); }
    else if(ext_str == ".obj") { return wavefront_obj(path, pool); }
    return {};
}

}// namespace vierkant::model