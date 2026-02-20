#include "vierkant/SwapChain.hpp"
#include "vierkant/Image.hpp"

namespace vierkant
{

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

VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR> &formats, bool use_hdr,
                                              bool &supports_hdr)
{
    VkSurfaceFormatKHR best_match = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    supports_hdr = false;

    for(const auto &fmt: formats)
    {
        if(fmt.format == VK_FORMAT_R16G16B16A16_SFLOAT) { supports_hdr = true; }

        if(fmt.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
        {
            supports_hdr = true;

            if(use_hdr)
            {
                //! (VK_COLOR_SPACE_HDR10_ST2084_EXT) requires the extensions VK_EXT_swapchain_colorspace
                if(/*fmt.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT ||*/
                   (fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && best_match.colorSpace <= fmt.colorSpace))
                {
                    best_match = fmt;
                }
            }
        }
    }
    return best_match;
}

VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR> &modes, bool use_vsync)
{
    VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

    for(const auto &m: modes)
    {
        if(!use_vsync)
        {
            if(m == VK_PRESENT_MODE_IMMEDIATE_KHR) { return m; }
            else if(m == VK_PRESENT_MODE_MAILBOX_KHR) { best_mode = m; }
        }
    }
    return best_mode;
}

bool has_stencil_component(VkFormat the_format)
{
    return the_format == VK_FORMAT_D32_SFLOAT_S8_UINT || the_format == VK_FORMAT_D24_UNORM_S8_UINT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SwapChain::SwapChain(DevicePtr device, VkSurfaceKHR surface, VkSampleCountFlagBits num_samples, bool use_vsync,
                     bool use_hdr, std::optional<VkExtent2D> extent)
    : m_device(std::move(device)), m_use_v_sync(use_vsync)
{
    SwapChainSupportDetails swap_chain_support = query_swapchain_support(m_device->physical_device(), surface);
    VkSurfaceFormatKHR surface_fmt = choose_swap_surface_format(swap_chain_support.formats, use_hdr, m_hdr_supported);
    VkPresentModeKHR present_mode = choose_swap_present_mode(swap_chain_support.modes, use_vsync);
    auto caps = swap_chain_support.capabilities;

    VkExtent2D framebuffer_size = extent ? *extent : VkExtent2D{0, 0};

    if(!extent && caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        framebuffer_size = caps.currentExtent;
    }
    else
    {
        framebuffer_size.width =
                std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, framebuffer_size.width));
        framebuffer_size.height =
                std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, framebuffer_size.height));
    }

    uint32_t max_image_count = std::min(swap_chain_support.capabilities.maxImageCount, max_frames_in_flight);
    uint32_t imageCount = swap_chain_support.capabilities.minImageCount + 1;

    if(swap_chain_support.capabilities.maxImageCount > 0 && imageCount > max_image_count)
    {
        imageCount = max_image_count;
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;

    create_info.minImageCount = imageCount;
    create_info.imageFormat = surface_fmt.format;
    create_info.imageColorSpace = surface_fmt.colorSpace;
    create_info.imageExtent = framebuffer_size;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto indices = m_device->queue_family_indices();
    auto graphics_family = (uint32_t) indices[Device::Queue::GRAPHICS].index;
    auto present_family = (uint32_t) indices[Device::Queue::GRAPHICS].index;

    uint32_t queueFamilyIndices[] = {graphics_family, present_family};

    if(graphics_family != present_family)
    {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

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
    m_extent = framebuffer_size;

    vierkant::Image::Format fmt;
    fmt.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    fmt.extent = {framebuffer_size.width, framebuffer_size.height, 1};
    fmt.format = surface_fmt.format;
    fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    fmt.initial_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    m_images.resize(imageCount);

    for(size_t i = 0; i < m_images.size(); i++)
    {
        // do not delete on destruction, we do not own the image
        auto shared_image = VkImagePtr(swap_chain_images[i], [](VkImage) {});
        m_images[i] = Image::create(m_device, shared_image, fmt);
    }

    // retrieve depth format
    m_depth_format = find_depth_format(m_device->physical_device());

    // clamp number of samples to device limit
    m_num_samples = std::clamp(num_samples, VK_SAMPLE_COUNT_1_BIT, m_device->max_usable_samples());

    // create framebuffers
    create_framebuffers();

    // create semaphores and fences
    create_sync_objects();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SwapChain::SwapChain(SwapChain &&other) noexcept : SwapChain() { swap(*this, other); }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SwapChain::~SwapChain()
{
    if(m_swap_chain)
    {
        vkDeviceWaitIdle(m_device->handle());
        vkDestroySwapchainKHR(m_device->handle(), m_swap_chain, nullptr);
        m_swap_chain = VK_NULL_HANDLE;

        for(auto &so: m_sync_objects)
        {
            vkDestroySemaphore(m_device->handle(), so.render_finished, nullptr);
            vkDestroySemaphore(m_device->handle(), so.image_available, nullptr);
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

SwapChain::acquire_image_result_t SwapChain::acquire_next_image(uint64_t timeout)
{
    acquire_image_result_t ret = {};

    m_framebuffers[m_current_frame_index].wait_fence();

    ret.image_available = m_sync_objects[m_current_frame_index].image_available;
    ret.result = vkAcquireNextImageKHR(m_device->handle(), m_swap_chain, timeout, ret.image_available, VK_NULL_HANDLE,
                                       &m_swapchain_image_index);
    ret.render_finished = m_sync_objects[m_swapchain_image_index].render_finished;
    ret.image_index = m_swapchain_image_index;
    m_last_acquired_image = ret;
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VkResult SwapChain::present()
{
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &m_last_acquired_image.render_finished;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_swap_chain;
    present_info.pImageIndices = &m_last_acquired_image.image_index;
    present_info.pResults = nullptr;// Optional

    // swap buffers
    VkResult result = VK_NOT_READY;
    VkQueue present_queue = m_device->queue(Device::Queue::PRESENT);
    if(auto *queue_asset = m_device->queue_asset(present_queue))
    {
        std::unique_lock lock(*queue_asset->mutex);
        result = vkQueuePresentKHR(present_queue, &present_info);
    }
    m_current_frame_index = (m_current_frame_index + 1) % m_images.size();
    return result;
}

void swap(SwapChain &lhs, SwapChain &rhs)
{
    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_num_samples, rhs.m_num_samples);
    std::swap(lhs.m_swap_chain, rhs.m_swap_chain);
    std::swap(lhs.m_use_v_sync, rhs.m_use_v_sync);
    std::swap(lhs.m_hdr_supported, rhs.m_hdr_supported);
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
    if(!m_framebuffers.empty()) { return m_framebuffers.front().renderpass().get(); }
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
        color_fmt.extent = {m_extent.width, m_extent.height, 1};
        color_fmt.format = m_color_format;
        color_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        color_fmt.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        color_image = Image::create(m_device, color_fmt);
    }
    else
    {
        color_image = m_images.front();
    }

    Image::Format depth_fmt;
    depth_fmt.extent = {m_extent.width, m_extent.height, 1};
    depth_fmt.sample_count = m_num_samples;
    depth_fmt.format = m_depth_format;
    depth_fmt.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_fmt.aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    auto depth_image = Image::create(m_device, depth_fmt);

    vierkant::attachment_map_t attachments;
    attachments[vierkant::AttachmentType::Color] = {color_image};
    attachments[vierkant::AttachmentType::DepthStencil] = {depth_image};
    if(resolve) { attachments[vierkant::AttachmentType::Resolve] = {m_images.front()}; }

    // subpass is dependant on swapchain image
    VkSubpassDependency2 dependency = {};
    dependency.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_2_NONE;
    dependency.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

    auto renderpass = vierkant::create_renderpass(m_device, attachments, true, true, {dependency});

    m_framebuffers.resize(m_images.size());

    for(size_t i = 0; i < m_images.size(); i++)
    {
        if(resolve) { attachments[vierkant::AttachmentType::Resolve] = {m_images[i]}; }
        else
        {
            attachments[vierkant::AttachmentType::Color] = {m_images[i]};
        }
        m_framebuffers[i] = vierkant::Framebuffer(m_device, attachments, renderpass);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SwapChain::create_sync_objects()
{
    // allocate sync-objects
    m_sync_objects.resize(m_images.size());

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for(size_t i = 0; i < m_images.size(); i++)
    {
        if(vkCreateSemaphore(m_device->handle(), &semaphore_info, nullptr, &m_sync_objects[i].image_available) !=
                   VK_SUCCESS ||
           vkCreateSemaphore(m_device->handle(), &semaphore_info, nullptr, &m_sync_objects[i].render_finished) !=
                   VK_SUCCESS)
        {
            throw std::runtime_error("failed to create sync object");
        }
    }
}

}// namespace vierkant
