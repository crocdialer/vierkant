//
// Created by crocdialer on 10/2/18.
//
#pragma once

#include "vierkant/Device.hpp"
#include "vierkant/Buffer.hpp"

namespace vierkant {

DEFINE_CLASS_PTR(Image)

using VkImagePtr = std::shared_ptr<VkImage_T>;

VkDeviceSize num_bytes_per_pixel(VkFormat format);

class Image
{
public:

    /**
     * @brief   Format groups all sort of information, necessary to describe and create a vierkant::Image.
     *          Format is default-constructable, trivially copyable, comparable and hashable.
     *          Can be used as key in std::unordered_map.
     */
    struct Format
    {
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        VkExtent3D extent = {};
        VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        VkImageType image_type = VK_IMAGE_TYPE_2D;
        VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VkSamplerAddressMode address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VkFilter min_filter = VK_FILTER_LINEAR;
        VkFilter mag_filter = VK_FILTER_LINEAR;
        VkComponentMapping component_swizzle = {VK_COMPONENT_SWIZZLE_IDENTITY,
                                                VK_COMPONENT_SWIZZLE_IDENTITY,
                                                VK_COMPONENT_SWIZZLE_IDENTITY,
                                                VK_COMPONENT_SWIZZLE_IDENTITY};
        float max_anisotropy = 0.f;
        bool initial_layout_transition = true;
        bool use_mipmap = false;
        bool autogenerate_mipmaps = true;
        VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        bool normalized_coords = true;
        VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
        uint32_t num_layers = 1;
        VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_GPU_ONLY;
        VmaPoolPtr memory_pool = nullptr;

        bool operator==(const Format &other) const;

        bool operator!=(const Format &other) const { return !(*this == other); };
    };

    /**
     * @brief   Create a memory pool, that can be used to allocate Images from.
     *
     * @param   device          handle for the vierkant::Device to create the pool with
     * @param   fmt             the Image::Format for Images allocated from this pool
     * @param   block_size      optional parameter for fixed block-sizes in bytes.
     * @param   min_block_count optional parameter for minimum number of allocated blocks
     * @param   max_block_count optional parameter for maximum number of allocated blocks
     * @param   vma_flags       the VmaPoolCreateFlags. can be used to change the memory-pool's allocation strategy
     * @return  the newly created VmaPoolPtr
     */
    static VmaPoolPtr create_pool(const DevicePtr &device, const Format& fmt, VkDeviceSize block_size = 0,
                                  size_t min_block_count = 0, size_t max_block_count = 0,
                                  VmaPoolCreateFlags vma_flags = 0);

    /**
     * @brief   Factory to create instances of ImagePtr.
     *
     * @return  a newly created ImagePtr
     */
    static ImagePtr create(DevicePtr device, void *data, Format format);

    /**
     * @brief   Factory to create instances of ImagePtr.
     *
     * @return  a newly created ImagePtr
     */
    static ImagePtr create(DevicePtr device, Format format);

    /**
     * @brief   Factory to create instances of ImagePtr.
     *
     * @return  a newly created ImagePtr
     */
    static ImagePtr create(DevicePtr device, const VkImagePtr& shared_image, Format format);

    Image(const Image &) = delete;

    Image(Image &&) = delete;

    Image &operator=(Image other) = delete;

    ~Image();

    /**
     * @return  the image extent
     */
    inline const VkExtent3D &extent() const { return m_format.extent; }

    /**
     * @return  the width of the image in pixels
     */
    inline uint32_t width() const { return m_format.extent.width; }

    /**
     * @return  the height of the image in pixels
     */
    inline uint32_t height() const { return m_format.extent.height; }

    /**
     * @return  the depth of the image in pixels
     */
    inline uint32_t depth() const { return m_format.extent.depth; }

    /**
     * @return  number of array layers
     */
    inline uint32_t num_layers() const { return m_format.num_layers; }

    /**
     * @return  the current format struct
     */
    const Format &format() const { return m_format; }

    /**
     * @return  handle to the managed VkImage
     */
    VkImage image() const { return m_image.get(); };

    /**
     * @return  shared handle to the managed VkImage
     */
    const VkImagePtr& shared_image() const { return m_image; };

    /**
     * @return  image view handle
     */
    VkImageView image_view() const { return m_image_view; };

    /**
     * @return  image sampler handle
     */
    VkSampler sampler() const { return m_sampler; };

    /**
     * @return  current image layout
     */
    VkImageLayout image_layout() const { return m_image_layout; };

    /**
     * @return  request transition to a new image layout
     */
    void transition_layout(VkImageLayout the_new_layout, VkCommandBuffer cmd_buffer_handle = VK_NULL_HANDLE);

    /**
     * @brief   copy contents from a buffer to this image
     *
     * @param   src         the source buffer to copy data from
     * @param   the_offset  the offset used for the copy operation
     * @param   the_extent  the extent of the memory region to copy
     * @param   the_layer   the target layer in the image to copy the data into
     */
    void copy_from(const BufferPtr &src, VkCommandBuffer cmd_buffer_handle = VK_NULL_HANDLE,
                   VkOffset3D the_offset = {0, 0, 0}, VkExtent3D the_extent = {0, 0, 0},
                   uint32_t the_layer = 0);

    /**
     * @brief   copy contents from this image to a buffer
     *
     * @param   dst         the destination buffer to copy data to
     * @param   the_offset  the offset used for the copy operation
     * @param   the_extent  the extent of the memory region to copy
     * @param   the_layer   the target layer in the image to copy the data from
     */
    void copy_to(const BufferPtr &dst, VkCommandBuffer command_buffer = VK_NULL_HANDLE,
                 VkOffset3D the_offset = {0, 0, 0}, VkExtent3D the_extent = {0, 0, 0},
                 uint32_t the_layer = 0);

private:

    Image(DevicePtr device, void *data, const VkImagePtr& shared_image, Format format);

    void init(void *data, const VkImagePtr& shared_image = nullptr);

    void generate_mipmaps(VkCommandBuffer command_buffer = VK_NULL_HANDLE);

    DevicePtr m_device;

    // number of images in mipmap chain
    uint32_t m_num_mip_levels = 1;

    // image handle
    VkImagePtr m_image = nullptr;

    // image view handle
    VkImageView m_image_view = VK_NULL_HANDLE;

    // sampler handle
    VkSampler m_sampler = VK_NULL_HANDLE;

    // current image layout
    VkImageLayout m_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    // current format
    Format m_format;

    // vma assets
    VmaAllocation m_allocation = nullptr;
    VmaAllocationInfo m_allocation_info = {};
};

}//namespace vierkant

namespace std {
template<>
struct hash<vierkant::Image::Format>
{
    size_t operator()(vierkant::Image::Format const &fmt) const;
};
}