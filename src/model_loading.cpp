//
// Created by crocdialer on 9/17/21.
//

#include <vierkant/model_loading.hpp>

namespace vierkant::model
{

VkFormat vk_format(const crocore::ImagePtr &img)
{
    VkFormat ret = VK_FORMAT_UNDEFINED;

    switch(img->num_components())
    {
        case 1:
            ret = VK_FORMAT_R8_UNORM;
            break;
        case 2:
            ret = VK_FORMAT_R8G8_UNORM;
            break;
        case 3:
            ret = VK_FORMAT_R8G8B8_UNORM;
            break;
        case 4:
            ret = VK_FORMAT_R8G8B8A8_UNORM;
            break;
    }
    return ret;
}

vierkant::MeshPtr load_mesh(const vierkant::DevicePtr &device,
                            const vierkant::model::mesh_assets_t &mesh_assets,
                            VkQueue load_queue,
                            VkBufferUsageFlags buffer_flags)
{
    vierkant::Mesh::create_info_t mesh_create_info = {};
    mesh_create_info.buffer_usage_flags = buffer_flags;
    auto mesh = vierkant::Mesh::create_with_entries(device, mesh_assets.entry_create_infos, mesh_create_info);

    std::vector<vierkant::BufferPtr> staging_buffers;

    // command pool for background transfer
    auto command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    auto cmd_buf = vierkant::CommandBuffer(device, command_pool.get());

    auto create_texture = [device = device, cmd_buf_handle = cmd_buf.handle(), &staging_buffers](
            const crocore::ImagePtr &img) -> vierkant::ImagePtr
    {
        if(!img){ return nullptr; }

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

        auto vk_img = vierkant::Image::create(device, nullptr, fmt);
        auto buf = vierkant::Buffer::create(device, img->data(), img->num_bytes(),
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
        vk_img->copy_from(buf, cmd_buf_handle);
        staging_buffers.push_back(std::move(buf));
        return vk_img;
    };

    cmd_buf.begin();

    // skin + bones
    mesh->root_bone = mesh_assets.root_bone;

    // node hierarchy
    mesh->root_node = mesh_assets.root_node;

    // node animations
    mesh->node_animations = mesh_assets.node_animations;

    mesh->materials.resize(std::max<size_t>(1, mesh_assets.materials.size()));

    // cache textures
    std::unordered_map<crocore::ImagePtr, vierkant::ImagePtr> texture_cache;
    auto cache_helper = [&texture_cache, &create_texture](const crocore::ImagePtr &img)
    {
        if(img && !texture_cache.count(img)){ texture_cache[img] = create_texture(img); }
    };

    for(const auto &asset_mat : mesh_assets.materials)
    {
        cache_helper(asset_mat.img_diffuse);
        cache_helper(asset_mat.img_emission);
        cache_helper(asset_mat.img_normals);
        cache_helper(asset_mat.img_ao_roughness_metal);
        cache_helper(asset_mat.img_thickness);
        cache_helper(asset_mat.img_transmission);
    }

    for(uint32_t i = 0; i < mesh_assets.materials.size(); ++i)
    {
        auto &material = mesh->materials[i];
        material = vierkant::Material::create();

        material->name = mesh_assets.materials[i].name;
        material->color = mesh_assets.materials[i].base_color;
        material->emission = mesh_assets.materials[i].emission;
        material->roughness = mesh_assets.materials[i].roughness;
        material->metalness = mesh_assets.materials[i].metalness;
        material->blend_mode = mesh_assets.materials[i].blend_mode;
        material->alpha_cutoff = mesh_assets.materials[i].alpha_cutoff;
        material->two_sided = mesh_assets.materials[i].twosided;

        material->transmission = mesh_assets.materials[i].transmission;
        material->attenuation_color = mesh_assets.materials[i].attenuation_color;
        material->attenuation_distance = mesh_assets.materials[i].attenuation_distance;
        material->ior = mesh_assets.materials[i].ior;

        auto color_img = mesh_assets.materials[i].img_diffuse;
        auto emmission_img = mesh_assets.materials[i].img_emission;
        auto normal_img = mesh_assets.materials[i].img_normals;
        auto ao_rough_metal_img = mesh_assets.materials[i].img_ao_roughness_metal;
        auto thickness_img = mesh_assets.materials[i].img_thickness;
        auto transmission_img = mesh_assets.materials[i].img_transmission;

        if(color_img){ material->textures[vierkant::Material::Color] = texture_cache[color_img]; }
        if(emmission_img){ material->textures[vierkant::Material::Emission] = texture_cache[emmission_img]; }
        if(normal_img){ material->textures[vierkant::Material::Normal] = texture_cache[normal_img]; }

        if(ao_rough_metal_img)
        {
            material->textures[vierkant::Material::Ao_rough_metal] = texture_cache[ao_rough_metal_img];
        }
        if(transmission_img){ material->textures[vierkant::Material::Transmission] = texture_cache[transmission_img]; }
        if(thickness_img){ material->textures[vierkant::Material::Thickness] = texture_cache[thickness_img]; }
    }

    // submit transfer and sync
    cmd_buf.submit(load_queue, true);

    return mesh;
}

}// namespace vierkant::model