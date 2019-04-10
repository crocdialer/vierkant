//
// Created by crocdialer on 10/1/18.
//

#include "../include/vierkant/Image.hpp"
#include "../include/vierkant/SwapChain.hpp"

namespace vierkant {

//////////////////////////////////////////////// SWAP CHAIN UTILS //////////////////////////////////////////////////////

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities = {};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> modes;
};

SwapChainSupportDetails query_swapchain_support(VkPhysicalDevice the_device, VkSurfaceKHR the_surface)
{
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(the_device, the_surface, &details.capabilities);
    uint32_t num_formats;
    vkGetPhysicalDeviceSurfaceFormatsKHR(the_device, the_surface, &num_formats, nullptr);

    if(num_formats)
    {
        details.formats.resize(num_formats);
        vkGetPhysicalDeviceSurfaceFormatsKHR(the_device, the_surface, &num_formats, details.formats.data());
    }
    uint32_t num_present_modes;
    vkGetPhysicalDeviceSurfacePresentModesKHR(the_device, the_surface, &num_present_modes, nullptr);

    if(num_present_modes)
    {
        details.modes.resize(num_present_modes);
        vkGetPhysicalDeviceSurfacePresentModesKHR(the_device, the_surface, &num_present_modes, details.modes.data());
    }
    return details;
}

VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR> &the_formats)
{
    if(the_formats.size() == 1 && the_formats[0].format == VK_FORMAT_UNDEFINED)
    {
        return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    for(const auto &fmt : the_formats)
    {
        if(fmt.format == VK_FORMAT_B8G8R8A8_UNORM && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){ return fmt; }
    }
    return the_formats[0];
}

VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR> &the_modes, bool use_vsync)
{
    VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

    if(!use_vsync)
    {
        for(const auto &m : the_modes)
        {
            if(m == VK_PRESENT_MODE_MAILBOX_KHR){ return m; }
            else if(m == VK_PRESENT_MODE_IMMEDIATE_KHR){ best_mode = m; }
        }
    }
    return best_mode;
}

