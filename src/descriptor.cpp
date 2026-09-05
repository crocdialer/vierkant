#include <vierkant/descriptor.hpp>
#include <vierkant/hash.hpp>

namespace vierkant
{

constexpr uint32_t g_max_bindless_resources = 512;

uint32_t max_bindless_resources(const vierkant::DevicePtr &device)
{
    const auto &limits = device->properties().vulkan12;
    return std::min({g_max_bindless_resources, limits.maxDescriptorSetUpdateAfterBindSampledImages,
                     limits.maxPerStageDescriptorUpdateAfterBindSampledImages});
}

///////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorPoolPtr create_descriptor_pool(const vierkant::DevicePtr &device, const descriptor_count_t &counts,
                                         uint32_t max_sets)
{
    std::vector<VkDescriptorPoolSize> pool_sizes;
    for(const auto &[type, count]: counts) { pool_sizes.push_back({type, count}); }

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = max_sets;
    pool_info.flags =
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

    VkDescriptorPoolInlineUniformBlockCreateInfo inline_uniform_block_info = {};
    if(auto inline_desc_it = counts.find(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK); inline_desc_it != counts.end())
    {
        uint32_t num_inline_uniform_bytes = inline_desc_it->second;
        num_inline_uniform_bytes =
                std::min(num_inline_uniform_bytes, device->properties().vulkan13.maxInlineUniformTotalSize);
        inline_uniform_block_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO;
        inline_uniform_block_info.maxInlineUniformBlockBindings = num_inline_uniform_bytes;
        pool_info.pNext = &inline_uniform_block_info;
    }

    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    vkCheck(vkCreateDescriptorPool(device->handle(), &pool_info, nullptr, &descriptor_pool),
            "failed to create descriptor pool!");
    return {descriptor_pool, [device](VkDescriptorPool p) { vkDestroyDescriptorPool(device->handle(), p, nullptr); }};
}


