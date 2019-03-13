//
// Created by crocdialer on 10/2/18.
//

#include "../include/vierkant/CommandBuffer.hpp"
#include "../include/vierkant/Buffer.hpp"
#include "../include/vierkant/Image.hpp"

namespace vierkant
{

///////////////////////////////////////////////////////////////////////////////////////////////////

VkDeviceSize num_bytes_per_pixel(VkFormat the_format)
{
    switch(the_format)
    {
        case VK_FORMAT_R8G8B8A8_UNORM:
            return 4;
        case VK_FORMAT_R8G8B8_UNORM:
            return 3;
        case VK_FORMAT_R8_UNORM:
            return 1;
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
            return 0;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void transition_image_layout(VkCommandBuffer commandBuffer, VkImage the_image, VkFormat the_format,
                             VkImageLayout the_old_layout, VkImageLayout the_new_layout,
                             uint32_t the_num_mip_levels)
{
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = the_old_layout;
    barrier.newLayout = the_new_layout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = the_image;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = the_num_mip_levels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;


    if(the_new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        bool has_stencil = the_format == VK_FORMAT_D32_SFLOAT_S8_UINT || the_format == VK_FORMAT_D24_UNORM_S8_UINT;
        if(has_stencil){ barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT; }
    }else{ barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; }

    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;

    if(the_old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
       the_new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }else if(the_old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             the_new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }else if(the_old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
             the_new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask =
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }else if(the_old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
             the_new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }else if(the_old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
             the_new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }else if(the_old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
             the_new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }else if(the_old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
             the_new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }else if(the_old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
             the_new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }else
    {
        throw std::invalid_argument("unsupported layout transition!");
    }
    vkCmdPipelineBarrier(commandBuffer, source_stage, destination_stage, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

ImagePtr Image::create(DevicePtr the_device, void *the_data, VkExtent3D size, Format the_format)
{
    return ImagePtr(new Image(std::move(the_device), the_data, VK_NULL_HANDLE,
                              size, 1, the_format));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

ImagePtr Image::create(DevicePtr the_device, VkExtent3D size, Format the_format)
{
    return ImagePtr(new Image(std::move(the_device), nullptr, VK_NULL_HANDLE,
                              size, 1, the_format));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

ImagePtr Image::create(DevicePtr the_device, VkImage the_image, VkExtent3D size, Format the_format)
{
    return ImagePtr(new Image(std::move(the_device), nullptr, the_image, size, 1, the_format));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Image::Image(DevicePtr the_device, void *the_data, VkImage the_image, VkExtent3D size,
             uint32_t the_num_layers, Format the_format) :
        m_device(std::move(the_device)),
        m_extent(size),
        m_format(the_format)
{
    init(the_data, the_image);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Image::~Image()
{
    if(m_sampler){ vkDestroySampler(m_device->handle(), m_sampler, nullptr); }
    if(m_image_view){ vkDestroyImageView(m_device->handle(), m_image_view, nullptr); }
    if(m_image && m_owner){ vmaDestroyImage(m_device->vk_mem_allocator(), m_image, m_allocation); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Image::init(void *the_data, VkImage the_image)
{
    if(!m_extent.width || !m_extent.height || !m_extent.depth){ throw std::runtime_error("image extent is zero"); }

    ////////////////////////////////////////// create image ////////////////////////////////////////////////////////////

    VkImageUsageFlags img_usage = m_format.usage;

    // we expect a transfer
    if(the_data){ img_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; }

    m_num_mip_levels = 1;

    if(m_format.use_mipmap)
    {
        // number of images in the mipmap chain
        m_num_mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(width(), height())))) + 1;

        // in order to generate mipmaps we need to be able to transfer from base mip-level
        img_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if(the_image)
    {
        if(m_image){ vmaDestroyImage(m_device->vk_mem_allocator(), m_image, m_allocation); }

        // use a provided VkImage that we do not own
        m_image = the_image;
        m_owner = false;
    }else
    {
        VkImageCreateInfo image_create_info = {};
        image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_create_info.imageType = VK_IMAGE_TYPE_2D;
        image_create_info.extent = m_extent;
        image_create_info.mipLevels = m_num_mip_levels;
        image_create_info.arrayLayers = m_format.num_layers;
        image_create_info.format = m_format.format;
        image_create_info.tiling = m_format.tiling;
        image_create_info.initialLayout = m_format.initial_layout;
        image_create_info.usage = img_usage;
        image_create_info.samples = m_format.sample_count;
        image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // ask vma to create the image
        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        vmaCreateImage(m_device->vk_mem_allocator(), &image_create_info, &alloc_info, &m_image, &m_allocation,
                       &m_allocation_info);
        m_owner = true;
    }


    ////////////////////////////////////////// copy contents ///////////////////////////////////////////////////////////

    if(the_data)
    {
        auto staging_buffer = Buffer::create(m_device, the_data,
                                             width() * height() * depth() *
                                             num_bytes_per_pixel(m_format.format),
                                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        copy_from(staging_buffer);
    }

    ////////////////////////////////////////// mipmap //////////////////////////////////////////////////////////////////

//    if(the_data && m_format.use_mipmap && m_owner){ generate_mipmaps(); }

    ////////////////////////////////////////// create image view ///////////////////////////////////////////////////////

    VkImageViewCreateInfo view_create_info = {};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = m_image;
    view_create_info.viewType = m_format.view_type;
    view_create_info.format = m_format.format;
    view_create_info.components = m_format.component_swizzle;
    view_create_info.subresourceRange.aspectMask = m_format.aspect;//VK_IMAGE_ASPECT_COLOR_BIT;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = m_num_mip_levels;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = m_format.num_layers;

    vkCheck(vkCreateImageView(m_device->handle(), &view_create_info, nullptr, &m_image_view),
            "failed to create texture image view!");

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

        vkCheck(vkCreateSampler(m_device->handle(), &sampler_create_info, nullptr, &m_sampler),
                "failed to create texture sampler!");
    }

    ////////////////////////////////////////// layout transitions //////////////////////////////////////////////////////

    m_image_layout = m_format.initial_layout;

    if(m_image_layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        if(img_usage & VK_IMAGE_USAGE_SAMPLED_BIT)
        {
            transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }else if(img_usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        {
            transition_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }else if(img_usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            transition_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Image::transition_layout(VkImageLayout the_new_layout, VkCommandBuffer cmdBufferHandle)
{
    if(the_new_layout != m_image_layout)
    {
        vierkant::CommandBuffer localCommandBuffer;

        if(cmdBufferHandle == VK_NULL_HANDLE)
        {
            localCommandBuffer = vierkant::CommandBuffer(m_device, m_device->command_pool_transient());
            localCommandBuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            cmdBufferHandle = localCommandBuffer.handle();
        }
        transition_image_layout(cmdBufferHandle, m_image, m_format.format, m_image_layout, the_new_layout,
                                m_num_mip_levels);

        // submit local command-buffer, if any. also creates a fence and waits for completion of operation
        if(localCommandBuffer)
        {
            localCommandBuffer.submit(m_device->graphics_queue(), true);
        }
        m_image_layout = the_new_layout;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Image::copy_from(const BufferPtr &src, VkCommandBuffer cmd_buffer_handle,
                      VkOffset3D offset, VkExtent3D extent, uint32_t layer)
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

        if(!extent.width || !extent.height || !extent.depth){ extent = m_extent; }

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = layer;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = offset;
        region.imageExtent = extent;
        vkCmdCopyBufferToImage(cmd_buffer_handle, src->handle(), m_image, m_image_layout, 1, &region);

        // generate new mipmaps after copying
        if(m_format.use_mipmap){ generate_mipmaps(cmd_buffer_handle); }

        if(localCommandBuffer)
        {
            localCommandBuffer.submit(m_device->graphics_queue(), true);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Image::copy_to(const BufferPtr &dst, VkCommandBuffer command_buffer, VkOffset3D offset, VkExtent3D extent,
                    uint32_t layer)
{
    if(dst)
    {
        vierkant::CommandBuffer local_command_buffer;

        if(!command_buffer)
        {
            local_command_buffer = CommandBuffer(m_device, m_device->command_pool_transient());
            local_command_buffer.begin();
            command_buffer = local_command_buffer.handle();
        }

        transition_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, command_buffer);

        if(!extent.width || !extent.height || !extent.depth){ extent = m_extent; }

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = layer;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = offset;
        region.imageExtent = extent;
        vkCmdCopyImageToBuffer(command_buffer, m_image, m_image_layout, dst->handle(), 1, &region);

        if(local_command_buffer)
        {
            local_command_buffer.submit(m_device->graphics_queue(), true);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Image::generate_mipmaps(VkCommandBuffer command_buffer)
{
    // Check if image format supports linear blitting
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(m_device->physical_device(), m_format.format, &format_properties);

    if(!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
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
    barrier.image = m_image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
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
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        VkImageBlit blit = {};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mip_width, mip_height, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {std::max(mip_width / 2, 1), std::max(mip_height / 2, 1), 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(command_buffer,
                       m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit,
                       VK_FILTER_LINEAR);

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
        local_command_buffer.submit(m_device->graphics_queue(), true);
    }
    m_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

}//namespace vulkan