#include "vierkant/Window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace vierkant
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline bool operator==(const videomode_t &lhs, const videomode_t &rhs)
{
    if(lhs.width != rhs.width) { return false; }
    if(lhs.height != rhs.height) { return false; }
    if(lhs.red_bits != rhs.red_bits) { return false; }
    if(lhs.green_bits != rhs.green_bits) { return false; }
    if(lhs.blue_bits != rhs.blue_bits) { return false; }
    if(lhs.refresh_rate != rhs.refresh_rate) { return false; }
    return true;
}

inline bool operator!=(const videomode_t &lhs, const videomode_t &rhs) { return !(lhs == rhs); }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<const char *> Window::required_extensions()
{
    if(!g_glfw_init) { g_glfw_init = std::make_shared<glfw_init_t>(); }

    uint32_t num_extensions = 0;
    const char **extensions = glfwGetRequiredInstanceExtensions(&num_extensions);
    std::vector<const char *> ret(extensions, extensions + num_extensions);
#if defined(__APPLE__)
    ret = {VK_KHR_SURFACE_EXTENSION_NAME, "VK_EXT_metal_surface"};
#endif
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


std::vector<videomode_t> Window::get_video_modes(uint32_t monitor_index)
{
    if(!g_glfw_init) { g_glfw_init = std::make_shared<glfw_init_t>(); }

    int num_monitors = 0;
    GLFWmonitor **monitors = glfwGetMonitors(&num_monitors);
    if(monitor_index >= static_cast<uint32_t>(num_monitors)) { return {}; }

    int num_video_modes = 0;
    const auto *vid_modes_in = glfwGetVideoModes(monitors[monitor_index], &num_video_modes);
    std::vector<videomode_t> ret(num_video_modes);
    for(int i = 0; i < num_video_modes; ++i)
    {
        ret[i].width = vid_modes_in[i].width;
        ret[i].height = vid_modes_in[i].height;
        ret[i].red_bits = vid_modes_in[i].redBits;
        ret[i].green_bits = vid_modes_in[i].greenBits;
        ret[i].blue_bits = vid_modes_in[i].blueBits;
        ret[i].refresh_rate = vid_modes_in[i].refreshRate;
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void get_modifiers(GLFWwindow *window, uint32_t &buttonModifiers, uint32_t &keyModifiers)
{
    buttonModifiers = 0;
    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) { buttonModifiers |= MouseEvent::BUTTON_LEFT; }
    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE)) { buttonModifiers |= MouseEvent::BUTTON_MIDDLE; }
    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)) { buttonModifiers |= MouseEvent::BUTTON_RIGHT; }

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<Joystick> get_joystick_states(const std::vector<Joystick> &previous_joysticks)
{
    std::vector<Joystick> ret;
    int count;
    for(int i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; i++)
    {
        if(!glfwJoystickPresent(i)) { continue; }

        const float *glfw_axis = glfwGetJoystickAxes(i, &count);
        std::vector<float> axis(glfw_axis, glfw_axis + count);

        const uint8_t *glfw_buttons = glfwGetJoystickButtons(i, &count);
        std::vector<uint8_t> buttons(glfw_buttons, glfw_buttons + count);

        std::string name(glfwGetJoystickName(i));

        std::vector<uint8_t> previous_buttons;
        if(static_cast<uint32_t>(i) < previous_joysticks.size()) { previous_buttons = previous_joysticks[i].buttons(); }
        ret.emplace_back(std::move(name), std::move(buttons), std::move(axis), previous_buttons);
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

WindowPtr Window::create(const create_info_t &create_info) { return WindowPtr(new Window(create_info)); }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Window::Window(const create_info_t &create_info)
    : m_instance(create_info.instance), m_fullscreen(create_info.fullscreen), m_enable_joysticks(create_info.joysticks),
      m_window_size(create_info.size), m_window_pos(create_info.position)
{
    if(!g_glfw_init) { g_glfw_init = std::make_shared<glfw_init_t>(); }

    int monitor_count = 0;
    GLFWmonitor **monitors = glfwGetMonitors(&monitor_count);
    uint32_t monitor_index = std::max(static_cast<int>(create_info.monitor_index), monitor_count - 1);

    init_handles(m_window_size.x, m_window_size.y, create_info.title,
                 create_info.fullscreen ? monitors[monitor_index] : nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Window::~Window() { clear_handles(); }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
    if(!m_handle) { throw std::runtime_error("could not init window-handles"); }

    vkCheck(glfwCreateWindowSurface(m_instance, m_handle, nullptr, &m_surface), "failed to create window surface!");

    // set user-pointer
    glfwSetWindowUserPointer(m_handle, this);

    // init callbacks
    glfwSetErrorCallback(&glfw_error_cb);
    glfwSetWindowSizeCallback(m_handle, &Window::glfw_resize_cb);
    glfwSetWindowPosCallback(m_handle, &Window::glfw_pos_cb);
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
    if(m_instance && m_surface) { vkDestroySurfaceKHR(m_instance, m_surface, nullptr); }
    if(m_handle) { glfwDestroyWindow(m_handle); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::create_swapchain(const DevicePtr &device, VkSampleCountFlagBits num_samples, bool v_sync, bool use_hdr)
{
    m_need_resize_swapchain = false;

    // while window is minimized
    while(is_minimized()) { glfwWaitEvents(); }

    // make sure everything is cleaned up
    // prevents: vkCreateSwapChainKHR(): surface has an existing swapchain other than oldSwapchain
    m_swap_chain = {};

    // create swapchain for this window
    auto fb_size = framebuffer_size();
    m_swap_chain = SwapChain(device, m_surface, num_samples, v_sync, use_hdr,
                             VkExtent2D{static_cast<uint32_t>(fb_size.x), static_cast<uint32_t>(fb_size.y)});

    for(auto &pair: window_delegates)
    {
        if(pair.second.resize_fn) { pair.second.resize_fn(m_swap_chain.extent().width, m_swap_chain.extent().height); }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Window::poll_events()
{
    glfwPollEvents();

    if(m_enable_joysticks && !joystick_delegates.empty())
    {
        m_joysticks = get_joystick_states(m_joysticks);

        for(auto &[name, delegate]: joystick_delegates)
        {
            if(delegate.joystick_cb && (!delegate.enabled || delegate.enabled())) { delegate.joystick_cb(m_joysticks); }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

glm::ivec2 Window::size() const
{
    // glm::ivec2 ret;
    // glfwGetWindowSize(m_handle, &ret.x, &ret.y);
    // return ret;
    return m_window_size;
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
    m_window_size = extent;
    glfwSetWindowSize(m_handle, extent.x, extent.y);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

glm::ivec2 Window::position() const
{
    // if(m_fullscreen) { return {0, 0}; }
    // else
    // {
    //     glm::ivec2 position;
    //     glfwGetWindowPos(m_handle, &position.x, &position.y);
    //     return position;
    // }
    return m_window_pos;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::set_position(const glm::ivec2 &position)
{
    m_window_pos = position;
    glfwSetWindowPos(m_handle, position.x, position.y);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

std::string Window::title() const { return m_title; }

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

void Window::set_cursor_position(const glm::vec2 &pos) { glfwSetCursorPos(m_handle, pos.x, pos.y); }

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Window::cursor_visible() const { return glfwGetInputMode(m_handle, GLFW_CURSOR) != GLFW_CURSOR_HIDDEN; }

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::set_cursor_visible(bool b)
{
    glfwSetInputMode(m_handle, GLFW_CURSOR, b ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::draw(std::vector<vierkant::semaphore_submit_info_t> semaphore_infos)
{
    auto recreate_swapchain = [&]() {
        create_swapchain(m_swap_chain.device(), m_swap_chain.sample_count(), m_swap_chain.v_sync());
    };

    if(!m_swap_chain) { return; }

    // increment frame-counter
    m_num_frames++;

    auto acquire_result = m_swap_chain.acquire_next_image();
    auto &framebuffer = swapchain().framebuffers()[acquire_result.image_index];

    // sync previous framebuffer
    framebuffer.wait_fence();

    if(acquire_result.result != VK_SUCCESS)
    {
        recreate_swapchain();
        spdlog::warn("acquire_next_image failed");
        return;
    }

    std::vector<VkCommandBuffer> commandbuffers;

    // create secondary commandbuffers
    for(auto &[delegate_id, delegate]: window_delegates)
    {
        if(delegate.draw_fn)
        {
            auto draw_result = delegate.draw_fn(shared_from_this());
            commandbuffers.insert(commandbuffers.end(), draw_result.command_buffers.begin(),
                                  draw_result.command_buffers.end());
            semaphore_infos.insert(semaphore_infos.end(), draw_result.semaphore_infos.begin(),
                                   draw_result.semaphore_infos.end());
        }
    }

    semaphore_submit_info_t image_available = {};
    image_available.semaphore = acquire_result.image_available;
    image_available.wait_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    semaphore_infos.push_back(image_available);

    semaphore_submit_info_t render_finished = {};
    render_finished.semaphore = acquire_result.render_finished;
    render_finished.signal_value = 1;
    render_finished.signal_stage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    semaphore_infos.push_back(render_finished);

    // execute all commands, submit primary commandbuffer
    framebuffer.submit(commandbuffers, m_swap_chain.device()->queue(), semaphore_infos);

    // present the image (submit to presentation-queue, wait for fences)
    VkResult result = m_swap_chain.present();

    if(m_need_resize_swapchain || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        recreate_swapchain();
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

    wx = m_window_pos.x;
    wy = m_window_pos.y;
    ww = m_window_size.x;
    wh = m_window_size.y;
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

bool Window::should_close() const { return static_cast<bool>(glfwWindowShouldClose(m_handle)); }

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_resize_cb(GLFWwindow *window, int width, int height)
{
    spdlog::debug("window resized: {} x {}", width, height);
    auto *self = static_cast<Window *>(glfwGetWindowUserPointer(window));
    self->m_window_size = {width, height};
    self->m_need_resize_swapchain = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_pos_cb(GLFWwindow *window, int x, int y)
{
    spdlog::debug("window position: {} x {}", x, y);
    auto *self = static_cast<Window *>(glfwGetWindowUserPointer(window));
    self->m_window_pos = {x, y};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_close_cb(GLFWwindow *window)
{
    auto *self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    for(auto &pair: self->window_delegates)
    {
        if(pair.second.close_fn) { pair.second.close_fn(); }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_error_cb(int error_code, const char *error_msg) { spdlog::error("{} ({})", error_msg, error_code); }

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_refresh_cb(GLFWwindow * /*window*/)
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
        MouseEvent e(static_cast<int>(button_mods), (int) x, (int) y, all_mods, glm::ivec2(0));

        for(auto &pair: self->mouse_delegates)
        {
            if(pair.second.enabled && !pair.second.enabled()) { continue; }
            if(pair.second.mouse_move) { pair.second.mouse_move(e); }
            if(button_mods && pair.second.mouse_drag) { pair.second.mouse_drag(e); }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_mouse_button_cb(GLFWwindow *window, int button, int action, int /*modifier_mask*/)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    if(!self->mouse_delegates.empty())
    {
        uint32_t initiator = 0;
        switch(button)
        {
            case GLFW_MOUSE_BUTTON_LEFT: initiator = MouseEvent::BUTTON_LEFT; break;
            case GLFW_MOUSE_BUTTON_MIDDLE: initiator = MouseEvent::BUTTON_MIDDLE; break;
            case GLFW_MOUSE_BUTTON_RIGHT: initiator = MouseEvent::BUTTON_RIGHT; break;
            default: break;
        }
        uint32_t button_mods, key_mods, all_mods;
        get_modifiers(window, button_mods, key_mods);
        all_mods = button_mods | key_mods;

        double posX, posY;
        glfwGetCursorPos(window, &posX, &posY);
        MouseEvent e(static_cast<int>(initiator), (int) posX, (int) posY, all_mods, glm::ivec2(0));

        for(auto &pair: self->mouse_delegates)
        {
            if(pair.second.enabled && !pair.second.enabled()) { continue; }
            if(action == GLFW_PRESS && pair.second.mouse_press) { pair.second.mouse_press(e); }
            else if(action == GLFW_RELEASE && pair.second.mouse_release) { pair.second.mouse_release(e); }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_mouse_wheel_cb(GLFWwindow *window, double offset_x, double offset_y)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    if(!self->mouse_delegates.empty())
    {
        for(auto &pair: self->mouse_delegates)
        {
            if(pair.second.enabled && !pair.second.enabled()) { continue; }
            glm::ivec2 offset = glm::ivec2(offset_x, offset_y);
            double posX, posY;
            glfwGetCursorPos(window, &posX, &posY);
            uint32_t button_mods, key_mods = 0;
            get_modifiers(window, button_mods, key_mods);
            MouseEvent e(0, (int) posX, (int) posY, key_mods, offset);

            if(pair.second.mouse_wheel) { pair.second.mouse_wheel(e); }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_key_cb(GLFWwindow *window, int key, int /*scancode*/, int action, int /*modifier_mask*/)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    if(!self->key_delegates.empty())
    {
        uint32_t buttonMod, keyMod;
        get_modifiers(window, buttonMod, keyMod);
        KeyEvent e(key, key, keyMod);

        for(auto &pair: self->key_delegates)
        {
            if(pair.second.enabled && !pair.second.enabled()) { continue; }

            switch(action)
            {
                case GLFW_REPEAT:
                case GLFW_PRESS:
                    if(pair.second.key_press) { pair.second.key_press(e); }
                    break;

                case GLFW_RELEASE:
                    if(pair.second.key_release) { pair.second.key_release(e); }
                    break;

                default: break;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_char_cb(GLFWwindow *window, unsigned int key)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    for(auto &pair: self->key_delegates)
    {
        if(pair.second.enabled && !pair.second.enabled()) { continue; }
        if(pair.second.character_input) { pair.second.character_input(key); }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_file_drop_cb(GLFWwindow *window, int num_files, const char **paths)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    if(!self->mouse_delegates.empty())
    {
        std::vector<std::string> files(num_files);
        for(int i = 0; i < num_files; i++) { files[i] = paths[i]; }
        uint32_t button_mods, key_mods, all_mods;
        get_modifiers(window, button_mods, key_mods);
        all_mods = button_mods | key_mods;
        double posX, posY;
        glfwGetCursorPos(window, &posX, &posY);
        MouseEvent e(static_cast<int>(button_mods), (int) posX, (int) posY, all_mods, glm::ivec2(0));

        for(auto &pair: self->mouse_delegates)
        {
            if(pair.second.enabled && !pair.second.enabled()) { continue; }
            if(pair.second.file_drop) { pair.second.file_drop(e, files); }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

//void glfw_monitor_cb(GLFWmonitor *the_monitor, int status)
//{
//    std::string name = glfwGetMonitorName(the_monitor);
//    if(status == GLFW_CONNECTED){ spdlog::debug("monitor connected: {}", name); }
//    else if(status == GLFW_DISCONNECTED){ spdlog::debug("monitor disconnected: {}", name); }
//}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::glfw_joystick_cb(int joy, int event)
{
    if(event == GLFW_CONNECTED) { spdlog::debug("{} connected ({})", glfwGetJoystickName(joy), joy); }
    else if(event == GLFW_DISCONNECTED) { spdlog::debug("disconnected joystick ({})", joy); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::set_fullscreen(bool b, uint32_t monitor_index)
{
    if(b == m_fullscreen) { return; }

    int num;
    GLFWmonitor **monitors = glfwGetMonitors(&num);
    if(monitor_index >= static_cast<uint32_t>(num)) { return; }
    const GLFWvidmode *mode = glfwGetVideoMode(monitors[monitor_index]);

    int w, h, x, y;

    if(b)
    {
        // m_window_size = size();
        // m_window_pos = position();
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

void Window::set_fullscreen_mode(const videomode_t &video_mode, uint32_t monitor_index)
{
    int num;
    GLFWmonitor **monitors = glfwGetMonitors(&num);
    if(monitor_index >= static_cast<uint32_t>(num)) { return; }
    const GLFWvidmode *mode_glfw = glfwGetVideoMode(monitors[monitor_index]);
    videomode_t current_mode{static_cast<uint32_t>(mode_glfw->width),    static_cast<uint32_t>(mode_glfw->height),
                             static_cast<uint32_t>(mode_glfw->redBits),  static_cast<uint32_t>(mode_glfw->greenBits),
                             static_cast<uint32_t>(mode_glfw->blueBits), static_cast<uint32_t>(mode_glfw->refreshRate)};

    if(video_mode != current_mode)
    {
        if(!m_fullscreen)
        {
            m_fullscreen = true;
            // m_window_size = size();
            // m_window_pos = position();
        }

        glfwSetWindowMonitor(m_handle, monitors[monitor_index], 0, 0, video_mode.width, video_mode.height,
                             video_mode.refresh_rate);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
}// namespace vierkant