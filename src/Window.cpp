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

void get_modifiers(GLFWwindow *window, uint32_t &buttonModifiers, uint32_t &keyModifiers)
{
    buttonModifiers = 0;
    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT))
        buttonModifiers |= MouseEvent::BUTTON_LEFT;
    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE))
        buttonModifiers |= MouseEvent::BUTTON_MIDDLE;
    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT))
        buttonModifiers |= MouseEvent::BUTTON_RIGHT;

    keyModifiers = 0;
    if(glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL))
        keyModifiers |= KeyEvent::CTRL_DOWN;
    if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT))
        keyModifiers |= KeyEvent::SHIFT_DOWN;
    if(glfwGetKey(window, GLFW_KEY_LEFT_ALT) || glfwGetKey(window, GLFW_KEY_RIGHT_ALT))
        keyModifiers |= KeyEvent::ALT_DOWN;
    if(glfwGetKey(window, GLFW_KEY_LEFT_SUPER) || glfwGetKey(window, GLFW_KEY_RIGHT_SUPER))
        keyModifiers |= KeyEvent::META_DOWN;
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
    glfwSetErrorCallback(&Window::glfw_error_cb);
    glfwSetWindowSizeCallback(m_handle, &Window::glfw_resize_cb);
    glfwSetWindowCloseCallback(m_handle, &Window::glfw_close_cb);
    glfwSetMouseButtonCallback(m_handle, &Window::glfw_mouse_button_cb);
    glfwSetCursorPosCallback(m_handle, &Window::glfw_mouse_move_cb);
    glfwSetScrollCallback(m_handle, &Window::glfw_mouse_wheel_cb);
    glfwSetKeyCallback(m_handle, &Window::glfw_key_cb);
    glfwSetCharCallback(m_handle, &Window::glfw_char_cb);
    glfwSetDropCallback(m_handle, &Window::glfw_file_drop_cb);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Window::~Window()
{
    m_swap_chain = SwapChain();
    if(m_instance && m_surface){ vkDestroySurfaceKHR(m_instance, m_surface, nullptr); }
    glfwDestroyWindow(m_handle);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::create_swapchain(DevicePtr device, VkSampleCountFlagBits num_samples, bool v_sync)
{
    // make sure everything is cleaned up
    // prevents: vkCreateSwapChainKHR(): surface has an existing swapchain other than oldSwapchain
    m_swap_chain = SwapChain();

    // create swapchain for this window
    m_swap_chain = SwapChain(device, m_surface, num_samples, v_sync);

    m_command_buffers.resize(m_swap_chain.max_frames_in_flight);

    for(uint32_t i = 0; i < m_command_buffers.size(); ++i)
    {
        m_command_buffers[i] = vierkant::CommandBuffer(device, device->command_pool_transient());
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::record_command_buffer()
{
    uint32_t image_index = swapchain().image_index();
    auto device = swapchain().device();
    const auto &framebuffers = swapchain().framebuffers();

    m_command_buffers[image_index].begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

    //  begin the renderpass
    framebuffers[image_index].begin_renderpass(m_command_buffers[image_index].handle(),
                                               VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    // fire secondary commandbuffers here
    if(draw_fn){ draw_fn(shared_from_this()); }

    framebuffers[image_index].end_renderpass();
    m_command_buffers[image_index].end();
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
    auto sync_objects = swapchain().sync_objects();

    // wait for prior frames to finish
    vkWaitForFences(swapchain().device()->handle(), 1, &sync_objects.in_flight, VK_TRUE,
                    std::numeric_limits<uint64_t>::max());
    vkResetFences(swapchain().device()->handle(), 1, &sync_objects.in_flight);

    uint32_t image_index;
    swapchain().aquire_next_image(&image_index);

    // record primary commandbuffer for this frame
    record_command_buffer();

    // submit with synchronization-infos
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
    m_command_buffers[image_index].submit(swapchain().device()->queue(), false, sync_objects.in_flight, submit_info);

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

void Window::glfw_resize_cb(GLFWwindow *window, int width, int height)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    // while window is minimized
    while(self->is_minimized()){ glfwWaitEvents(); }

    // wait for work to finish in queue
    vkDeviceWaitIdle(self->m_swap_chain.device()->handle());

    // recreate a swapchain
    self->create_swapchain(self->m_swap_chain.device(), self->m_swap_chain.sample_count(), self->m_swap_chain.v_sync());

    if(self->resize_fn){ self->resize_fn(width, height); }

}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_close_cb(GLFWwindow *window)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));
    if(self->close_fn){ self->close_fn(); }

}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_error_cb(int error_code, const char *error_msg)
{
    LOG_ERROR << error_msg;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_refresh_cb(GLFWwindow *window)
{

}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_mouse_move_cb(GLFWwindow *window, double x, double y)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    if(self->mouse_delegate)
    {
        uint32_t button_mods, key_mods, all_mods;
        get_modifiers(window, button_mods, key_mods);
        all_mods = button_mods | key_mods;
        MouseEvent e(button_mods, (int)x, (int)y, all_mods, glm::ivec2(0));

        if(button_mods && self->mouse_delegate.mouse_drag){ self->mouse_delegate.mouse_drag(e); }
        else if(self->mouse_delegate.mouse_move){ self->mouse_delegate.mouse_move(e); }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_mouse_button_cb(GLFWwindow *window, int button, int action, int modifier_mask)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    if(self->mouse_delegate)
    {
        uint32_t initiator = 0;
        switch(button)
        {
            case GLFW_MOUSE_BUTTON_LEFT:
                initiator = MouseEvent::BUTTON_LEFT;
                break;
            case GLFW_MOUSE_BUTTON_MIDDLE:
                initiator = MouseEvent::BUTTON_MIDDLE;
                break;
            case GLFW_MOUSE_BUTTON_RIGHT:
                initiator = MouseEvent::BUTTON_RIGHT;
                break;
            default:
                break;
        }
        uint32_t button_mods, key_mods, all_mods;
        get_modifiers(window, button_mods, key_mods);
        all_mods = button_mods | key_mods;

        double posX, posY;
        glfwGetCursorPos(window, &posX, &posY);
        MouseEvent e(initiator, (int)posX, (int)posY, all_mods, glm::ivec2(0));

        if(action == GLFW_PRESS && self->mouse_delegate.mouse_press){ self->mouse_delegate.mouse_press(e); }
        else if(action == GLFW_RELEASE && self->mouse_delegate.mouse_release)
        {
            self->mouse_delegate.mouse_release(e);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_mouse_wheel_cb(GLFWwindow *window, double offset_x, double offset_y)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    if(self->mouse_delegate.mouse_wheel)
    {
        glm::ivec2 offset = glm::ivec2(offset_x, offset_y);
        double posX, posY;
        glfwGetCursorPos(window, &posX, &posY);
        uint32_t button_mods, key_mods = 0;
        get_modifiers(window, button_mods, key_mods);
        MouseEvent e(0, (int)posX, (int)posY, key_mods, offset);
        self->mouse_delegate.mouse_wheel(e);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_key_cb(GLFWwindow *window, int key, int scancode, int action, int modifier_mask)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    if(self->key_delegate)
    {
        uint32_t buttonMod, keyMod;
        get_modifiers(window, buttonMod, keyMod);
        KeyEvent e(key, key, keyMod);

        switch(action)
        {
            case GLFW_REPEAT:
            case GLFW_PRESS:
                if(self->key_delegate.key_press){ self->key_delegate.key_press(e); }
                break;

            case GLFW_RELEASE:
                if(self->key_delegate.key_release){ self->key_delegate.key_release(e); }
                break;

            default:
                break;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_char_cb(GLFWwindow *window, unsigned int key)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));
    if(self->key_delegate.character_input){ self->key_delegate.character_input(key); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_file_drop_cb(GLFWwindow *window, int num_files, const char **paths)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    if(self->mouse_delegate.file_drop)
    {
        std::vector<std::string> files(num_files);
        for(int i = 0; i < num_files; i++){ files[i] = paths[i]; }
        uint32_t button_mods, key_mods, all_mods;
        get_modifiers(window, button_mods, key_mods);
        all_mods = button_mods | key_mods;
        double posX, posY;
        glfwGetCursorPos(window, &posX, &posY);
        MouseEvent e(button_mods, (int)posX, (int)posY, all_mods, glm::ivec2(0));
        self->mouse_delegate.file_drop(e, files);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_monitor_cb(GLFWmonitor *the_monitor, int status)
{
    std::string name = glfwGetMonitorName(the_monitor);
    if(status == GLFW_CONNECTED){ LOG_DEBUG << "monitor connected: " << name; }
    else if(status == GLFW_DISCONNECTED){ LOG_DEBUG << "monitor disconnected: " << name; }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_joystick_cb(int joy, int event)
{
    if(event == GLFW_CONNECTED)
    {
        LOG_DEBUG << "joystick " << joy << " connected";
    }else if(event == GLFW_DISCONNECTED)
    {
        LOG_DEBUG << "joystick " << joy << " disconnected";
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
}//namespace vulkan