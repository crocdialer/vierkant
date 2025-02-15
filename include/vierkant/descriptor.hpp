//
// Created by crocdialer on 1/17/21.
//

#pragma once

#include <vierkant/Buffer.hpp>
#include <vierkant/Device.hpp>
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
    //! type of contained descriptor
    VkDescriptorType type;

    //! stage-flags depicting in which shader-stages this descriptor will be used
    VkShaderStageFlags stage_flags;

    //! using Vulkan 1.2 descriptor-indexing
    bool variable_count = false;

    //! used for descriptors containing buffers
    std::vector<vierkant::BufferPtr> buffers;

    //! optional array of buffer-offsets. if no value for a buffer index is found, 0 is used.
    std::vector<VkDeviceSize> buffer_offsets;

    //! used for descriptors containing (an array of) images
    std::vector<vierkant::ImagePtr> images;

    //! optional array of image-views. if no value for an image index is found, the default view is used.
    std::vector<VkImageView> image_views;

    //! used for descriptors containing (an array of) raytracing acceleration-structures
    std::vector<AccelerationStructurePtr> acceleration_structures;

    //! used for descriptors containing an inline-uniform-block
    std::vector<uint8_t> inline_uniform_block;

    bool operator==(const descriptor_t &other) const = default;
};

//! maps binding-indices to descriptors
using descriptor_map_t = std::map<uint32_t, descriptor_t>;

//! maps a descriptor_map_t to a shared VkDescriptorSet
using descriptor_set_map_t = std::unordered_map<vierkant::descriptor_map_t, vierkant::DescriptorSetPtr>;

/**
 * @brief   Create a shared VkDescriptorPool (DescriptorPoolPtr)
 *
 * @param   device  handle for the vierkant::Device to create the DescriptorPool
 * @param   counts  a descriptor_count_map_t providing type and count of descriptors
 * @return  the newly constructed DescriptorPoolPtr
 */
DescriptorPoolPtr create_descriptor_pool(const vierkant::DevicePtr &device, const descriptor_count_t &counts,
                                         uint32_t max_sets);

/**
 * @brief   Create a shared VkDescriptorSetLayout (DescriptorSetLayoutPtr) for a given array of vierkant::descriptor_t
 *
 * @param   device      handle for the vierkant::Device to create the DescriptorSetLayout
 * @param   descriptors an array of descriptor_t to create a layout from
 * @return  the newly created DescriptorSetLayoutPtr
 */
DescriptorSetLayoutPtr create_descriptor_set_layout(const vierkant::DevicePtr &device,
                                                    const descriptor_map_t &descriptors);

/**
 * @brief   Create a shared VkDescriptorSet (DescriptorSetPtr) for a provided set-layout.
 *
 * @param   device      handle for the vierkant::Device to create the DescriptorSet
 * @param   pool        handle for a shared VkDescriptorPool to allocate the DescriptorSet from
 * @param   set_layout  handle for a VkDescriptorSetLayout
 * @return  the newly created DescriptorSetPtr
 */
DescriptorSetPtr create_descriptor_set(const vierkant::DevicePtr &device, const DescriptorPoolPtr &pool,
                                       VkDescriptorSetLayout set_layout, bool variable_count);

/**
 * @brief   Update an existing shared VkDescriptorSet with a provided array of vierkant::descriptor_t.
 *
 * @param   device          handle for the vierkant::Device to update the DescriptorSet
 * @param   descriptors     an array of descriptor_t to use for updating the DescriptorSet
 * @param   descriptor_set  handle for a shared VkDescriptorSet to update
 */
void update_descriptor_set(const vierkant::DevicePtr &device, const descriptor_map_t &descriptors,
                           const DescriptorSetPtr &descriptor_set);

/**
 * @brief   Update an existing shared vierkant::Buffer, used as descriptor-buffer,
 *          with a provided array of vierkant::descriptor_t.
 *
 * @param   device          handle for the vierkant::Device to update the DescriptorSet
 * @param   descriptors     an array of descriptor_t to use for updating the descriptor-buffer
 * @param   descriptor_set  handle for a shared VkDescriptorSet to update
 */
void update_descriptor_buffer(const vierkant::DevicePtr &device, const DescriptorSetLayoutPtr &layout,
                              const descriptor_map_t &descriptors, const vierkant::BufferPtr &out_descriptor_buffer);

/**
 * @brief   find_or_create_set_layout can be used to search for an existing descriptor-set-layout or create a new one.
 *          the result be will be returned and stored in a provided cache.
 *
 * @param   device      handle for a vierkant::Device to create new descriptor-set-layouts
 * @param   descriptors a provided descriptor-map
 * @param   current     output cache of retrieved/created descriptor-sets.
 * @return  a retrieved or newly created, shared VkDescriptorSetLayout.
 */
DescriptorSetLayoutPtr find_or_create_set_layout(const vierkant::DevicePtr &device, descriptor_map_t descriptors,
                                                 std::unordered_map<descriptor_map_t, DescriptorSetLayoutPtr> &current,
                                                 std::unordered_map<descriptor_map_t, DescriptorSetLayoutPtr> &next);

/**
 * @brief   find_or_create_descriptor_set can be used to search for an existing descriptor-set or create a new one.
 *          the result be will be returned and stored in a provided cache.
 *
 * @param   device          handle for a vierkant::Device to create new descriptor-sets
 * @param   set_layout      a provided set-layout.
 * @param   descriptors     a provided descriptor-map
 * @param   pool            a provided descriptor-pool
 * @param   last            cache of previously used descriptor-sets.
 * @param   current         output cache of retrieved/created descriptor-sets.
 * @param   variable_count  flag indicating if a variable descriptor-count is desired.
 * @param   relax_reuse     flag to somewhat relax reuse of descriptors
 * @return  a retrieved or newly created, shared VkDescriptorSet.
 */
DescriptorSetPtr find_or_create_descriptor_set(const vierkant::DevicePtr &device,
                                               VkDescriptorSetLayout set_layout,
                                               const descriptor_map_t &descriptors,
                                               const vierkant::DescriptorPoolPtr &pool, descriptor_set_map_t &last,
                                               descriptor_set_map_t &current, bool variable_count,
                                               bool relax_reuse = false);

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

}// namespace std