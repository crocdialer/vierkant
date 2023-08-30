//
// Created by crocdialer on 9/28/18.
//

#pragma once

#include "vierkant/Instance.hpp"
#include "vierkant/Semaphore.hpp"
#include "vierkant/SwapChain.hpp"
#include "vierkant/intersection.hpp"
#include "vierkant/Input.hpp"

struct GLFWwindow;
struct GLFWmonitor;

namespace vierkant
{

class glfw_init_t;
DEFINE_CLASS_PTR(Window)

struct window_delegate_t
{
    struct draw_result_t
    {
        std::vector<VkCommandBuffer> command_buffers;
        std::vector<semaphore_submit_info_t> semaphore_infos;
    };
    using draw_fn_t = std::function<draw_result_t(const WindowPtr &)>;
    using close_fn_t = std::function<void()>;
    using resize_fn_t = std::function<void(uint32_t w, uint32_t h)>;

    //! Callback for draw-operations
    draw_fn_t draw_fn;

    //! Callback for closing the window
    close_fn_t close_fn;

    //! Callback for resizing the window
    resize_fn_t resize_fn;
};

class Window : public std::enable_shared_from_this<Window>
{
public:

    struct create_info_t
    {
        VkInstance instance = VK_NULL_HANDLE;
        glm::ivec2 size = {1920, 1080};
        glm::ivec2 position = {};
        bool fullscreen = false;
        bool vsync = true;
        uint32_t monitor_index;
        VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
        std::string title = "Vierkant";
    };

    //! Delegate objects for mouse callbacks
    std::map<std::string, mouse_delegate_t> mouse_delegates;

    //! Delegate objects for keyboard callbacks
    std::map<std::string, key_delegate_t> key_delegates;

    //! Delegate objects for joystick callbacks
    std::map<std::string, joystick_delegate_t> joystick_delegates;

    //! Delegate objects for window callbacks
    std::map<std::string, window_delegate_t> window_delegates;

    /**
     * @brief   Helper function to retrieve a list of Vulkan-Extensions required for Window
     * @return  a list of Vulkan-Extensions names
     */
    static std::vector<const char *> required_extensions();

    /**
     * @brief               Factory to create a new WindowPtr.
     *
     * New Windows do not have an initialized SwapChain.
     * After Window creation you'll probably want to call @ref create_swapchain at some point
     *
     * @param create_info   a struct grouping all parameters
     * @return              the newly created WindowPtr
     */
    static WindowPtr
    create(const create_info_t &create_info);

    ~Window();

    /**
     * @brief   Draws a frame.
     *          Will create a primary commandbuffer and start a renderpass with current framebuffer,
     *          then gather secondary commandbuffers from the attached draw-delegates and execute them.
     *          Finally the primary commandbuffer is submitted to a graphics-queue and presented to a surface.
     *
     * @param   semaphore_infos an optional array of semaphore_submit_info_t, can be used to pass in signal/wait semaphores
     */
    void draw(std::vector<vierkant::semaphore_submit_info_t> semaphore_infos = {});

    /**
     *  @brief  poll_events.
     */
    void poll_events();

    /**
     * @return  the size of the contained framebuffer in pixels (might be different from the window size)
     */
    glm::ivec2 framebuffer_size() const;

    /**
     * @return  the size of the Window in pixels
     */
    glm::ivec2 size() const;

    /**
     * @brief   set the size of the Window
     *
     * @param   size    the desired window size
     */
    void set_size(const glm::ivec2 &extent);

    /**
     * @return  the current position of the Window
     */
    glm::ivec2 position() const;

    /**
     * @brief   set the position of the Window on screen
     *
     * @param   position    the desired window position
     */
    void set_position(const glm::ivec2 &position);

    /**
     * @return  true if the window is fullscreen
     */
    bool fullscreen() const{ return m_fullscreen; }

    /**
     * @brief   set the window to fullscreen or back.
     *
     * @param   b               a flag indicating fullscreen or not
     * @param   monitor_index   an optional monitor index to use for fullscreen mode
     */
    void set_fullscreen(bool b, uint32_t monitor_index = 0);

    /**
     * @return  the aspect-ratio for the Window
     */
    float aspect_ratio() const
    {
        auto sz = size();
        return sz.x / (float) sz.y;
    }

    /**
     * @return  true if this Window is minimized
     */
    bool is_minimized() const;

    /**
     * @return the Window title
     */
    std::string title() const;

    /**
     * @brief   set the title for this Window
     *
     * @param   title   the desired Window title
     */
    void set_title(const std::string &title);

    /**
     * @return  the current cursor-position relative to this window
     */
    glm::vec2 cursor_position() const;

    /**
     * @brief   set the current mouse-cursor position relative to this window.
     *
     * @param   pos     the desired window position
     */
    void set_cursor_position(const glm::vec2 &pos);

    /**
     * @return  true if the cursor is currently visible
     */
    bool cursor_visible() const;

    /**
     * @brief   sets the mouse-cursor for this window visible or not.
     * @param   b   a flag indicating if the mouse-cursor shall be visible
     */
    void set_cursor_visible(bool b);

    /**
     * @return  the current monitor index this Window resides on
     */
    uint32_t monitor_index() const;

    /**
     * @return  true if this Window was requested to be closed
     */
    bool should_close() const;

//    /**
//     * @return  the managed GLFWwindow handle
//     */
//    inline GLFWwindow *handle(){ return m_handle; }

    /**
     * @return  the VkSurfaceKHR handle for this Window
     */
    VkSurfaceKHR surface() const{ return m_surface; }

    /**
     * @return  the contained SwapChain, which holds the Framebuffers for this Window
     */
    SwapChain &swapchain(){ return m_swap_chain; }

    /**
     * @brief   create an internal SwapChain for this Window.
     *          this is necessary after creating or resizing the Window
     *
     * @param   device      handle for the vk::Device to create the SwapChain with
     * @param   num_samples the desired value for MSAA for the SwapChain
     * @param   v_sync      use vertical synchronization or not
     */
    void create_swapchain(const DevicePtr &device, VkSampleCountFlagBits num_samples = VK_SAMPLE_COUNT_1_BIT,
                          bool v_sync = true);

private:

    explicit Window(const create_info_t &create_info);

    void init_handles(int width, int height, const std::string &title, GLFWmonitor *monitor);

    void clear_handles();

    GLFWwindow *m_handle = nullptr;

    VkInstance m_instance = VK_NULL_HANDLE;

    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    SwapChain m_swap_chain;

    std::string m_title;

    bool m_fullscreen = false;

    // keep track of window params when switching between window/fullscreen
    glm::ivec2 m_window_size{}, m_window_pos{};

    // keep track of previous joystick-states
    std::vector<vierkant::Joystick> m_joysticks;
};

}//namespace vulkan