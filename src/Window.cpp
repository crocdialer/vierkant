//
// Created by crocdialer on 9/28/18.
//

#include "../include/vierkant/Window.hpp"
#include <iostream>

namespace vierkant {

/**
 * @brief RAII Helper for glfw initialization and termination
 */
class glfw_init_t
{
public:
    glfw_init_t() { glfwInit(); }

    ~glfw_init_t() { glfwTerminate(); }
};

static std::shared_ptr<glfw_init_t> g_glfw_init;

///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<const char *> Window::get_required_extensions()
{
    if(!g_glfw_init){ g_glfw_init = std::make_shared<glfw_init_t>(); }

    uint32_t num_extensions = 0;
    const char **extensions = glfwGetRequiredInstanceExtensions(&num_extensions);
    std::vector<const char *> ret(extensions, extensions + num_extensions);
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

WindowPtr Window::create(VkInstance instance, uint32_t width, uint32_t height, const std::string &the_name,
                         bool fullscreen, uint32_t monitor_index)
{
    return WindowPtr(new Window(instance, width, height, the_name, fullscreen, monitor_index));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Window::Window(VkInstance instance,
               uint32_t width,
               uint32_t height,
               const std::string &title,
               bool fullscreen,
               uint32_t monitor_index) :
        m_instance(instance)
{
    if(!g_glfw_init){ g_glfw_init = std::make_shared<glfw_init_t>(); }

    int monitor_count = 0;
    GLFWmonitor **monitors = glfwGetMonitors(&monitor_count);
    monitor_index = std::max<int>(monitor_index, monitor_count - 1);

    // no GL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_handle = glfwCreateWindow(width, height,
                                title.c_str(),
                                fullscreen ? monitors[monitor_index] : nullptr,
                                nullptr);
    if(!m_handle){ throw std::runtime_error("could not create window"); }

    vkCheck(glfwCreateWindowSurface(instance, m_handle, nullptr, &m_surface),
            "failed to create window surface!");

    // set user-pointer
    glfwSetWindowUserPointer(m_handle, this);

    // init callbacks
    glfwSetWindowSizeCallback(m_handle, &Window::resize_cb);
    glfwSetWindowCloseCallback(m_handle, &Window::close_cb);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Window::~Window()
{
    m_swap_chain = SwapChain();
    if(m_instance && m_surface){ vkDestroySurfaceKHR(m_instance, m_surface, nullptr); }
    glfwDestroyWindow(m_handle);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::create_swapchain(DevicePtr device, VkSampleCountFlagBits num_samples)
{
    // make sure everything is cleaned up
    // prevents: vkCreateSwapChainKHR(): surface has an existing swapchain other than oldSwapchain
    m_swap_chain = SwapChain();

    // create swapchain for this window
    m_swap_chain = SwapChain(device, m_surface, num_samples);

    m_command_buffer = vierkant::CommandBuffer(device, device->command_pool_transient());
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::record_command_buffer()
{
    uint32_t image_index = swapchain().image_index();
    auto device = swapchain().device();
    const auto &framebuffers = swapchain().framebuffers();

    if(!m_command_buffer){ m_command_buffer = vierkant::CommandBuffer(device, device->command_pool_transient()); }
    else{ m_command_buffer.reset(); }

    m_command_buffer.begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

    //  begin the renderpass
    framebuffers[image_index].begin_renderpass(m_command_buffer.handle(),
                                               VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    // fire secondary commandbuffers here
    if(m_draw_fn){ m_draw_fn(); }

    framebuffers[image_index].end_renderpass();
    m_command_buffer.end();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

glm::ivec2 Window::size() const
{
    glm::ivec2 ret;
    glfwGetWindowSize(m_handle, &ret.x, &ret.y);
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Window::is_minimized() const
{
    auto extent = framebuffer_size();
    return extent.x == 0 && extent.y == 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

glm::ivec2 Window::framebuffer_size() const
{
    glm::ivec2 ret;
    glfwGetFramebufferSize(m_handle, &ret.x, &ret.y);
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::set_size(const glm::ivec2 &extent)
{
    glfwSetWindowSize(m_handle, extent.x, extent.y);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

glm::ivec2 Window::position() const
{
    glm::ivec2 position;
    glfwGetWindowPos(m_handle, &position.x, &position.y);
    return position;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::set_position(const glm::ivec2 &position)
{
    glfwSetWindowPos(m_handle, position.x, position.y);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

std::string Window::title() const
{
    return m_title;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::set_title(const std::string &title)
{
    m_title = title;
    glfwSetWindowTitle(m_handle, title.c_str());
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::draw()
{
    uint32_t image_index;
    swapchain().aquire_next_image(&image_index);

    // record primary commandbuffer for this frame
    record_command_buffer();

    // submit with synchronization-infos
    auto sync_objects = swapchain().sync_objects();
    VkSemaphore wait_semaphores[] = {sync_objects.image_available};
    VkSemaphore signal_semaphores[] = {sync_objects.render_finished};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    // submit primary commandbuffer
    m_command_buffer.submit(swapchain().device()->queue(), false, sync_objects.in_flight, submit_info);

    // present the image (submit to presentation-queue, wait for fences)
    VkResult result = m_swap_chain.present();

    switch(result)
    {
        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
        default:
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t Window::monitor_index() const
{
    uint32_t ret = 0;
    int nmonitors, i;
    int wx, wy, ww, wh;
    int mx, my, mw, mh;
    int overlap, bestoverlap;
    GLFWmonitor **monitors;
    const GLFWvidmode *mode;
    bestoverlap = 0;

    glfwGetWindowPos(m_handle, &wx, &wy);
    glfwGetWindowSize(m_handle, &ww, &wh);
    monitors = glfwGetMonitors(&nmonitors);

    for(i = 0; i < nmonitors; i++)
    {
        mode = glfwGetVideoMode(monitors[i]);
        glfwGetMonitorPos(monitors[i], &mx, &my);
        mw = mode->width;
        mh = mode->height;

        overlap = std::max(0, std::min(wx + ww, mx + mw) - std::max(wx, mx)) *
                  std::max(0, std::min(wy + wh, my + mh) - std::max(wy, my));

        if(bestoverlap < overlap)
        {
            bestoverlap = overlap;
            ret = i;
        }
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Window::should_close() const
{
    return static_cast<bool>(glfwWindowShouldClose(m_handle));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::resize_cb(GLFWwindow *window, int width, int height)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    // while window is minimized
    while(self->is_minimized()){ glfwWaitEvents(); }

    // wait for work to finish in queue
    vkDeviceWaitIdle(self->m_swap_chain.device()->handle());

    // recreate a swapchain
    self->create_swapchain(self->m_swap_chain.device(), self->m_swap_chain.sample_count());

    if(self->m_resize_fn){ self->m_resize_fn(width, height); }

}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::close_cb(GLFWwindow *window)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));
    if(self->m_close_fn){ self->m_close_fn(); }

}

///////////////////////////////////////////////////////////////////////////////////////////////////
}//namespace vulkan