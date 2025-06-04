#include "vierkant/Window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace vierkant
{

//! unused
//static void glfw_resize_cb(GLFWwindow *window, int width, int height);
//
//static void glfw_monitor_cb(GLFWmonitor *the_monitor, int);

static void glfw_close_cb(GLFWwindow *window);

static void glfw_error_cb(int error_code, const char *error_msg);

static void glfw_refresh_cb(GLFWwindow *window);

static void glfw_mouse_move_cb(GLFWwindow *window, double x, double y);

static void glfw_mouse_button_cb(GLFWwindow *window, int button, int action, int modifier_mask);

static void glfw_mouse_wheel_cb(GLFWwindow *window, double offset_x, double offset_y);

static void glfw_key_cb(GLFWwindow *window, int key, int scancode, int action, int modifier_mask);

static void glfw_char_cb(GLFWwindow *window, unsigned int key);

static void glfw_file_drop_cb(GLFWwindow *window, int num_files, const char **paths);

static void glfw_joystick_cb(int joy, int event);

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
    //    glfwSetWindowSizeCallback(m_handle, &Window::glfw_resize_cb);
    glfwSetWindowCloseCallback(m_handle, &glfw_close_cb);
    glfwSetMouseButtonCallback(m_handle, &glfw_mouse_button_cb);
    glfwSetCursorPosCallback(m_handle, &glfw_mouse_move_cb);
    glfwSetScrollCallback(m_handle, &glfw_mouse_wheel_cb);
    glfwSetKeyCallback(m_handle, &glfw_key_cb);
    glfwSetCharCallback(m_handle, &glfw_char_cb);
    glfwSetDropCallback(m_handle, &glfw_file_drop_cb);
    glfwSetJoystickCallback(&glfw_joystick_cb);
    glfwSetWindowRefreshCallback(m_handle, &glfw_refresh_cb);
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
    // while window is minimized
    while(is_minimized()) { glfwWaitEvents(); }

    // make sure everything is cleaned up
    // prevents: vkCreateSwapChainKHR(): surface has an existing swapchain other than oldSwapchain
    m_swap_chain = {};

    // create swapchain for this window
    m_swap_chain = SwapChain(device, m_surface, num_samples, v_sync, use_hdr);

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

void Window::set_size(const glm::ivec2 &extent) { glfwSetWindowSize(m_handle, extent.x, extent.y); }

///////////////////////////////////////////////////////////////////////////////////////////////////

glm::ivec2 Window::position() const
{
    glm::ivec2 position;
    glfwGetWindowPos(m_handle, &position.x, &position.y);
    return position;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Window::set_position(const glm::ivec2 &position) { glfwSetWindowPos(m_handle, position.x, position.y); }

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
    if(!m_swap_chain) { return; }

    // increment frame-counter
    m_num_frames++;

    auto acquire_result = m_swap_chain.acquire_next_image();
    auto &framebuffer = swapchain().framebuffers()[acquire_result.image_index];
    
    // sync previous framebuffer
    framebuffer.wait_fence();

    if(acquire_result.result != VK_SUCCESS)
    {
        create_swapchain(m_swap_chain.device(), m_swap_chain.sample_count(), m_swap_chain.v_sync());
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

bool Window::should_close() const { return static_cast<bool>(glfwWindowShouldClose(m_handle)); }

///////////////////////////////////////////////////////////////////////////////////////////////////

//void glfw_resize_cb(GLFWwindow */*window*/, int /*width*/, int /*height*/)
//{
//
//}

///////////////////////////////////////////////////////////////////////////////////////////////////

void glfw_close_cb(GLFWwindow *window)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    for(auto &pair: self->window_delegates)
    {
        if(pair.second.close_fn) { pair.second.close_fn(); }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void glfw_error_cb(int error_code, const char *error_msg) { spdlog::error("{} ({})", error_msg, error_code); }

///////////////////////////////////////////////////////////////////////////////////////////////////

void glfw_refresh_cb(GLFWwindow * /*window*/)
{
    // like resizing!?
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void glfw_mouse_move_cb(GLFWwindow *window, double x, double y)
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

void glfw_mouse_button_cb(GLFWwindow *window, int button, int action, int /*modifier_mask*/)
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

void glfw_mouse_wheel_cb(GLFWwindow *window, double offset_x, double offset_y)
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

void glfw_key_cb(GLFWwindow *window, int key, int /*scancode*/, int action, int /*modifier_mask*/)
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

void glfw_char_cb(GLFWwindow *window, unsigned int key)
{
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    for(auto &pair: self->key_delegates)
    {
        if(pair.second.enabled && !pair.second.enabled()) { continue; }
        if(pair.second.character_input) { pair.second.character_input(key); }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void glfw_file_drop_cb(GLFWwindow *window, int num_files, const char **paths)
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

void glfw_joystick_cb(int joy, int event)
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
}// namespace vierkant