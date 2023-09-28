//
// Created by crocdialer on 9/17/21.
//

#define VK_NO_PROTOTYPES
#include <volk.h>

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

VkSamplerAddressMode vk_sampler_address_mode(const vierkant::model::texture_sampler_t::AddressMode &address_mode)
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

VkFilter vk_filter(const vierkant::model::texture_sampler_t::Filter &filter)
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

vierkant::VkSamplerPtr create_sampler(const vierkant::DevicePtr &device, const vierkant::model::texture_sampler_t &ts,
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
    sampler_create_info.maxAnisotropy = device->properties().limits.maxSamplerAnisotropy;

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

bool compress_textures(vierkant::model::mesh_assets_t &mesh_assets)
{
    std::chrono::milliseconds compress_total_duration(0);
    crocore::ThreadPool threadpool(std::thread::hardware_concurrency());
    size_t num_pixels = 0;

    for(auto &[tex_id, texture_variant]: mesh_assets.textures)
    {
        try
        {
            texture_variant = std::visit(
                    [&threadpool, &compress_total_duration, &num_pixels](auto &&img) -> texture_variant_t {
                        using T = std::decay_t<decltype(img)>;

                        if constexpr(std::is_same_v<T, crocore::ImagePtr>)
                        {
                            if(img)
                            {
                                bc7::compress_info_t compress_info = {};
                                compress_info.image = img;
                                compress_info.generate_mipmaps = true;
                                compress_info.delegate_fn = [&threadpool](auto fn) { return threadpool.post(fn); };
                                auto compressed_img = bc7::compress(compress_info);
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

vierkant::MeshPtr load_mesh(const load_mesh_params_t &params, const vierkant::model::mesh_assets_t &mesh_assets,
                            const std::optional<asset_bundle_t> &asset_bundle)
{
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
    auto mesh = asset_bundle ? vierkant::Mesh::create_from_bundle(params.device, asset_bundle->mesh_buffer_bundle,
                                                                  mesh_create_info)
                             : vierkant::Mesh::create_with_entries(params.device, mesh_assets.entry_create_infos,
                                                                   mesh_create_info);

    // skin + bones
    mesh->root_bone = mesh_assets.root_bone;

    // node hierarchy
    mesh->root_node = mesh_assets.root_node;

    // node animations
    mesh->node_animations = mesh_assets.node_animations;

    // check if we need to override materials/textures with our asset-bundle
    const auto &textures = (asset_bundle && asset_bundle->textures.size() == mesh_assets.textures.size())
                                   ? asset_bundle->textures
                                   : mesh_assets.textures;

    const auto &materials = (asset_bundle && asset_bundle->materials.size() == mesh_assets.materials.size())
                                    ? asset_bundle->materials
                                    : mesh_assets.materials;

    // create + cache textures & samplers
    std::unordered_map<vierkant::TextureSourceId, vierkant::ImagePtr> texture_cache;
    std::unordered_map<vierkant::SamplerId, vierkant::VkSamplerPtr> sampler_cache;

    // generate textures with default samplers
    for(const auto &[tex_id, tex_variant]: textures)
    {
        std::visit(
                [&params, tex_id = tex_id, &texture_cache, &create_texture](auto &&img) {
                    using T = std::decay_t<decltype(img)>;

                    if constexpr(std::is_same_v<T, crocore::ImagePtr>) { texture_cache[tex_id] = create_texture(img); }
                    else if constexpr(std::is_same_v<T, vierkant::bc7::compress_result_t>)
                    {
                        vierkant::Image::Format fmt;
                        fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                        fmt.max_anisotropy = params.device->properties().limits.maxSamplerAnisotropy;
                        texture_cache[tex_id] = create_compressed_texture(params.device, img, fmt, params.load_queue);
                    }
                },
                tex_variant);
    }
    mesh->materials.resize(std::max<size_t>(1, materials.size()));

    for(uint32_t i = 0; i < materials.size(); ++i)
    {
        auto &material = mesh->materials[i];
        material = vierkant::Material::create();

        material->name = materials[i].name;
        material->color = materials[i].base_color;
        material->emission = glm::vec4(materials[i].emission, materials[i].emissive_strength);
        material->roughness = materials[i].roughness;
        material->metalness = materials[i].metalness;
        material->blend_mode = materials[i].blend_mode;
        material->alpha_cutoff = materials[i].alpha_cutoff;
        material->two_sided = materials[i].twosided;

        material->transmission = materials[i].transmission;
        material->attenuation_color = materials[i].attenuation_color;
        material->attenuation_distance = materials[i].attenuation_distance;
        material->ior = materials[i].ior;

        material->sheen_color = materials[i].sheen_color;
        material->sheen_roughness = materials[i].sheen_roughness;

        material->sheen_color = materials[i].sheen_color;
        material->sheen_roughness = materials[i].sheen_roughness;

        material->iridescence_factor = materials[i].iridescence_factor;
        material->iridescence_ior = materials[i].iridescence_ior;
        material->iridescence_thickness_range = materials[i].iridescence_thickness_range;

        for(const auto &[tex_type, tex_id]: materials[i].textures)
        {
            auto vk_img = texture_cache[tex_id];

            // optional sampler-override
            auto sampler_id_it = materials[i].samplers.find(tex_type);
            if(sampler_id_it != materials[i].samplers.end())
            {
                // clone img
                vk_img = texture_cache[tex_id]->clone();
                const auto &sampler_id = sampler_id_it->second;
                assert(mesh_assets.texture_samplers.contains(sampler_id));
                auto vk_sampler = create_sampler(params.device, mesh_assets.texture_samplers.at(sampler_id),
                                                 vk_img->num_mip_levels());
                vk_img->set_sampler(vk_sampler);
            }
            material->textures[tex_type] = vk_img;
        }

        material->texture_transform = materials[i].texture_transform;
    }

    // submit transfer and sync
    cmd_buf.submit(params.load_queue ? params.load_queue : params.device->queue(), true);
    return mesh;
}

// TODO: fix unnecessary blocking, rework with commandbuffer-handle and staging-buffer!?
vierkant::ImagePtr create_compressed_texture(const vierkant::DevicePtr &device,
                                             const vierkant::bc7::compress_result_t &compression_result,
                                             vierkant::Image::Format format, VkQueue load_queue)
{
    // adhoc using global pool
    auto pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                              VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    auto command_buffer = vierkant::CommandBuffer(device, pool.get());
    command_buffer.begin();

    format.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    format.format = VK_FORMAT_BC7_UNORM_BLOCK;
    format.extent = {compression_result.base_width, compression_result.base_height, 1};
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

std::optional<mesh_assets_t> load_model(const std::filesystem::path &path, crocore::ThreadPool *pool)
{
    auto ext_str = path.extension().string();
    std::transform(ext_str.begin(), ext_str.end(), ext_str.begin(), ::tolower);
    if(ext_str == ".gltf" || ext_str == ".glb") { return gltf(path, pool); }
    else if(ext_str == ".obj") { return wavefront_obj(path, pool); }
    return {};
}

}// namespace vierkant::model