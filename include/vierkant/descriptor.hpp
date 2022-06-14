//
// Created by crocdialer on 1/17/21.
//

#pragma once

#include <vierkant/Device.hpp>
#include <vierkant/Buffer.hpp>
#include <vierkant/Image.hpp>

namespace vierkant
{

using DescriptorPoolPtr = std::shared_ptr<VkDescriptorPool_T>;

using DescriptorSetLayoutPtr = std::shared_ptr<VkDescriptorSetLayout_T>;

using DescriptorSetPtr = std::shared_ptr<VkDescriptorSet_T>;

//! define a shared handle for a VkAccelerationStructureKHR
using AccelerationStructurePtr = std::shared_ptr<VkAccelerationStructureKHR_T>;

using descriptor_count_t = std::map<VkDescriptorType, uint32_t>;

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief   descriptor_t defines a resource-descriptor available in a shader program.
 *          it is default constructable, copyable, movable and hashable.
 */
struct descriptor_t
{
    VkDescriptorType type;
    VkShaderStageFlags stage_flags;

    //! using Vulkan 1.2 descriptor-indexing
    bool variable_count = false;

    //! used for descriptors containing buffers
    std::vector<vierkant::BufferPtr> buffers;

    //! optional array of buffer-offsets. if no value for a buffer index is found, 0 is used.
    std::vector<VkDeviceSize> buffer_offsets;

    //! used for descriptors containing (an array) of images
    std::vector<vierkant::ImagePtr> images;

    //! optional array of image-views. if no value for an image index is found, the default view is used.
    std::vector<VkImageView> image_views;

    //! used for descriptor containing a raytracing acceleration-structure
    AccelerationStructurePtr acceleration_structure;

    bool operator==(const descriptor_t &other) const;

    bool operator!=(const descriptor_t &other) const{ return !(*this == other); };
};

using descriptor_map_t = std::map<uint32_t, descriptor_t>;

/**
 * @brief   Create a shared VkDescriptorPool (DescriptorPoolPtr)
 *
 * @param   device  handle for the vierkant::Device to create the DescriptorPool
 * @param   counts  a descriptor_count_map_t providing type and count of descriptors
 * @return  the newly constructed DescriptorPoolPtr
 */
DescriptorPoolPtr create_descriptor_pool(const vierkant::DevicePtr &device,
                                         const descriptor_count_t &counts,
                                         uint32_t max_sets);

/**
 * @brief   Create a shared VkDescriptorSetLayout (DescriptorSetLayoutPtr) for a given array of vierkant::descriptor_t
 *
 * @param   device      handle for the vierkant::Device to create the DescriptorSetLayout
 * @param   descriptors an array of descriptor_t to create a layout from
 * @return  the newly created DescriptorSetLayoutPtr
 */
DescriptorSetLayoutPtr
create_descriptor_set_layout(const vierkant::DevicePtr &device,
                             const descriptor_map_t &descriptors);

/**
 * @brief   Create a shared VkDescriptorSet (DescriptorSetPtr) for a provided DescriptorLayout
 *
 * @param   device  handle for the vierkant::Device to create the DescriptorSet
 * @param   pool    handle for a shared VkDescriptorPool to allocate the DescriptorSet from
 * @param   layout  handle for a shared VkDescriptorSetLayout to use as blueprint
 * @return  the newly created DescriptorSetPtr
 */
DescriptorSetPtr create_descriptor_set(const vierkant::DevicePtr &device,
                                       const DescriptorPoolPtr &pool,
                                       const DescriptorSetLayoutPtr &layout,
                                       bool variable_count);

/**
 * @brief   Update an existing shared VkDescriptorSet with a provided array of vierkant::descriptor_t.
 *
 * @param   device          handle for the vierkant::Device to update the DescriptorSet
 * @param   descriptor_set  handle for a shared VkDescriptorSet to update
 * @param   descriptors     an array of descriptor_t to use for updating the DescriptorSet
 */
void update_descriptor_set(const vierkant::DevicePtr &device, const DescriptorSetPtr &descriptor_set,
                           const descriptor_map_t &descriptors);

DescriptorSetLayoutPtr find_set_layout(const vierkant::DevicePtr &device,
                                       descriptor_map_t descriptors,
                                       std::unordered_map<descriptor_map_t, DescriptorSetLayoutPtr>& layout_map);

}//namespace vierkant

namespace std
{
template<>
struct hash<vierkant::descriptor_t>
{
    size_t operator()(const vierkant::descriptor_t &descriptor) const;
};

template<>
struct hash<vierkant::descriptor_map_t>
{
    size_t operator()(const vierkant::descriptor_map_t &map) const;
};

}