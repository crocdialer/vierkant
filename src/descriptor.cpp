//
// Created by crocdialer on 1/17/21.
//

#include <vierkant/descriptor.hpp>
#include <vierkant/hash.hpp>

namespace vierkant
{

constexpr uint32_t g_max_bindless_resources = 512;

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
        layout_binding.descriptorCount =
                desc.variable_count ? g_max_bindless_resources : layout_binding.descriptorCount;
        layout_binding.descriptorType = desc.type;
        layout_binding.pImmutableSamplers = nullptr;
        layout_binding.stageFlags = desc.stage_flags;
        bindings.push_back(layout_binding);
        flags_array.push_back(desc.variable_count ? bindless_flags : default_flags);
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo extended_info = {};
    extended_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    extended_info.bindingCount = flags_array.size();
    extended_info.pBindingFlags = flags_array.data();

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.pNext = &extended_info;
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
                                       const DescriptorSetLayoutPtr &layout, bool variable_count)
{
    VkDescriptorSet descriptor_set;
    VkDescriptorSetLayout layout_handle = layout.get();

    // max allocatable count
    uint32_t max_binding = g_max_bindless_resources - 1;

    VkDescriptorSetVariableDescriptorCountAllocateInfo descriptor_count_allocate_info = {};
    descriptor_count_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    descriptor_count_allocate_info.descriptorSetCount = 1;
    descriptor_count_allocate_info.pDescriptorCounts = &max_binding;

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = variable_count ? &descriptor_count_allocate_info : nullptr;
    alloc_info.descriptorPool = pool.get();
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout_handle;

    spdlog::info("create_descriptor_set - variable_count: {}", variable_count);

    vkCheck(vkAllocateDescriptorSets(device->handle(), &alloc_info, &descriptor_set),
            "failed to allocate descriptor sets!");

    return {descriptor_set,
            [device, pool](VkDescriptorSet s) { vkFreeDescriptorSets(device->handle(), pool.get(), 1, &s); }};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void update_descriptor_set(const vierkant::DevicePtr &device, const DescriptorSetPtr &descriptor_set,
                           const descriptor_map_t &descriptors)
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
        VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    };
    std::vector<acceleration_write_asset_t> acceleration_write_assets;

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
                        auto image_view = j < desc.image_views.size() ? desc.image_views[j] : img->image_view();

                        VkDescriptorImageInfo img_info = {};
                        img_info.imageLayout = img->image_layout();
                        img_info.imageView = image_view;
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
        for(auto &img: descriptor.images) { img.reset(); }
        for(auto &buf: descriptor.buffers) { buf.reset(); }
        descriptor.acceleration_structure.reset();
    }

    // retrieve set-layout
    auto set_it = current.find(descriptors);

    if(set_it != current.end())
    {
        auto new_it = next.insert(std::move(*set_it)).first;
        current.erase(set_it);
        set_it = new_it;
    }
    else { set_it = next.find(descriptors); }

    // not found -> create and insert descriptor-set layout
    if(set_it == next.end())
    {
        auto new_set = vierkant::create_descriptor_set_layout(device, descriptors);
        set_it = next.insert(std::make_pair(std::move(descriptors), std::move(new_set))).first;
    }
    return set_it->second;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorSetPtr find_or_create_descriptor_set(const vierkant::DevicePtr &device,
                                               const DescriptorSetLayoutPtr &set_layout,
                                               const descriptor_map_t &descriptors,
                                               const vierkant::DescriptorPoolPtr &pool, descriptor_set_map_t &last,
                                               descriptor_set_map_t &current, bool variable_count)
{
    // handle for a descriptor-set
    DescriptorSetPtr ret;

    // search/create descriptor set

    // start searching in next_assets
    auto descriptor_set_it = current.find(descriptors);

    // not found in current assets
    if(descriptor_set_it == current.end())
    {
        // search in last assets (might already been processed for this frame)
        auto current_assets_it = last.find(descriptors);

        // not found in last assets
        if(current_assets_it == last.end())
        {
            // create a new descriptor set
            ret = vierkant::create_descriptor_set(device, pool, set_layout, variable_count);
        }
        else
        {
            // use existing descriptor set
            ret = std::move(current_assets_it->second);
            last.erase(current_assets_it);
        }

        // update the descriptor set
        vierkant::update_descriptor_set(device, ret, descriptors);

        // insert all created assets and store in map
        current[descriptors] = ret;
    }
    else { ret = descriptor_set_it->second; }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool descriptor_t::operator==(const descriptor_t &other) const
{
    if(type != other.type) { return false; }
    if(stage_flags != other.stage_flags) { return false; }
    if(variable_count != other.variable_count) { return false; }
    if(buffers != other.buffers) { return false; }
    if(buffer_offsets != other.buffer_offsets) { return false; }
    if(images != other.images) { return false; }
    if(image_views != other.image_views) { return false; }
    if(acceleration_structure != other.acceleration_structure) { return false; }
    return true;
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
    for(const auto &offset: descriptor.buffer_offsets) { hash_combine(h, offset); }
    for(const auto &img: descriptor.images) { hash_combine(h, img); }
    for(const auto &s: descriptor.image_views) { hash_combine(h, s); }
    hash_combine(h, descriptor.acceleration_structure);
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
