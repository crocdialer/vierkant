//
// Created by crocdialer on 1/17/21.
//

#include <vierkant/descriptor.h>

namespace vierkant
{
void add_descriptor_counts(const descriptor_map_t &descriptors, descriptor_count_t &counts)
{
    std::map<VkDescriptorType, uint32_t> mesh_counts;
    for(const auto &pair : descriptors){ mesh_counts[pair.second.type]++; }
    for(const auto &pair : mesh_counts){ counts.push_back(pair); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorPoolPtr create_descriptor_pool(const vierkant::DevicePtr &device,
                                         const descriptor_count_t &counts,
                                         uint32_t max_sets)
{
    std::vector<VkDescriptorPoolSize> pool_sizes;
    for(const auto &pair : counts){ pool_sizes.push_back({pair.first, pair.second}); }

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

    for(const auto &pair : descriptors)
    {
        auto &desc = pair.second;
        VkDescriptorSetLayoutBinding ubo_layout_binding = {};
        ubo_layout_binding.binding = pair.first;
        ubo_layout_binding.descriptorCount = std::max<uint32_t>(1, static_cast<uint32_t>(desc.image_samplers.size()));
        ubo_layout_binding.descriptorType = desc.type;
        ubo_layout_binding.pImmutableSamplers = nullptr;
        ubo_layout_binding.stageFlags = desc.stage_flags;
        bindings.push_back(ubo_layout_binding);
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
    for(const auto &pair : descriptors){ num_writes += std::max<size_t>(1, pair.second.image_samplers.size()); }

    std::vector<VkWriteDescriptorSet> descriptor_writes;

    std::vector<VkDescriptorBufferInfo> buffer_infos;
    buffer_infos.reserve(num_writes);

    // keep all VkDescriptorImageInfo structs around until vkUpdateDescriptorSets has processed them
    std::vector<std::vector<VkDescriptorImageInfo>> image_infos_collection;

    for(const auto &pair : descriptors)
    {
        auto &desc = pair.second;

        VkWriteDescriptorSet desc_write = {};
        desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_write.descriptorType = desc.type;
        desc_write.dstSet = descriptor_set.get();
        desc_write.dstBinding = pair.first;
        desc_write.dstArrayElement = 0;
        desc_write.descriptorCount = 1;

        if(desc.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        {
            VkDescriptorBufferInfo buffer_info = {};
            buffer_info.buffer = desc.buffer->handle();
            buffer_info.offset = desc.buffer_offset;
            buffer_info.range = desc.buffer->num_bytes() - desc.buffer_offset;
            buffer_infos.push_back(buffer_info);
            desc_write.pBufferInfo = &buffer_infos.back();
        }
        else if(desc.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            std::vector<VkDescriptorImageInfo> image_infos(desc.image_samplers.size());

            for(uint32_t j = 0; j < desc.image_samplers.size(); ++j)
            {
                const auto &img = desc.image_samplers[j];
                image_infos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;//img->image_layout();
                image_infos[j].imageView = img->image_view();
                image_infos[j].sampler = img->sampler();
            }
            desc_write.descriptorCount = static_cast<uint32_t>(image_infos.size());
            desc_write.pImageInfo = image_infos.data();
            image_infos_collection.push_back(std::move(image_infos));
        }
        descriptor_writes.push_back(desc_write);
    }

    // write all descriptors
    vkUpdateDescriptorSets(device->handle(), descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool descriptor_t::operator==(const descriptor_t &other) const
{
    if(type != other.type){ return false; }
    if(stage_flags != other.stage_flags){ return false; }
    if(buffer != other.buffer){ return false; }
    if(buffer_offset != other.buffer_offset){ return false; }
    if(image_samplers != other.image_samplers){ return false; }
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
    hash_combine(h, descriptor.buffer);
    hash_combine(h, descriptor.buffer_offset);
    for(const auto &img : descriptor.image_samplers){ hash_combine(h, img); }
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
