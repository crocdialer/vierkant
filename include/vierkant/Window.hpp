//
// Created by crocdialer on 9/28/18.
//

#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "vierkant/Instance.hpp"
#include "vierkant/SwapChain.hpp"
#include "vierkant/intersection.hpp"
#include "vierkant/Input.hpp"

namespace vierkant {

DEFINE_CLASS_PTR(Window)

class Window
{
public:
    using draw_fn_t = std::function<void()>;
    using close_fn_t = std::function<void()>;
    using resize_fn_t = std::function<void(uint32_t w, uint32_t h)>;

    // delegates
    MouseDelegate mouse_delegate;

    KeyDelegate key_delegate;

    // callbacks

    // a callback for draw-operations
    draw_fn_t draw_fn;

    close_fn_t close_fn;

    resize_fn_t resize_fn;

    /**
     * @brief   Helper function to retrieve a list of Vulkan-Extensions required for Window
     * @return  a list of Vulkan-Extensions names
     */
    static std::vector<const char *> get_required_extensions();

    /**
     * @brief               Factory to create a new WindowPtr.
     *                      New Windows do not have an initialized SwapChain,
     *                      so after Window creation you'd probably want to call @ref create_swapchain at some point
     *
     * @param instance      a VkInstance to create the Window for
     * @param width         desired width for the Window
     * @param height        desired height for the Window
     * @param title         the Window title
     * @param fullscreen    flag to request a fullscreen Window
     * @param monitor_index monitor index to open the Window on
     * @return              the newly created WindowPtr
     */
    static WindowPtr
    create(VkInstance instance, uint32_t width, uint32_t height, const std::string &the_name = "Vierkant",
           bool fullscreen = false, uint32_t monitor_index = 0);

    ~Window();

    /**
     * @brief   WIP
     */
    void draw();

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
     * @return  the aspect-ratio for the Window
     */
    float aspect_ratio() const
    {
        auto sz = size();
        return sz.x / (float)sz.y;
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
     * @return  the current monitor index this Window resides on
     */
    uint32_t monitor_index() const;

    /**
     * @return  true if this Window was requested to be closed
     */
    bool should_close() const;

    /**
     * @return  the managed GLFWwindow handle
     */
    inline GLFWwindow *handle() { return m_handle; }

    /**
     * @return  the VkSurfaceKHR handle for this Window
     */
    VkSurfaceKHR surface() const { return m_surface; }

    /**
     * @return  the contained SwapChain, which holds the Framebuffers for this Window
     */
    SwapChain &swapchain() { return m_swap_chain; }

    /**
     * @return  the primary command buffer that inits this Window's renderpass
     */
    const CommandBuffer &command_buffer() const { return m_command_buffer; }

//    /**
//     * @brief   set a callback for Draw-operations
//     *
//     * @param   draw_fn     a draw-function
//     */
//    void set_draw_fn(const draw_fn_t &draw_fn) { draw_fn = draw_fn; }
//
//    /**
//     * @brief   set a callback to be called, when this Window closes
//     * @param   close_fn    a close-function
//     */
//    void set_close_fn(const close_fn_t &close_fn) { close_fn = close_fn; }
//
//    /**
//     * @brief   set a callback to be called, when this Window resizes
//     * @param   resize_fn   a resize-function
//     */
//    void set_resize_fn(const resize_fn_t &resize_fn) { resize_fn = resize_fn; }

    /**
     * @brief   create an internal SwapChain for this Window.
     *          this is necessary after creating or resizing the Window
     *
     * @param   device      handle for the vk::Device to create the SwapChain with
     * @param   num_samples the desired value for MSAA for the SwapChain
     */
    void create_swapchain(DevicePtr device, VkSampleCountFlagBits num_samples = VK_SAMPLE_COUNT_1_BIT);

private:

    Window(VkInstance instance, uint32_t width, uint32_t height, const std::string &title,
           bool fullscreen, uint32_t monitor_index);

    void record_command_buffer();

    GLFWwindow *m_handle = nullptr;

    VkInstance m_instance = VK_NULL_HANDLE;

    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    CommandBuffer m_command_buffer;

    SwapChain m_swap_chain;

    std::string m_title;

    static void glfw_resize_cb(GLFWwindow *window, int width, int height);

    static void glfw_close_cb(GLFWwindow *window);

    static void glfw_error_cb(int error_code, const char *error_msg);

    static void glfw_refresh_cb(GLFWwindow *window);

    static void glfw_mouse_move_cb(GLFWwindow *window, double x, double y);

    static void glfw_mouse_button_cb(GLFWwindow *window, int button, int action, int modifier_mask);

    static void glfw_mouse_wheel_cb(GLFWwindow *window, double offset_x, double offset_y);

    static void glfw_key_cb(GLFWwindow *window, int key, int scancode, int action, int modifier_mask);

    static void glfw_char_cb(GLFWwindow *window, unsigned int key);

    static void glfw_file_drop_cb(GLFWwindow *window, int num_files, const char **paths);

    static void glfw_monitor_cb(GLFWmonitor *the_monitor, int);

    static void glfw_joystick_cb(int joy, int event);
};

}//namespace vulkan