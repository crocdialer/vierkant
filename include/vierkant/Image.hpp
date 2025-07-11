//
// Created by crocdialer on 10/2/18.
//
#pragma once

#include "vierkant/Buffer.hpp"
#include "vierkant/Device.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(Image)

using VkImagePtr = std::shared_ptr<VkImage_T>;
using VkImageViewPtr = std::shared_ptr<VkImageView_T>;
using VkImageLayoutPtr = std::shared_ptr<VkImageLayout>;
using VkSamplerPtr = std::shared_ptr<VkSampler_T>;

VkDeviceSize num_bytes(VkFormat format);

VkDeviceSize num_bytes(VkIndexType format);

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

        VkSamplerReductionMode reduction_mode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;

        VkComponentMapping component_swizzle = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
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
        VkCommandBuffer initial_cmd_buffer = VK_NULL_HANDLE;
        std::string name;

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
    static VmaPoolPtr create_pool(const DevicePtr &device, const Format &fmt, VkDeviceSize block_size = 0,
                                  size_t min_block_count = 0, size_t max_block_count = 0,
                                  VmaPoolCreateFlags vma_flags = 0);

    /**
     * @brief   Factory to create an empty, shared vierkant::Image with provided format.
     *
     * @return  a newly created ImagePtr
     */
    static ImagePtr create(DevicePtr device, Format format);

    /**
     * @brief   Factory to create a shared vierkant::Image with provided format and image-data.
     *
     * provided image-data will be copied into a gpu-buffer using internal staging with blocking sync.
     *
     * @return  a newly created ImagePtr
     */
    static ImagePtr create(DevicePtr device, const void *data, Format format);

    /**
     * @brief   Factory to create a shared vierkant::Image with provided format, using an existing image-handle.
     *
     * 'can' be used to e.g. wrap pre-existing swapchain-image-handles.
     *
     * @return  a newly created ImagePtr
     */
    static ImagePtr create(DevicePtr device, const VkImagePtr &shared_image, Format format);

    Image(Image &&) = delete;

    Image &operator=(Image other) = delete;

    ~Image() = default;

    /**
     * @return  the image extent
     */
    [[nodiscard]] inline const VkExtent3D &extent() const { return m_format.extent; }

    /**
     * @return  the width of the image in pixels
     */
    [[nodiscard]] inline uint32_t width() const { return m_format.extent.width; }

    /**
     * @return  the height of the image in pixels
     */
    [[nodiscard]] inline uint32_t height() const { return m_format.extent.height; }

    /**
     * @return  the depth of the image in pixels
     */
    [[nodiscard]] inline uint32_t depth() const { return m_format.extent.depth; }

    /**
     * @return  number of array layers
     */
    [[nodiscard]] inline uint32_t num_layers() const { return m_format.num_layers; }

    /**
     * @return  the current format struct
     */
    [[nodiscard]] const Format &format() const { return m_format; }

    /**
     * @return  handle to the managed VkImage
     */
    [[nodiscard]] VkImage image() const { return m_image.get(); };

    /**
     * @return  image view handle
     */
    [[nodiscard]] VkImageView image_view() const { return m_image_view.get(); };

    /**
     * @brief   set a new shared VkImageView-handle.
     *
     * @param   sampler desired imageview
     */
    void set_image_view(VkImageViewPtr image_view) { m_image_view = std::move(image_view); }

    /**
     * @return  image view handles for mips
     */
    [[nodiscard]] const std::vector<VkImageViewPtr> &mip_image_views() const { return m_mip_image_views; };

    /**
     * @return  shared image sampler handle
     */
    [[nodiscard]] VkSamplerPtr sampler() const { return m_sampler; };

    /**
     * @brief   set a new shared VkSampler-handle.
     *
     * @param   sampler desired sampler
     */
    void set_sampler(VkSamplerPtr sampler) { m_sampler = std::move(sampler); }

    /**
     * @return  current image layout
     */
    [[nodiscard]] VkImageLayout image_layout() const { return *m_image_layout; };

    /**
     * @return  number of images in the mipmap chain.
     */
    [[nodiscard]] uint32_t num_mip_levels() const { return m_num_mip_levels; };

    /**
     * @brief  request transition to a new image layout
     */
    void transition_layout(VkImageLayout new_layout, VkCommandBuffer cmd_buffer = VK_NULL_HANDLE,
                           VkDependencyFlags dependency_flags = 0);

    /**
     * @brief  more explicit alternative to 'transition_layout'
     */
    void barrier(VkImageLayout new_layout, VkCommandBuffer command_buffer, VkPipelineStageFlags2 src_stage,
                 VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access);

    /**
     * @brief   generate a mipmap-chain by performing linear-filtered blits.
     *
     * @param   command_buffer  optional commandbuffer-handle to record into.
     */
    void generate_mipmaps(VkCommandBuffer command_buffer = VK_NULL_HANDLE);

    /**
     * @brief   copy contents from a buffer to this image
     *
     * @param   src         the source buffer to copy data from
     * @param   buf_offset  the source-offset in bytes inside the src-buffer
     * @param   img_offset  the offset used for the copy operation
     * @param   extent      the extent of the memory region to copy
     * @param   layer       the target layer in the image to copy the data into
     * @param   level       the target mip-level to copy the data into
     */
    void copy_from(const BufferPtr &src, VkCommandBuffer cmd_buffer_handle = VK_NULL_HANDLE, size_t buf_offset = 0,
                   VkOffset3D img_offset = {0, 0, 0}, VkExtent3D extent = {0, 0, 0}, uint32_t layer = 0,
                   uint32_t level = 0);

    /**
     * @brief   copy contents from this image to a buffer
     *
     * @param   dst         the destination buffer to copy data to
     * @param   buf_offset  the destination-offset in bytes inside the target buffer
     * @param   img_offset  the image-offset used for the copy operation
     * @param   extent      the extent of the memory region to copy
     * @param   layer       the target layer in the image to copy the data from
     * @param   level       the target mip-level to copy the data from
     */
    void copy_to(const BufferPtr &dst, VkCommandBuffer command_buffer = VK_NULL_HANDLE, size_t buf_offset = 0,
                 VkOffset3D img_offset = {0, 0, 0}, VkExtent3D extent = {0, 0, 0}, uint32_t layer = 0,
                 uint32_t level = 0);

    /**
     * @brief   copy contents from this image to another image
     *
     * @param   dst         the destination image to copy data to
     * @param   src_offset  the source-offset used for the copy operation
     * @param   dst_offset  the desitnation-offset used for the copy operation
     * @param   extent      the extent of the memory region to copy
     */
    void copy_to(const ImagePtr &dst, VkCommandBuffer command_buffer = VK_NULL_HANDLE,
                 VkOffset3D src_offset = {0, 0, 0}, VkOffset3D dst_offset = {0, 0, 0}, VkExtent3D extent = {0, 0, 0});

    /**
     * @brief   clone an image, yielding a new ImagePtr with image/image-views/sampler being shared.
     *
     * @return  a cloned vierkant::ImagePtr
     */
    [[nodiscard]] ImagePtr clone() const;

    /**
     * @return  the vierkant::DevicePtr used to create the image.
     */
    [[nodiscard]] vierkant::DevicePtr device() const { return m_device; }

private:
    Image(DevicePtr device, const void *data, const VkImagePtr &shared_image, Format format);

    Image(const Image &) = default;

    DevicePtr m_device = nullptr;

    // number of images in mipmap chain
    uint32_t m_num_mip_levels = 1;

    // image handle
    VkImagePtr m_image = nullptr;

    // image view handle
    VkImageViewPtr m_image_view = nullptr;

    // image view handles for mipmap-levels
    std::vector<VkImageViewPtr> m_mip_image_views;

    // sampler handle
    VkSamplerPtr m_sampler = nullptr;

    // current image layout
    VkImageLayoutPtr m_image_layout = nullptr;

    // current format
    Format m_format;
};

}//namespace vierkant

static inline bool operator==(const VkExtent3D &lhs, const VkExtent3D &rhs)
{
    if(lhs.width != rhs.width) { return false; }
    if(lhs.height != rhs.height) { return false; }
    if(lhs.depth != rhs.depth) { return false; }
    return true;
}

static inline bool operator!=(const VkExtent3D &lhs, const VkExtent3D &rhs) { return !(lhs == rhs); }

namespace std
{
template<>
struct hash<vierkant::Image::Format>
{
    size_t operator()(vierkant::Image::Format const &fmt) const;
};
}// namespace std
