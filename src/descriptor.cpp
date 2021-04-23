//
// Created by crocdialer on 1/17/21.
//

#include <vierkant/descriptor.hpp>

namespace vierkant
{

///////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorPoolPtr create_descriptor_pool(const vierkant::DevicePtr &device,
                                         const descriptor_count_t &counts,
                                         uint32_t max_sets)
{
    std::vector<VkDescriptorPoolSize> pool_sizes;
    for(const auto &[type, count] : counts){ pool_sizes.push_back({type, count}); }

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = max_sets;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    vkCheck(vkCreateDescriptorPool(device->handle(), &pool_info, nullptr, &descriptor_pool),
            "failed to create descriptor pool!");
    return DescriptorPoolPtr(descriptor_pool, [device](VkDescriptorPool p)
    {
        vkDestroyDescriptorPool(device->handle(), p, nullptr);
    });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorSetLayoutPtr create_descriptor_set_layout(const vierkant::DevicePtr &device,
                                                    const descriptor_map_t &descriptors)
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for(const auto &[binding, desc] : descriptors)
    {
        VkDescriptorSetLayoutBinding layout_binding = {};
        layout_binding.binding = binding;
        layout_binding.descriptorCount = std::max<uint32_t>(1, static_cast<uint32_t>(desc.image_samplers.size()));
        layout_binding.descriptorCount = std::max<uint32_t>(layout_binding.descriptorCount,
                                                            static_cast<uint32_t>(desc.buffers.size()));
        layout_binding.descriptorType = desc.type;
        layout_binding.pImmutableSamplers = nullptr;
        layout_binding.stageFlags = desc.stage_flags;
        bindings.push_back(layout_binding);
    }
    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    vkCheck(vkCreateDescriptorSetLayout(device->handle(), &layout_info, nullptr, &descriptor_set_layout),
            "failed to create descriptor set layout!");

    return DescriptorSetLayoutPtr(descriptor_set_layout, [device](VkDescriptorSetLayout dl)
    {
        vkDestroyDescriptorSetLayout(device->handle(), dl, nullptr);
    });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorSetPtr create_descriptor_set(const vierkant::DevicePtr &device,
                                       const DescriptorPoolPtr &pool,
                                       const DescriptorSetLayoutPtr &layout)
{
    VkDescriptorSet descriptor_set;
    VkDescriptorSetLayout layout_handle = layout.get();

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = pool.get();
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout_handle;

    vkCheck(vkAllocateDescriptorSets(device->handle(), &alloc_info, &descriptor_set),
            "failed to allocate descriptor sets!");

    return DescriptorSetPtr(descriptor_set, [device, pool](VkDescriptorSet s)
    {
        vkFreeDescriptorSets(device->handle(), pool.get(), 1, &s);
    });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void update_descriptor_set(const vierkant::DevicePtr &device, const DescriptorSetPtr &descriptor_set,
                           const descriptor_map_t &descriptors)
{
    size_t num_writes = 0;
    for(const auto &[binding, descriptor] : descriptors)
    {
        size_t count = 1;
        count = std::max<size_t>(count, descriptor.image_samplers.size());
        count = std::max<size_t>(count, descriptor.buffers.size());
        num_writes += count;
    }

    std::vector<VkWriteDescriptorSet> descriptor_writes;

    // keep buffer_infos for vkUpdateDescriptorSets
    std::vector<std::vector<VkDescriptorBufferInfo>> buffer_infos_collection;

    // keep all image_infos for vkUpdateDescriptorSets
    std::vector<std::vector<VkDescriptorImageInfo>> image_infos_collection;

    // keep acceleration_structure_info + handle-ptr for vkUpdateDescriptorSets
    struct acceleration_write_asset_t
    {
        VkWriteDescriptorSetAccelerationStructureKHR writeDescriptorSetAccelerationStructure = {};
        VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    };
    std::vector<acceleration_write_asset_t> acceleration_write_assets;
    acceleration_write_assets.reserve(num_writes);

    for(const auto &[binding, desc] : descriptors)
    {

        VkWriteDescriptorSet desc_write = {};
        desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_write.descriptorType = desc.type;
        desc_write.dstSet = descriptor_set.get();
        desc_write.dstBinding = binding;
        desc_write.dstArrayElement = 0;
        desc_write.descriptorCount = 1;

        switch(desc.type)
        {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            {
                std::vector<VkDescriptorBufferInfo> buffer_infos;
                buffer_infos.reserve(desc.buffers.size());

                auto offsets = desc.buffer_offsets;
                offsets.resize(desc.buffers.size(), 0);

                for(uint32_t j = 0; j < desc.buffers.size(); ++j)
                {
                    const auto &buf = desc.buffers[j];

                    if(buf)
                    {
                        VkDescriptorBufferInfo buffer_info = {};
                        buffer_info.buffer = buf->handle();
                        buffer_info.offset = offsets[j];
                        buffer_info.range = buf->num_bytes() - offsets[j];
                        buffer_infos.push_back(buffer_info);
                    }
                }
                desc_write.descriptorCount = static_cast<uint32_t>(buffer_infos.size());
                desc_write.pBufferInfo = buffer_infos.data();
                buffer_infos_collection.push_back(std::move(buffer_infos));
            }
                break;

            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            {
                std::vector<VkDescriptorImageInfo> image_infos;
                image_infos.reserve(desc.image_samplers.size());

                for(const auto &img : desc.image_samplers)
                {
                    if(img)
                    {
                        VkDescriptorImageInfo img_info = {};
                        img_info.imageLayout = img->image_layout();
                        img_info.imageView = img->image_view();
                        img_info.sampler = img->sampler();
                        image_infos.push_back(img_info);
                    }
                }
                desc_write.descriptorCount = static_cast<uint32_t>(image_infos.size());
                desc_write.pImageInfo = image_infos.data();
                image_infos_collection.push_back(std::move(image_infos));
            }
                break;

            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            {
                acceleration_write_asset_t acceleration_write_asset = {};
                acceleration_write_asset.handle = desc.acceleration_structure.get();

                auto &acceleration_write_info = acceleration_write_asset.writeDescriptorSetAccelerationStructure;
                acceleration_write_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                acceleration_write_info.accelerationStructureCount = 1;
                acceleration_write_info.pAccelerationStructures = &acceleration_write_asset.handle;

                acceleration_write_assets.push_back(acceleration_write_asset);
                desc_write.pNext = &acceleration_write_assets.back().writeDescriptorSetAccelerationStructure;
            }
                break;

            default:
                throw std::runtime_error("update_descriptor_set: unsupported descriptor-type -> " +
                                         std::to_string(desc.type));
        }
        if(desc_write.descriptorCount){ descriptor_writes.push_back(desc_write); }
    }

    // write all descriptors
    vkUpdateDescriptorSets(device->handle(), descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorSetLayoutPtr find_set_layout(const vierkant::DevicePtr &device,
                                       descriptor_map_t descriptors,
                                       std::unordered_map<descriptor_map_t, DescriptorSetLayoutPtr> &layout_map)
{
    // clean descriptor-map to enable sharing
    for(auto &[binding, descriptor] : descriptors)
    {
        for(auto &img : descriptor.image_samplers){ img.reset(); }
        for(auto &buf : descriptor.buffers){ buf.reset(); }
        descriptor.acceleration_structure.reset();
    }

    // retrieve set-layout
    auto set_it = layout_map.find(descriptors);

    // not found -> create and insert descriptor-set layout
    if(set_it == layout_map.end())
    {
        auto new_set = vierkant::create_descriptor_set_layout(device, descriptors);
        set_it = layout_map.insert(std::make_pair(std::move(descriptors), std::move(new_set))).first;
    }
    return set_it->second;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool descriptor_t::operator==(const descriptor_t &other) const
{
    if(type != other.type){ return false; }
    if(stage_flags != other.stage_flags){ return false; }
    if(buffers != other.buffers){ return false; }
    if(buffer_offsets != other.buffer_offsets){ return false; }
    if(image_samplers != other.image_samplers){ return false; }
    if(acceleration_structure != other.acceleration_structure){ return false; }
    return true;
}

}//namespace vierkant

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using crocore::hash_combine;

size_t std::hash<vierkant::descriptor_t>::operator()(const vierkant::descriptor_t &descriptor) const
{
    size_t h = 0;
    hash_combine(h, descriptor.type);
    hash_combine(h, descriptor.stage_flags);
    for(const auto &buf : descriptor.buffers){ hash_combine(h, buf); }
    for(const auto &offset : descriptor.buffer_offsets){ hash_combine(h, offset); }
    for(const auto &img : descriptor.image_samplers){ hash_combine(h, img); }
    hash_combine(h, descriptor.acceleration_structure);
    return h;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

size_t std::hash<vierkant::descriptor_map_t>::operator()(const vierkant::descriptor_map_t &map) const
{
    size_t h = 0;

    for(auto &pair : map)
    {
        hash_combine(h, pair.first);
        hash_combine(h, pair.second);
    }
    return h;
}
