//
// Created by crocdialer on 9/28/18.
//

#include "vierkant/Window.hpp"

namespace vierkant
{

/**
 * @brief RAII Helper for glfw initialization and termination
 */
class glfw_init_t
{
public:
    glfw_init_t(){ glfwInit(); }

    ~glfw_init_t(){ glfwTerminate(); }
};

static std::shared_ptr<glfw_init_t> g_glfw_init;

///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<const char *> Window::required_extensions()
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
    {
        buttonModifiers |= MouseEvent::BUTTON_LEFT;
    }
    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE))
    {
        buttonModifiers |= MouseEvent::BUTTON_MIDDLE;
    }
    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT))
    {
        buttonModifiers |= MouseEvent::BUTTON_RIGHT;
    }

    keyModifiers = 0;
    if(glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL))
    {
        keyModifiers |= KeyEvent::CTRL_DOWN;
    }
    if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT))
    {
        keyModifiers |= KeyEvent::SHIFT_DOWN;
    }
    if(glfwGetKey(window, GLFW_KEY_LEFT_ALT) || glfwGetKey(window, GLFW_KEY_RIGHT_ALT))
    {
        keyModifiers |= KeyEvent::ALT_DOWN;
    }
    if(glfwGetKey(window, GLFW_KEY_LEFT_SUPER) || glfwGetKey(window, GLFW_KEY_RIGHT_SUPER))
    {
        keyModifiers |= KeyEvent::META_DOWN;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

WindowPtr Window::create(const create_info_t &create_info)
{
    return WindowPtr(new Window(create_info));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Window::Window(const create_info_t &create_info) :
        m_instance(create_info.instance),
        m_fullscreen(create_info.fullscreen),
        m_window_size(create_info.size),
        m_window_pos(create_info.position)
{
    if(!g_glfw_init){ g_glfw_init = std::make_shared<glfw_init_t>(); }

    int monitor_count = 0;
    GLFWmonitor **monitors = glfwGetMonitors(&monitor_count);
    uint32_t monitor_index = std::max<int>(create_info.monitor_index, monitor_count - 1);

    init_handles(m_window_size.x, m_window_size.y, create_info.title,
                 create_info.fullscreen ? monitors[monitor_index] : nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Window::~Window()
{
    clear_handles();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::init_handles(int width, int height, const std::string &title, GLFWmonitor *monitor)
{
    clear_handles();

    if(monitor)
    {
        // TODO: check for available modes and pick closest
        const GLFWvidmode *mode = glfwGetVideoMode(monitor);
        width = mode->width;
        height = mode->height;
    }

    // no GL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_handle = glfwCreateWindow(width, height, title.c_str(), monitor, nullptr);
    if(!m_handle){ throw std::runtime_error("could not init window-handles"); }

    vkCheck(glfwCreateWindowSurface(m_instance, m_handle, nullptr, &m_surface),
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
    glfwSetJoystickCallback(&Window::glfw_joystick_cb);
    glfwSetWindowRefreshCallback(m_handle, &Window::glfw_refresh_cb);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::clear_handles()
{
    m_swap_chain = SwapChain();
    if(m_instance && m_surface){ vkDestroySurfaceKHR(m_instance, m_surface, nullptr); }
    if(m_handle){ glfwDestroyWindow(m_handle); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::create_swapchain(const DevicePtr &device, VkSampleCountFlagBits num_samples, bool v_sync)
{
    // while window is minimized
    while(is_minimized()){ glfwWaitEvents(); }

    // wait for good measure
    vkDeviceWaitIdle(device->handle());

    // make sure everything is cleaned up
    // prevents: vkCreateSwapChainKHR(): surface has an existing swapchain other than oldSwapchain
    m_swap_chain = SwapChain();

    // wait for good measure
    vkDeviceWaitIdle(device->handle());

    // create swapchain for this window
    m_swap_chain = SwapChain(device, m_surface, num_samples, v_sync);

    for(auto &pair : window_delegates)
    {
        if(pair.second.resize_fn){ pair.second.resize_fn(m_swap_chain.extent().width, m_swap_chain.extent().height); }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Window::poll_events()
{
    glfwPollEvents();

    if(!joystick_delegates.empty())
    {
        auto states = get_joystick_states();

        for(auto &[name, delegate] : joystick_delegates)
        {
            if(delegate.enabled && delegate.joystick_cb){ delegate.joystick_cb(states); }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

glm::vec2 Window::cursor_position() const
{
    double x_pos, y_pos;
    glfwGetCursorPos(m_handle, &x_pos, &y_pos);
    return {x_pos, y_pos};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::set_cursor_position(const glm::vec2 &pos)
{
    glfwSetCursorPos(m_handle, pos.x, pos.y);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Window::cursor_visible() const
{
    return glfwGetInputMode(m_handle, GLFW_CURSOR) != GLFW_CURSOR_HIDDEN;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::set_cursor_visible(bool b)
{
    glfwSetInputMode(m_handle, GLFW_CURSOR, b ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::draw(std::vector<vierkant::semaphore_submit_info_t> semaphore_infos)
{
    if(!m_swap_chain){ return; }

    auto sync_objects = m_swap_chain.sync_objects();
    uint32_t image_index;

    if(!m_swap_chain.aquire_next_image(&image_index))
    {
        create_swapchain(m_swap_chain.device(), m_swap_chain.sample_count(), m_swap_chain.v_sync());
        return;
    }

    auto &framebuffer = swapchain().framebuffers()[image_index];

    // wait for prior frame to finish
    framebuffer.wait_fence();

    std::vector<VkCommandBuffer> commandbuffers;

    // create secondary commandbuffers
    for(auto &[delegate_id, delegate] : window_delegates)
    {
        if(delegate.draw_fn)
        {
            auto draw_result = delegate.draw_fn(shared_from_this());
            commandbuffers.insert(commandbuffers.end(),
                                  draw_result.command_buffers.begin(),
                                  draw_result.command_buffers.end());
            semaphore_infos.insert(semaphore_infos.end(),
                                   draw_result.semaphore_infos.begin(),
                                   draw_result.semaphore_infos.end());
        }
    }

    semaphore_submit_info_t image_available = {};
    image_available.semaphore = sync_objects.image_available;
    image_available.wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    semaphore_infos.push_back(image_available);

    semaphore_submit_info_t render_finished = {};
    render_finished.semaphore = sync_objects.render_finished;
    render_finished.signal_value = 1;
    semaphore_infos.push_back(render_finished);

    // execute all commands, submit primary commandbuffer
    framebuffer.submit(commandbuffers, m_swap_chain.device()->queue(), semaphore_infos);

    // present the image (submit to presentation-queue, wait for fences)
    VkResult result = m_swap_chain.present();

    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        create_swapchain(m_swap_chain.device(), m_swap_chain.sample_count(), m_swap_chain.v_sync());
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

}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_close_cb(GLFWwindow *window)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    for(auto &pair : self->window_delegates)
    {
        if(pair.second.close_fn){ pair.second.close_fn(); }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_error_cb(int error_code, const char *error_msg)
{
    LOG_ERROR << error_msg;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_refresh_cb(GLFWwindow *window)
{
    // like resizing!?
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_mouse_move_cb(GLFWwindow *window, double x, double y)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    if(!self->mouse_delegates.empty())
    {
        uint32_t button_mods, key_mods, all_mods;
        get_modifiers(window, button_mods, key_mods);
        all_mods = button_mods | key_mods;
        MouseEvent e(button_mods, (int) x, (int) y, all_mods, glm::ivec2(0));

        for(auto &pair : self->mouse_delegates)
        {
            if(pair.second.enabled && !pair.second.enabled()){ continue; }
            if(pair.second.mouse_move){ pair.second.mouse_move(e); }
            if(button_mods && pair.second.mouse_drag){ pair.second.mouse_drag(e); }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_mouse_button_cb(GLFWwindow *window, int button, int action, int modifier_mask)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    if(!self->mouse_delegates.empty())
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
        MouseEvent e(initiator, (int) posX, (int) posY, all_mods, glm::ivec2(0));

        for(auto &pair : self->mouse_delegates)
        {
            if(pair.second.enabled && !pair.second.enabled()){ continue; }
            if(action == GLFW_PRESS && pair.second.mouse_press){ pair.second.mouse_press(e); }
            else if(action == GLFW_RELEASE && pair.second.mouse_release)
            {
                pair.second.mouse_release(e);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_mouse_wheel_cb(GLFWwindow *window, double offset_x, double offset_y)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    if(!self->mouse_delegates.empty())
    {
        for(auto &pair : self->mouse_delegates)
        {
            if(pair.second.enabled && !pair.second.enabled()){ continue; }
            glm::ivec2 offset = glm::ivec2(offset_x, offset_y);
            double posX, posY;
            glfwGetCursorPos(window, &posX, &posY);
            uint32_t button_mods, key_mods = 0;
            get_modifiers(window, button_mods, key_mods);
            MouseEvent e(0, (int) posX, (int) posY, key_mods, offset);

            if(pair.second.mouse_wheel){ pair.second.mouse_wheel(e); }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_key_cb(GLFWwindow *window, int key, int scancode, int action, int modifier_mask)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    if(!self->key_delegates.empty())
    {
        uint32_t buttonMod, keyMod;
        get_modifiers(window, buttonMod, keyMod);
        KeyEvent e(key, key, keyMod);

        for(auto &pair : self->key_delegates)
        {
            if(pair.second.enabled && !pair.second.enabled()){ continue; }

            switch(action)
            {
                case GLFW_REPEAT:
                case GLFW_PRESS:
                    if(pair.second.key_press){ pair.second.key_press(e); }
                    break;

                case GLFW_RELEASE:
                    if(pair.second.key_release){ pair.second.key_release(e); }
                    break;

                default:
                    break;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_char_cb(GLFWwindow *window, unsigned int key)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    for(auto &pair : self->key_delegates)
    {
        if(pair.second.enabled && !pair.second.enabled()){ continue; }
        if(pair.second.character_input){ pair.second.character_input(key); }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_file_drop_cb(GLFWwindow *window, int num_files, const char **paths)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    if(!self->mouse_delegates.empty())
    {
        std::vector<std::string> files(num_files);
        for(int i = 0; i < num_files; i++){ files[i] = paths[i]; }
        uint32_t button_mods, key_mods, all_mods;
        get_modifiers(window, button_mods, key_mods);
        all_mods = button_mods | key_mods;
        double posX, posY;
        glfwGetCursorPos(window, &posX, &posY);
        MouseEvent e(button_mods, (int) posX, (int) posY, all_mods, glm::ivec2(0));

        for(auto &pair : self->mouse_delegates)
        {
            if(pair.second.enabled && !pair.second.enabled()){ continue; }
            if(pair.second.file_drop){ pair.second.file_drop(e, files); }
        }
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
    }
    else if(event == GLFW_DISCONNECTED)
    {
        LOG_DEBUG << "joystick " << joy << " disconnected";
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::set_fullscreen(bool b, uint32_t monitor_index)
{
    if(b == m_fullscreen){ return; }

    int num;
    GLFWmonitor **monitors = glfwGetMonitors(&num);
    if(monitor_index >= static_cast<uint32_t>(num)){ return; }
    const GLFWvidmode *mode = glfwGetVideoMode(monitors[monitor_index]);

    int w, h, x, y;

    if(b)
    {
        m_window_size = size();
        m_window_pos = position();
        w = mode->width;
        h = mode->height;
        x = y = 0;
    }
    else
    {
        w = m_window_size.x;
        h = m_window_size.y;
        x = m_window_pos.x;
        y = m_window_pos.y;
    }
    glfwSetWindowMonitor(m_handle, b ? monitors[monitor_index] : nullptr, x, y, w, h, mode->refreshRate);
    m_fullscreen = b;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
}//namespace vulkan