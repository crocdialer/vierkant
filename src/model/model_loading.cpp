//
// Created by crocdialer on 9/17/21.
//

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

std::vector<vierkant::bc7::compress_result_t> create_compressed_images(const mesh_assets_t &mesh_assets)
{
    std::vector<vierkant::bc7::compress_result_t> ret;
    std::set<crocore::ImagePtr> images;

    crocore::ThreadPool threadpool(std::thread::hardware_concurrency());
    std::chrono::milliseconds compress_total_duration(0);
    size_t num_pixels = 0;

    for(const auto &mat: mesh_assets.materials)
    {
        for(const auto &[tex_type, tex_id]: mat.textures)
        {
            if(!mesh_assets.textures.contains(tex_id))
            {
                spdlog::warn("missing texture in mesh-assets: type: {} - id: {}", (uint32_t) tex_type, tex_id.str());
                continue;
            }

            const auto &img = std::get<crocore::ImagePtr>(mesh_assets.textures.at(tex_id));

            if(img && !images.contains(img))
            {
                images.insert(img);
                bc7::compress_info_t compress_info = {};
                compress_info.image = img;
                compress_info.generate_mipmaps = true;
                compress_info.delegate_fn = [&threadpool](auto fn) { return threadpool.post(fn); };
                auto compress_result = bc7::compress(compress_info);
                compress_total_duration += compress_result.duration;
                num_pixels += img->width() * img->height();
                ret.push_back(std::move(compress_result));
            }
        }
    }
    float mpx_per_sec =
            1.e-6f * static_cast<float>(num_pixels) / std::chrono::duration<float>(compress_total_duration).count();
    spdlog::debug("compressed {} images in {} ms - avg. {:03.2f} Mpx/s", images.size(), compress_total_duration.count(),
                  mpx_per_sec);
    return ret;
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
        fmt.max_anisotropy = device->properties().limits.maxSamplerAnisotropy;
        fmt.address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        fmt.address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
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

    mesh->materials.resize(std::max<size_t>(1, mesh_assets.materials.size()));

    std::map<vierkant::TextureId, uint32_t> index_map;
    std::set<vierkant::TextureId> texture_id_set;
    uint32_t idx = 0;

    for(const auto &mat: mesh_assets.materials)
    {
        for(const auto &[tex_type, tex_id]: mat.textures)
        {
            if(tex_id && !texture_id_set.contains(tex_id))
            {
                texture_id_set.insert(tex_id);
                index_map[tex_id] = idx++;
            }
        }
    }

    // TODO: refactor using TextureId
    std::vector<vierkant::bc7::compress_result_t> compressed_images;
    if(asset_bundle && asset_bundle->compressed_images.size() == index_map.size())
    {
        compressed_images = asset_bundle->compressed_images;
    }
    if(params.compress_textures && compressed_images.empty())
    {
        compressed_images = create_compressed_images(mesh_assets);
    }

    // cache textures
    std::unordered_map<vierkant::TextureId, vierkant::ImagePtr> texture_cache;

    auto cache_helper = [&params, &texture_cache, &create_texture, &compressed_images,
                         &index_map](const vierkant::TextureId &tex_id, const crocore::ImagePtr &img) {
        if(tex_id && !texture_cache.count(tex_id))
        {
            if(!params.compress_textures) { texture_cache[tex_id] = create_texture(img); }
            else
            {
                vierkant::Image::Format fmt;
                fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                fmt.max_anisotropy = params.device->properties().limits.maxSamplerAnisotropy;
                fmt.address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                fmt.address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;

                const auto &compress_result = compressed_images[index_map[tex_id]];
                texture_cache[tex_id] =
                        create_compressed_texture(params.device, compress_result, fmt, params.load_queue);
            }
        }
    };

    for(const auto &[tex_id, tex_variant]: mesh_assets.textures)
    {
        try
        {
            auto img = std::get<crocore::ImagePtr>(tex_variant);
            if(img) { cache_helper(tex_id, img); }
        } catch(std::bad_variant_access &e)
        {
            spdlog::error("was expecting an uncompressed image: {}", e.what());
        }
    }

    for(uint32_t i = 0; i < mesh_assets.materials.size(); ++i)
    {
        auto &material = mesh->materials[i];
        material = vierkant::Material::create();

        material->name = mesh_assets.materials[i].name;
        material->color = mesh_assets.materials[i].base_color;
        material->emission = glm::vec4(mesh_assets.materials[i].emission, mesh_assets.materials[i].emissive_strength);
        material->roughness = mesh_assets.materials[i].roughness;
        material->metalness = mesh_assets.materials[i].metalness;
        material->blend_mode = mesh_assets.materials[i].blend_mode;
        material->alpha_cutoff = mesh_assets.materials[i].alpha_cutoff;
        material->two_sided = mesh_assets.materials[i].twosided;

        material->transmission = mesh_assets.materials[i].transmission;
        material->attenuation_color = mesh_assets.materials[i].attenuation_color;
        material->attenuation_distance = mesh_assets.materials[i].attenuation_distance;
        material->ior = mesh_assets.materials[i].ior;

        material->sheen_color = mesh_assets.materials[i].sheen_color;
        material->sheen_roughness = mesh_assets.materials[i].sheen_roughness;

        material->sheen_color = mesh_assets.materials[i].sheen_color;
        material->sheen_roughness = mesh_assets.materials[i].sheen_roughness;

        material->iridescence_factor = mesh_assets.materials[i].iridescence_factor;
        material->iridescence_ior = mesh_assets.materials[i].iridescence_ior;
        material->iridescence_thickness_range = mesh_assets.materials[i].iridescence_thickness_range;

        for(const auto &[tex_type, tex_id] : mesh_assets.materials[i].textures)
        {
            material->textures[tex_type] = texture_cache[tex_id];
        }

        material->texture_transform = mesh_assets.materials[i].texture_transform;
    }

    // submit transfer and sync
    cmd_buf.submit(params.load_queue ? params.load_queue : params.device->queue(), true);
    return mesh;
}

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