///////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorSetLayoutPtr create_descriptor_set_layout(const vierkant::DevicePtr &device,
                                                    const descriptor_map_t &descriptors)
{
    constexpr VkDescriptorBindingFlags default_flags =
            VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    constexpr VkDescriptorBindingFlags bindless_flags = default_flags | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<VkDescriptorBindingFlags> flags_array;

    for(const auto &[binding, desc]: descriptors)
    {
        VkDescriptorSetLayoutBinding layout_binding = {};
        layout_binding.binding = binding;
        layout_binding.descriptorCount = std::max<uint32_t>(1, static_cast<uint32_t>(desc.images.size()));
        layout_binding.descriptorCount =
                std::max<uint32_t>(layout_binding.descriptorCount, static_cast<uint32_t>(desc.buffers.size()));
        layout_binding.descriptorCount = std::max<uint32_t>(layout_binding.descriptorCount,
                                                            static_cast<uint32_t>(desc.inline_uniform_block.size()));
        layout_binding.descriptorCount =
                desc.variable_count ? max_bindless_resources(device) : layout_binding.descriptorCount;
        layout_binding.descriptorType = desc.type;
        layout_binding.pImmutableSamplers = nullptr;
        layout_binding.stageFlags = desc.stage_flags;
        bindings.push_back(layout_binding);
        flags_array.push_back(desc.variable_count ? bindless_flags : default_flags);
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info = {};
    flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flags_info.bindingCount = flags_array.size();
    flags_info.pBindingFlags = flags_array.data();

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.pNext = &flags_info;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();
    layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    vkCheck(vkCreateDescriptorSetLayout(device->handle(), &layout_info, nullptr, &descriptor_set_layout),
            "failed to create descriptor set layout!");

    return {descriptor_set_layout,
            [device](VkDescriptorSetLayout dl) { vkDestroyDescriptorSetLayout(device->handle(), dl, nullptr); }};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorSetPtr create_descriptor_set(const vierkant::DevicePtr &device, const DescriptorPoolPtr &pool,
                                       VkDescriptorSetLayout set_layout, uint32_t variable_count)
{
    VkDescriptorSet descriptor_set;

    // only allocate what is actually in use, the layout still declares max_bindless_resources
    uint32_t num_descriptors = std::min(variable_count, max_bindless_resources(device));

    VkDescriptorSetVariableDescriptorCountAllocateInfo descriptor_count_allocate_info = {};
    descriptor_count_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    descriptor_count_allocate_info.descriptorSetCount = 1;
    descriptor_count_allocate_info.pDescriptorCounts = &num_descriptors;

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = variable_count ? &descriptor_count_allocate_info : nullptr;
    alloc_info.descriptorPool = pool.get();
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &set_layout;

    spdlog::trace("create_descriptor_set - variable_count: {}", variable_count);

    vkCheck(vkAllocateDescriptorSets(device->handle(), &alloc_info, &descriptor_set),
            "failed to allocate descriptor sets!");

    return {descriptor_set,
            [device, pool](VkDescriptorSet s) { vkFreeDescriptorSets(device->handle(), pool.get(), 1, &s); }};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void update_descriptor_set(const vierkant::DevicePtr &device, const descriptor_map_t &descriptors,
                           const DescriptorSetPtr &descriptor_set)
{
    std::vector<VkWriteDescriptorSet> descriptor_writes;

    // keep buffer_infos for vkUpdateDescriptorSets
    std::vector<std::vector<VkDescriptorBufferInfo>> buffer_infos_collection;

    // keep all image_infos for vkUpdateDescriptorSets
    std::vector<std::vector<VkDescriptorImageInfo>> image_infos_collection;

    // keep acceleration_structure_info + handle-ptr for vkUpdateDescriptorSets
    struct acceleration_write_asset_t
    {
        VkWriteDescriptorSetAccelerationStructureKHR writeDescriptorSetAccelerationStructure = {};
        std::unique_ptr<VkAccelerationStructureKHR[]> handles;
    };
    std::vector<acceleration_write_asset_t> acceleration_write_assets;
    std::vector<VkWriteDescriptorSetInlineUniformBlock> inline_uniform_write_assets;

    for(const auto &[binding, desc]: descriptors)
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

                auto buffer_ranges = desc.buffer_ranges;
                buffer_ranges.resize(desc.buffers.size());

                for(uint32_t j = 0; j < desc.buffers.size(); ++j)
                {
                    const auto &buf = desc.buffers[j];

                    if(buf && buf->handle())
                    {
                        VkDescriptorBufferInfo buffer_info = {};
                        buffer_info.buffer = buf->handle();
                        buffer_info.offset = buffer_ranges[j].offset;
                        buffer_info.range = buffer_ranges[j].range ? buffer_ranges[j].range
                                                                   : buf->num_bytes() - buffer_ranges[j].offset;
                        buffer_infos.push_back(buffer_info);
                    }
                }
                desc_write.descriptorCount = static_cast<uint32_t>(buffer_infos.size());
                desc_write.pBufferInfo = buffer_infos.data();
                buffer_infos_collection.push_back(std::move(buffer_infos));
            }
            break;

            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            {
                std::vector<VkDescriptorImageInfo> image_infos;
                image_infos.reserve(desc.images.size());

                for(uint32_t j = 0; j < desc.images.size(); ++j)
                {
                    const auto &img = desc.images[j];

                    if(img)
                    {
                        const auto &image_view = j < desc.image_views.size() ? desc.image_views[j] : img->image_view();

                        VkDescriptorImageInfo img_info = {};
                        img_info.imageLayout = img->image_layout();

                        if(img_info.imageLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
                        {
                            img_info.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
                        }

                        img_info.imageView = image_view;
                        img_info.sampler = img->sampler().get();
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
                assert(acceleration_write_assets.empty());
                acceleration_write_assets.push_back({});
                auto &acceleration_write_asset = acceleration_write_assets.back();
                acceleration_write_asset.handles =
                        std::make_unique<VkAccelerationStructureKHR[]>(desc.acceleration_structures.size());

                for(uint32_t i = 0; i < desc.acceleration_structures.size(); ++i)
                {
                    acceleration_write_asset.handles[i] = desc.acceleration_structures[i].get();
                }

                auto &acceleration_write_info = acceleration_write_asset.writeDescriptorSetAccelerationStructure;
                acceleration_write_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                acceleration_write_info.pNext = nullptr;
                acceleration_write_info.accelerationStructureCount =
                        static_cast<uint32_t>(desc.acceleration_structures.size());
                acceleration_write_info.pAccelerationStructures = acceleration_write_asset.handles.get();

                desc_write.descriptorCount = static_cast<uint32_t>(desc.acceleration_structures.size());
                desc_write.pNext = &acceleration_write_asset;
            }
            break;

            case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
            {
                assert(inline_uniform_write_assets.empty());
                inline_uniform_write_assets.push_back({});
                auto &write_inline_block = inline_uniform_write_assets.back();
                write_inline_block.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK;
                write_inline_block.pNext = nullptr;
                write_inline_block.pData = desc.inline_uniform_block.data();
                write_inline_block.dataSize = desc.inline_uniform_block.size();

                desc_write.descriptorCount = static_cast<uint32_t>(desc.inline_uniform_block.size());
                desc_write.pNext = &write_inline_block;
                break;
            }
            default:
                throw std::runtime_error("update_descriptor_set: unsupported descriptor-type -> " +
                                         std::to_string(desc.type));
        }
        if(desc_write.descriptorCount) { descriptor_writes.push_back(desc_write); }
    }

    // write all descriptors
    vkUpdateDescriptorSets(device->handle(), descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorSetLayoutPtr find_or_create_set_layout(const vierkant::DevicePtr &device, descriptor_map_t descriptors,
                                                 std::unordered_map<descriptor_map_t, DescriptorSetLayoutPtr> &current,
                                                 std::unordered_map<descriptor_map_t, DescriptorSetLayoutPtr> &next)
{
    // clean descriptor-map to enable sharing
    for(auto &[binding, descriptor]: descriptors)
    {
        // a variable-count binding is sized by device-limits, not by the array -> drop it from the key
        if(descriptor.variable_count)
        {
            descriptor.images.clear();
            descriptor.buffers.clear();
            descriptor.acceleration_structures.clear();
            continue;
        }
        for(auto &img: descriptor.images) { img.reset(); }
        for(auto &buf: descriptor.buffers) { buf.reset(); }
        for(auto &as: descriptor.acceleration_structures) { as.reset(); }
        memset(descriptor.inline_uniform_block.data(), 0, descriptor.inline_uniform_block.size());
    }

    // retrieve set-layout
    auto set_it = current.find(descriptors);

    if(set_it != current.end())
    {
        auto new_it = next.insert(std::move(*set_it)).first;
        current.erase(set_it);
        set_it = new_it;
    }
    else
    {
        set_it = next.find(descriptors);
    }

    // not found -> create and insert descriptor-set layout
    if(set_it == next.end())
    {
        auto new_set = vierkant::create_descriptor_set_layout(device, descriptors);
        set_it = next.insert(std::make_pair(std::move(descriptors), std::move(new_set))).first;
    }
    return set_it->second;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorSetPtr find_or_create_descriptor_set(const vierkant::DevicePtr &device, VkDescriptorSetLayout set_layout,
                                               const descriptor_map_t &descriptors,
                                               const vierkant::DescriptorPoolPtr &pool, descriptor_set_map_t &last,
                                               descriptor_set_map_t &current, bool variable_count, bool relax_reuse)
{
    auto descriptors_copy = descriptors;

    // number of descriptors to allocate for a variable-count binding. the set-cache keys on array-sizes,
    // so a set is only ever reused for an identical count.
    uint32_t num_variable_descriptors = 0;

    if(variable_count)
    {
        for(const auto &[binding, descriptor]: descriptors)
        {
            if(descriptor.variable_count)
            {
                num_variable_descriptors = std::max<uint32_t>(descriptor.images.size(), descriptor.buffers.size());
            }
        }
        num_variable_descriptors = std::max(1u, num_variable_descriptors);
    }

    if(relax_reuse)
    {
        // clean descriptor-map to enable sharing
        for(auto &[binding, descriptor]: descriptors_copy)
        {
            for(auto &img: descriptor.images) { img.reset(); }
            for(auto &buf: descriptor.buffers) { buf.reset(); }
            for(auto &as: descriptor.acceleration_structures) { as.reset(); }
            descriptor.inline_uniform_block.clear();
        }
    }

    // handle for a descriptor-set
    DescriptorSetPtr ret;

    // search/create descriptor set

    // start searching in next_assets
    auto descriptor_set_it = current.find(descriptors_copy);

    // not found in current assets
    if(descriptor_set_it == current.end())
    {
        // search in last assets (might already been processed for this frame)
        auto current_assets_it = last.find(descriptors_copy);

        // not found in last assets
        if(current_assets_it == last.end())
        {
            // create a new descriptor set
            ret = vierkant::create_descriptor_set(device, pool, set_layout, num_variable_descriptors);
        }
        else
        {
            // use existing descriptor set
            ret = std::move(current_assets_it->second);
            last.erase(current_assets_it);
        }

        // update the descriptor set
        vierkant::update_descriptor_set(device, descriptors, ret);

        // insert all created assets and store in map
        current[descriptors_copy] = ret;
    }
    else
    {
        ret = descriptor_set_it->second;
    }
    return ret;
}

}//namespace vierkant

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using vierkant::hash_combine;

size_t std::hash<vierkant::descriptor_t>::operator()(const vierkant::descriptor_t &descriptor) const
{
    size_t h = 0;
    hash_combine(h, descriptor.type);
    hash_combine(h, descriptor.stage_flags);
    hash_combine(h, descriptor.variable_count);
    for(const auto &buf: descriptor.buffers) { hash_combine(h, buf); }
    for(const auto &[offset, range]: descriptor.buffer_ranges)
    {
        hash_combine(h, offset);
        hash_combine(h, range);
    }
    for(const auto &img: descriptor.images) { hash_combine(h, img); }
    for(const auto &s: descriptor.image_views) { hash_combine(h, s); }
    for(const auto &as: descriptor.acceleration_structures) { hash_combine(h, as); }
    hash_combine(h,
                 vierkant::hash_range(descriptor.inline_uniform_block.begin(), descriptor.inline_uniform_block.end()));
    return h;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

size_t std::hash<vierkant::descriptor_map_t>::operator()(const vierkant::descriptor_map_t &map) const
{
    size_t h = 0;

    for(auto &[binding, descriptor]: map)
    {
        hash_combine(h, binding);
        hash_combine(h, descriptor);
    }
    return h;
}
