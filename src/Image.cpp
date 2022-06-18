//
// Created by crocdialer on 10/2/18.
//

#include "vierkant/CommandBuffer.hpp"
#include "vierkant/Buffer.hpp"
#include "vierkant/Image.hpp"

namespace vierkant
{

///////////////////////////////////////////////////////////////////////////////////////////////////

VkDeviceSize num_bytes(VkFormat format)
{
    switch(format)
    {
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
        case VK_FORMAT_R8_UNORM:
            return 1;

        case VK_FORMAT_R8G8B8A8_UNORM:
            return 4;
        case VK_FORMAT_R8G8B8_UNORM:
            return 3;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return 16;
        case VK_FORMAT_R32G32B32_SFLOAT:
            return 12;
        case VK_FORMAT_R32_SFLOAT:
            return 4;
        case VK_FORMAT_R16_SFLOAT:
            return 2;
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return 8;
        case VK_FORMAT_R16G16B16_SFLOAT:
            return 6;
        default:
            throw std::runtime_error("num_bytes: format not handled");
    }
}

VkDeviceSize num_bytes(VkIndexType index_type)
{
    switch(index_type)
    {
        case VK_INDEX_TYPE_UINT16:
            return 2;
        case VK_INDEX_TYPE_UINT32:
            return 4;
        default:
            return 0;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void transition_image_layout(VkCommandBuffer command_buffer,
                             VkImage image,
                             VkFormat format,
                             VkImageLayout old_layout,
                             VkImageLayout new_layout,
                             uint32_t num_layers,
                             uint32_t num_mip_levels,
                             VkImageAspectFlags aspectMask)
{
    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = num_mip_levels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = num_layers;
    barrier.subresourceRange.aspectMask = aspectMask;

    switch(old_layout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            barrier.srcAccessMask = VK_ACCESS_2_NONE;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
            break;

            // TODO: check if this makes sense
        case VK_IMAGE_LAYOUT_GENERAL:
            barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
            barrier.srcStageMask =
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
        {
            if(aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
            {
                barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            }
            else
            {
                barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            }
        }
            break;

        default:
            break;
    }

    switch(new_layout)
    {
        case VK_IMAGE_LAYOUT_GENERAL:
            barrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                                   VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
        {
            if(aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
            {
                barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
            }
            else
            {
                barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            }
            break;
        }

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            break;

        default:
            barrier.dstAccessMask = VK_ACCESS_2_NONE;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            break;
    }
    VkDependencyInfo dependency_info = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &barrier;
    dependency_info.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    vkCmdPipelineBarrier2(command_buffer, &dependency_info);
}

VmaPoolPtr Image::create_pool(const DevicePtr &device, const Image::Format &format, VkDeviceSize block_size,
                              size_t min_block_count, size_t max_block_count, VmaPoolCreateFlags vma_flags)
{
    VkImageCreateInfo image_create_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_create_info.imageType = format.image_type;
    image_create_info.extent = format.extent;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = format.num_layers;
    image_create_info.format = format.format;
    image_create_info.tiling = format.tiling;
    image_create_info.initialLayout = format.initial_layout;
    image_create_info.usage = format.usage;
    image_create_info.samples = format.sample_count;
    image_create_info.sharingMode = format.sharing_mode;

    VmaAllocationCreateInfo dummy_alloc_create_info = {};
    dummy_alloc_create_info.usage = format.memory_usage;
    uint32_t mem_type_index;
    vmaFindMemoryTypeIndexForImageInfo(device->vk_mem_allocator(), &image_create_info, &dummy_alloc_create_info,
                                       &mem_type_index);

    VmaPoolCreateInfo pool_create_info = {};
    pool_create_info.memoryTypeIndex = mem_type_index;
    pool_create_info.blockSize = block_size;
    pool_create_info.minBlockCount = min_block_count;
    pool_create_info.maxBlockCount = max_block_count;
    pool_create_info.flags = vma_flags;

    // create pool
    VmaPool pool;
    vmaCreatePool(device->vk_mem_allocator(), &pool_create_info, &pool);

    // return self-destructing VmaPoolPtr
    return VmaPoolPtr(pool, [device](VmaPool p){ vmaDestroyPool(device->vk_mem_allocator(), p); });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

ImagePtr Image::create(DevicePtr device, const void *data, Format format)
{
    return ImagePtr(new Image(std::move(device), data, VK_NULL_HANDLE, std::move(format)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

ImagePtr Image::create(DevicePtr device, Format format)
{
    return ImagePtr(new Image(std::move(device), nullptr, VK_NULL_HANDLE, std::move(format)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

ImagePtr Image::create(DevicePtr device, const VkImagePtr &shared_image, Format format)
{
    return ImagePtr(new Image(std::move(device), nullptr, shared_image, std::move(format)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Image::Image(DevicePtr device, const void *data, const VkImagePtr &shared_image, Format format) :
        m_device(std::move(device)),
        m_format(std::move(format))
{
    init(data, shared_image);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Image::~Image()
{
    if(m_sampler){ vkDestroySampler(m_device->handle(), m_sampler, nullptr); }
    if(m_image_view){ vkDestroyImageView(m_device->handle(), m_image_view, nullptr); }

    for(uint32_t i = 0; i < m_num_mip_levels; ++i)
    {
        vkDestroyImageView(m_device->handle(), m_mip_image_views[i], nullptr);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Image::init(const void *data, const VkImagePtr &shared_image)
{
    if(!m_format.extent.width || !m_format.extent.height || !m_format.extent.depth)
    {
        throw std::runtime_error("image extent is zero");
    }

    ////////////////////////////////////////// create image ////////////////////////////////////////////////////////////

    VkImageUsageFlags img_usage = m_format.usage;

    // we expect a transfer
    if(data){ img_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; }

    m_num_mip_levels = 1;

    if(m_format.use_mipmap)
    {
        // number of images in the mipmap chain
        m_num_mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(width(), height())))) + 1;

        // BC7 has blocks >= 4 pixels and thus 2 levels less
        bool compressed = m_format.format == VK_FORMAT_BC7_UNORM_BLOCK || m_format.format == VK_FORMAT_BC7_SRGB_BLOCK;
        if(compressed){ m_num_mip_levels = std::max(static_cast<int32_t>(m_num_mip_levels) - 2, 1); }

        // in order to generate mipmaps we need to be able to transfer from base mip-level
        img_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if(shared_image)
    {
        // use an existing, shared VkImage
        m_image = shared_image;
    }
    else
    {
        VkImageCreateInfo image_create_info = {};
        image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_create_info.imageType = m_format.image_type;
        image_create_info.extent = m_format.extent;
        image_create_info.mipLevels = m_num_mip_levels;
        image_create_info.arrayLayers = m_format.num_layers;
        image_create_info.format = m_format.format;
        image_create_info.tiling = m_format.tiling;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_create_info.usage = img_usage;
        image_create_info.samples = m_format.sample_count;
        image_create_info.sharingMode = m_format.sharing_mode;

        if(m_format.view_type == VK_IMAGE_VIEW_TYPE_CUBE || m_format.view_type == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
        {
            image_create_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        // ask vma to create the image
        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = m_format.memory_usage;
        alloc_info.pool = m_format.memory_pool.get();

        VkImage image;
        vmaCreateImage(m_device->vk_mem_allocator(), &image_create_info, &alloc_info, &image, &m_allocation,
                       &m_allocation_info);

        m_image = VkImagePtr(image, [this](VkImage img)
        {
            vmaDestroyImage(m_device->vk_mem_allocator(), img, m_allocation);
        });
    }

    ////////////////////////////////////////// copy contents ///////////////////////////////////////////////////////////

    if(data)
    {
        auto staging_buffer = Buffer::create(m_device, data,
                                             width() * height() * depth() *
                                             num_bytes(m_format.format),
                                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                             VMA_MEMORY_USAGE_CPU_ONLY);

        // copy and sync with local commandbuffer
        copy_from(staging_buffer);
    }

    ////////////////////////////////////////// create image view ///////////////////////////////////////////////////////

    VkImageViewCreateInfo view_create_info = {};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = m_image.get();
    view_create_info.viewType = m_format.view_type;
    view_create_info.format = m_format.format;
    view_create_info.components = m_format.component_swizzle;
    view_create_info.subresourceRange.aspectMask = m_format.aspect;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = m_num_mip_levels;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = m_format.num_layers;

    vkCheck(vkCreateImageView(m_device->handle(), &view_create_info, nullptr, &m_image_view),
            "failed to create texture image view!");

    m_mip_image_views.resize(m_num_mip_levels);

    for(uint32_t i = 0; i < m_num_mip_levels; ++i)
    {
        view_create_info.subresourceRange.baseMipLevel = i;
        view_create_info.subresourceRange.levelCount = 1;
        vkCheck(vkCreateImageView(m_device->handle(), &view_create_info, nullptr, &m_mip_image_views[i]),
                "failed to create texture image view!");
    }
    ////////////////////////////////////////// create image sampler ////////////////////////////////////////////////////

    if(img_usage & VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        VkSamplerCreateInfo sampler_create_info = {};
        sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_create_info.magFilter = m_format.mag_filter;
        sampler_create_info.minFilter = m_format.min_filter;
        sampler_create_info.addressModeU = m_format.address_mode_u;
        sampler_create_info.addressModeV = m_format.address_mode_v;
        sampler_create_info.addressModeW = m_format.address_mode_w;

        sampler_create_info.anisotropyEnable = static_cast<VkBool32>(m_format.max_anisotropy > 0.f);
        sampler_create_info.maxAnisotropy = m_format.max_anisotropy;

        sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

        // [0 ... tex_width] vs. [0 ... 1]
        sampler_create_info.unnormalizedCoordinates = static_cast<VkBool32>(!m_format.normalized_coords);

        sampler_create_info.compareEnable = VK_FALSE;
        sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;

        sampler_create_info.mipmapMode = m_format.mipmap_mode;
        sampler_create_info.mipLodBias = 0.0f;
        sampler_create_info.minLod = 0.0f;
        sampler_create_info.maxLod = static_cast<float>(m_num_mip_levels);

        VkSamplerReductionModeCreateInfo reduction_mode_info = {};
        reduction_mode_info.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
        reduction_mode_info.reductionMode = m_format.reduction_mode;
        sampler_create_info.pNext = &reduction_mode_info;

        vkCheck(vkCreateSampler(m_device->handle(), &sampler_create_info, nullptr, &m_sampler),
                "failed to create texture sampler!");
    }

    ////////////////////////////////////////// layout transitions //////////////////////////////////////////////////////

    if(m_format.initial_layout_transition)
    {
        if(m_format.initial_layout != VK_IMAGE_LAYOUT_UNDEFINED)
        {
            transition_layout(m_format.initial_layout, m_format.initial_cmd_buffer);
        }
        else if(img_usage & VK_IMAGE_USAGE_SAMPLED_BIT)
        {
            transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_format.initial_cmd_buffer);
        }
        else if(img_usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        {
            transition_layout(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, m_format.initial_cmd_buffer);
        }
        else if(img_usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            transition_layout(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, m_format.initial_cmd_buffer);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Image::transition_layout(VkImageLayout new_layout, VkCommandBuffer cmd_buffer)
{
    if(new_layout != m_image_layout)
    {
        vierkant::CommandBuffer localCommandBuffer;

        if(cmd_buffer == VK_NULL_HANDLE)
        {
            localCommandBuffer = vierkant::CommandBuffer(m_device, m_device->command_pool_transient());
            localCommandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            cmd_buffer = localCommandBuffer.handle();
        }
        transition_image_layout(cmd_buffer, m_image.get(), m_format.format, m_image_layout, new_layout,
                                m_format.num_layers, m_num_mip_levels, m_format.aspect);

        // submit local command-buffer, if any. also creates a fence and waits for completion of operation
        if(localCommandBuffer)
        {
            localCommandBuffer.submit(m_device->queue(), true);
        }
        m_image_layout = new_layout;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Image::copy_from(const BufferPtr &src, VkCommandBuffer cmd_buffer_handle,
                      VkOffset3D offset, VkExtent3D extent, uint32_t layer, uint32_t level)
{
    if(src)
    {
        vierkant::CommandBuffer localCommandBuffer;

        if(!cmd_buffer_handle)
        {
            localCommandBuffer = CommandBuffer(m_device, m_device->command_pool_transient());
            localCommandBuffer.begin();
            cmd_buffer_handle = localCommandBuffer.handle();
        }
        transition_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmd_buffer_handle);

        if(!extent.width || !extent.height || !extent.depth)
        {
            extent = m_format.extent;
            extent.width = std::max<uint32_t>(extent.width >> level, 1);
            extent.height = std::max<uint32_t>(extent.height >> level, 1);
            extent.depth = std::max<uint32_t>(extent.depth >> level, 1);
        }

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = level;
        region.imageSubresource.baseArrayLayer = layer;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = offset;
        region.imageExtent = extent;
        vkCmdCopyBufferToImage(cmd_buffer_handle, src->handle(), m_image.get(), m_image_layout, 1, &region);

        // generate new mipmaps after copying
        if(m_format.use_mipmap && m_format.autogenerate_mipmaps){ generate_mipmaps(cmd_buffer_handle); }

        if(localCommandBuffer)
        {
            localCommandBuffer.submit(m_device->queue(), true);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Image::copy_to(const BufferPtr &dst, VkCommandBuffer command_buffer, VkOffset3D offset, VkExtent3D extent,
                    uint32_t layer, uint32_t level)
{
    if(dst)
    {
        if(!extent.width || !extent.height || !extent.depth){ extent = m_format.extent; }

        // assure dst buffer has correct size, no-op if already the case
        dst->set_data(nullptr, num_bytes(m_format.format) * extent.width * extent.height * extent.depth);

        vierkant::CommandBuffer local_command_buffer;

        if(!command_buffer)
        {
            local_command_buffer = CommandBuffer(m_device, m_device->command_pool_transient());
            local_command_buffer.begin();
            command_buffer = local_command_buffer.handle();
        }

        transition_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, command_buffer);

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = level;
        region.imageSubresource.baseArrayLayer = layer;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = offset;
        region.imageExtent = extent;
        vkCmdCopyImageToBuffer(command_buffer, m_image.get(), m_image_layout, dst->handle(), 1, &region);

        if(local_command_buffer)
        {
            local_command_buffer.submit(m_device->queue(), true);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Image::copy_to(const ImagePtr &dst, VkCommandBuffer command_buffer, VkOffset3D offset, VkExtent3D extent)
{
    if(dst)
    {
        if(!extent.width || !extent.height || !extent.depth){ extent = m_format.extent; }

        vierkant::CommandBuffer local_command_buffer;

        if(!command_buffer)
        {
            local_command_buffer = CommandBuffer(m_device, m_device->command_pool_transient());
            local_command_buffer.begin();
            command_buffer = local_command_buffer.handle();
        }

        transition_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, command_buffer);

        // copy src-image -> dst0image
        VkImageCopy region = {};
        region.extent = extent;
        region.dstOffset = region.srcOffset = offset;
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.baseArrayLayer = 0;
        region.srcSubresource.layerCount = m_format.num_layers;
        region.srcSubresource.mipLevel = 0;

        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.baseArrayLayer = 0;
        region.dstSubresource.layerCount = m_format.num_layers;
        region.dstSubresource.mipLevel = 0;

        // transition src-layout
        transition_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, command_buffer);

        // transition dst-layout
        dst->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, command_buffer);

        // actual copy command
        vkCmdCopyImage(command_buffer, m_image.get(), m_image_layout, dst->image(), dst->image_layout(), 1, &region);

        if(local_command_buffer)
        {
            local_command_buffer.submit(m_device->queue(), true);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Image::generate_mipmaps(VkCommandBuffer command_buffer)
{
    if(!m_format.use_mipmap || m_num_mip_levels <= 1)
    {
        transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, command_buffer);
        return;
    }

    // Check if image format supports linear blitting
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(m_device->physical_device(), m_format.format, &format_properties);

    constexpr VkFormatFeatureFlags needed_features = VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                                                     VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;

    constexpr VkFilter blit_filter = VK_FILTER_LINEAR;

    if((format_properties.optimalTilingFeatures & needed_features) != needed_features)
    {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    vierkant::CommandBuffer local_command_buffer;

    if(!command_buffer)
    {
        local_command_buffer = CommandBuffer(m_device, m_device->command_pool_transient());
        local_command_buffer.begin();
        command_buffer = local_command_buffer.handle();
    }

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = m_image.get();
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = m_format.num_layers;
    barrier.subresourceRange.levelCount = 1;

    int32_t mip_width = width();
    int32_t mip_height = height();

    for(uint32_t i = 1; i < m_num_mip_levels; i++)
    {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        VkImageBlit blit = {};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mip_width, mip_height, 1};
        blit.srcSubresource.aspectMask = m_format.aspect;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = m_format.num_layers;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {std::max(mip_width / 2, 1), std::max(mip_height / 2, 1), 1};
        blit.dstSubresource.aspectMask = m_format.aspect;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = m_format.num_layers;

        vkCmdBlitImage(command_buffer,
                       m_image.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       m_image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit,
                       blit_filter);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        mip_width = std::max(mip_width / 2, 1);
        mip_height = std::max(mip_height / 2, 1);
    }

    barrier.subresourceRange.baseMipLevel = m_num_mip_levels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    if(local_command_buffer)
    {
        local_command_buffer.submit(m_device->queue(), true);
    }
    m_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Image::Format::operator==(const Image::Format &other) const
{
    if(aspect != other.aspect){ return false; }
    if(format != other.format){ return false; }
    if(memcmp(&extent, &other.extent, sizeof(VkExtent3D)) != 0){ return false; }
    if(initial_layout != other.initial_layout){ return false; }
    if(tiling != other.tiling){ return false; }
    if(image_type != other.image_type){ return false; }
    if(sharing_mode != other.sharing_mode){ return false; }
    if(view_type != other.view_type){ return false; }
    if(usage != other.usage){ return false; }
    if(address_mode_u != other.address_mode_u){ return false; }
    if(address_mode_v != other.address_mode_v){ return false; }
    if(address_mode_w != other.address_mode_w){ return false; }
    if(min_filter != other.min_filter){ return false; }
    if(mag_filter != other.mag_filter){ return false; }
    if(reduction_mode != other.reduction_mode){ return false; }
    if(memcmp(&component_swizzle, &other.component_swizzle, sizeof(VkComponentMapping)) != 0){ return false; }
    if(max_anisotropy != other.max_anisotropy){ return false; }
    if(initial_layout_transition != other.initial_layout_transition){ return false; }
    if(use_mipmap != other.use_mipmap){ return false; }
    if(autogenerate_mipmaps != other.autogenerate_mipmaps){ return false; }
    if(mipmap_mode != other.mipmap_mode){ return false; }
    if(normalized_coords != other.normalized_coords){ return false; }
    if(sample_count != other.sample_count){ return false; }
    if(num_layers != other.num_layers){ return false; }
    if(memory_usage != other.memory_usage){ return false; }
    if(memory_pool != other.memory_pool){ return false; }
    if(initial_cmd_buffer != other.initial_cmd_buffer){ return false; }
    return true;
}

}//namespace vierkant

size_t std::hash<vierkant::Image::Format>::operator()(vierkant::Image::Format const &fmt) const
{
    using crocore::hash_combine;

    size_t h = 0;
    hash_combine(h, fmt.aspect);
    hash_combine(h, fmt.format);
    hash_combine(h, fmt.extent.width);
    hash_combine(h, fmt.extent.height);
    hash_combine(h, fmt.extent.depth);
    hash_combine(h, fmt.initial_layout);
    hash_combine(h, fmt.tiling);
    hash_combine(h, fmt.image_type);
    hash_combine(h, fmt.sharing_mode);
    hash_combine(h, fmt.view_type);
    hash_combine(h, fmt.usage);
    hash_combine(h, fmt.address_mode_u);
    hash_combine(h, fmt.address_mode_v);
    hash_combine(h, fmt.address_mode_w);
    hash_combine(h, fmt.min_filter);
    hash_combine(h, fmt.mag_filter);
    hash_combine(h, fmt.reduction_mode);
    hash_combine(h, fmt.component_swizzle.r);
    hash_combine(h, fmt.component_swizzle.g);
    hash_combine(h, fmt.component_swizzle.b);
    hash_combine(h, fmt.component_swizzle.a);
    hash_combine(h, fmt.max_anisotropy);
    hash_combine(h, fmt.initial_layout_transition);
    hash_combine(h, fmt.use_mipmap);
    hash_combine(h, fmt.autogenerate_mipmaps);
    hash_combine(h, fmt.mipmap_mode);
    hash_combine(h, fmt.normalized_coords);
    hash_combine(h, fmt.sample_count);
    hash_combine(h, fmt.num_layers);
    hash_combine(h, fmt.memory_usage);
    hash_combine(h, fmt.memory_pool);
    hash_combine(h, fmt.initial_cmd_buffer);
    return h;
}