bool has_stencil_component(VkFormat the_format)
{
    return the_format == VK_FORMAT_D32_SFLOAT_S8_UINT || the_format == VK_FORMAT_D24_UNORM_S8_UINT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SwapChain::SwapChain(DevicePtr device, VkSurfaceKHR surface, VkSampleCountFlagBits num_samples, bool use_vsync) :
        m_device(std::move(device))
{
    SwapChainSupportDetails swap_chain_support = query_swapchain_support(m_device->physical_device(),
                                                                         surface);
    VkSurfaceFormatKHR surface_fmt = choose_swap_surface_format(swap_chain_support.formats);
    VkPresentModeKHR present_mode = choose_swap_present_mode(swap_chain_support.modes, use_vsync);
    auto caps = swap_chain_support.capabilities;
    VkExtent2D extent;

    if(caps.currentExtent.width != std::numeric_limits<uint32_t>::max()){ extent = caps.currentExtent; }
    else
    {
        extent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, extent.width));
        extent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, extent.height));
    }

    uint32_t imageCount = swap_chain_support.capabilities.minImageCount + 1;
    if(swap_chain_support.capabilities.maxImageCount > 0 && imageCount > swap_chain_support.capabilities.maxImageCount)
    {
        imageCount = swap_chain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;

    create_info.minImageCount = imageCount;
    create_info.imageFormat = surface_fmt.format;
    create_info.imageColorSpace = surface_fmt.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto indices = m_device->queue_family_indices();
    auto graphics_family = (uint32_t)indices[Device::Queue::GRAPHICS].index;
    auto present_family = (uint32_t)indices[Device::Queue::GRAPHICS].index;

    uint32_t queueFamilyIndices[] = {graphics_family,present_family};

    if(graphics_family != present_family)
    {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queueFamilyIndices;
    }else{ create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; }

    create_info.preTransform = swap_chain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    vkCheck(vkCreateSwapchainKHR(m_device->handle(), &create_info, nullptr, &m_swap_chain),
            "failed to create swap chain!");

    vkGetSwapchainImagesKHR(m_device->handle(), m_swap_chain, &imageCount, nullptr);
    std::vector<VkImage> swap_chain_images(imageCount);
    vkGetSwapchainImagesKHR(m_device->handle(), m_swap_chain, &imageCount, swap_chain_images.data());

    // retrieve color format
    m_color_format = surface_fmt.format;

    // surface extent
    m_extent = extent;

    vierkant::Image::Format fmt;
    fmt.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    fmt.format = surface_fmt.format;
    fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    fmt.initial_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    m_images.resize(imageCount);

    for(size_t i = 0; i < m_images.size(); i++)
    {
        m_images[i] = Image::create(m_device, swap_chain_images[i], {extent.width, extent.height, 1}, fmt);
    }

    // retrieve depth format
    m_depth_format = find_depth_format(m_device->physical_device());

    // clamp number of samples to device limit
    m_num_samples = std::max(VK_SAMPLE_COUNT_1_BIT, std::min(num_samples, m_device->max_usable_samples()));

    // create framebuffers
    create_framebuffers();

    // create semaphores and fences
    create_sync_objects();

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SwapChain::SwapChain(SwapChain &&other) noexcept:
        SwapChain()
{
    swap(*this, other);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SwapChain::~SwapChain()
{
    if(m_swap_chain)
    {
        vkDestroySwapchainKHR(m_device->handle(), m_swap_chain, nullptr);
        m_swap_chain = VK_NULL_HANDLE;

        for(auto &so : m_sync_objects)
        {
            vkDestroySemaphore(m_device->handle(), so.render_finished, nullptr);
            vkDestroySemaphore(m_device->handle(), so.image_available, nullptr);
            vkDestroyFence(m_device->handle(), so.in_flight, nullptr);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SwapChain &SwapChain::operator=(SwapChain other)
{
    swap(*this, other);
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VkResult SwapChain::aquire_next_image(uint32_t *image_index,
                                      uint64_t timeout)
{
    VkResult result = vkAcquireNextImageKHR(m_device->handle(), m_swap_chain,
                                            timeout,
                                            sync_objects().image_available,
                                            VK_NULL_HANDLE, &m_swapchain_image_index);
    if(image_index){ *image_index = m_swapchain_image_index; }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VkResult SwapChain::present()
{
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &sync_objects().render_finished;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_swap_chain;
    present_info.pImageIndices = &m_swapchain_image_index;
    present_info.pResults = nullptr; // Optional

    // swap buffers
    VkResult result = vkQueuePresentKHR(m_device->queue(Device::Queue::PRESENT), &present_info);

    // wait for prior frames to finish
    vkWaitForFences(m_device->handle(), 1, &sync_objects().in_flight, VK_TRUE,
                    std::numeric_limits<uint64_t>::max());
    vkResetFences(m_device->handle(), 1, &sync_objects().in_flight);
    m_current_frame_index = (m_current_frame_index + 1) % SwapChain::max_frames_in_flight;
    return result;
}

void swap(SwapChain &lhs, SwapChain &rhs)
{
    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_num_samples, rhs.m_num_samples);
    std::swap(lhs.m_swap_chain, rhs.m_swap_chain);
    std::swap(lhs.m_images, rhs.m_images);
    std::swap(lhs.m_framebuffers, rhs.m_framebuffers);
    std::swap(lhs.m_color_format, rhs.m_color_format);
    std::swap(lhs.m_depth_format, rhs.m_depth_format);
    std::swap(lhs.m_extent, rhs.m_extent);
    std::swap(lhs.m_sync_objects, rhs.m_sync_objects);
    std::swap(lhs.m_current_frame_index, rhs.m_current_frame_index);
    std::swap(lhs.m_swapchain_image_index, rhs.m_swapchain_image_index);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VkRenderPass SwapChain::renderpass() const
{
    if(!m_framebuffers.empty()){ return m_framebuffers.front().renderpass().get(); }
    return VK_NULL_HANDLE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SwapChain::create_framebuffers()
{
    m_framebuffers.clear();

    ImagePtr color_image;

    bool resolve = m_num_samples != VK_SAMPLE_COUNT_1_BIT;

    if(resolve)
    {
        Image::Format color_fmt;
        color_fmt.sample_count = m_num_samples;
        color_fmt.format = m_color_format;
        color_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        color_fmt.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        color_image = Image::create(m_device, {m_extent.width, m_extent.height, 1}, color_fmt);
    }else{ color_image = m_images.front(); }

    Image::Format depth_fmt;
    depth_fmt.sample_count = m_num_samples;
    depth_fmt.format = m_depth_format;
    depth_fmt.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_fmt.aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    auto depth_image = Image::create(m_device, {m_extent.width, m_extent.height, 1}, depth_fmt);

    vierkant::Framebuffer::AttachmentMap attachments;
    attachments[vierkant::Framebuffer::Attachment::Color] = {color_image};
    attachments[vierkant::Framebuffer::Attachment::DepthStencil] = {depth_image};
    if(resolve){ attachments[vierkant::Framebuffer::Attachment::Resolve] = {m_images.front()}; }

    // subpass is dependant on swapchain image
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    auto renderpass = vierkant::Framebuffer::create_renderpass(m_device, attachments, {dependency});

    m_framebuffers.resize(m_images.size());

    for(size_t i = 0; i < m_images.size(); i++)
    {
        if(resolve){ attachments[vierkant::Framebuffer::Attachment::Resolve] = {m_images[i]}; }
        else{ attachments[vierkant::Framebuffer::Attachment::Color] = {m_images[i]}; }
        m_framebuffers[i] = vierkant::Framebuffer(m_device, attachments, renderpass);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SwapChain::create_sync_objects()
{
    // allocate sync-objects
    m_sync_objects.resize(SwapChain::max_frames_in_flight);

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    for(size_t i = 0; i < SwapChain::max_frames_in_flight; i++)
    {
        if(vkCreateSemaphore(m_device->handle(), &semaphore_info, nullptr, &m_sync_objects[i].image_available) !=
           VK_SUCCESS ||
           vkCreateSemaphore(m_device->handle(), &semaphore_info, nullptr, &m_sync_objects[i].render_finished) !=
           VK_SUCCESS ||
           vkCreateFence(m_device->handle(), &fence_info, nullptr, &m_sync_objects[i].in_flight) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create sync object");
        }
        m_sync_objects[i].frame_index = static_cast<uint32_t>(i);
    }
}

}//namespace vulkan