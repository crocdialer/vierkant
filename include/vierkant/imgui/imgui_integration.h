#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
//#define IMGUI_DISABLE_OBSOLETE_KEYIO
#include "imgui.h"
#include "implot.h"
#include "ImGuizmo.h"
#include "vierkant/Rasterizer.hpp"
#include "vierkant/Window.hpp"
#include <filesystem>

namespace vierkant::gui
{

class Context
{
public:

    enum CaptureFlagBits
    {
        WantCaptureMouse = 1 << 0,
        WantCaptureKeyboard = 1 << 1,
        WantTextInput = 1 << 2,
        WantSetMousePos = 1 << 3,
        WantSaveIniSettings = 1 << 4
    };
    using CaptureFlags = uint32_t;

    using draw_fn_t = std::function<void()>;

    struct create_info_t
    {
        std::vector<uint8_t> font_data;
        float font_size = 0.f;
        float ui_scale = 1.f;
    };

    /**
     * @brief   Create a new gui::Context with provided device.
     *
     * @param   device  a shared vierkant::Device to create the gui-assets with.
     */
    Context(const vierkant::DevicePtr &device, const create_info_t &create_info);

    Context() = default;

    Context(const Context &) = delete;

    Context(Context &&other) noexcept;

    ~Context();

    Context &operator=(Context other);

    void update(double time_delta, const glm::vec2 &size);

    /**
     * @brief   Draw the gui using a provided renderer.
     *          Will invoke the current draw-delegate objects to create all gui elements.
     *
     * @param   renderer    a vierkant::Renderer that is used to stage drawables for the gui.
     */
    void draw_gui(vierkant::Rasterizer &renderer);

    [[nodiscard]] const vierkant::mouse_delegate_t &mouse_delegate() const;

    [[nodiscard]] const vierkant::key_delegate_t &key_delegate() const;

    [[nodiscard]] CaptureFlags capture_flags() const;

    friend void swap(Context &lhs, Context &rhs) noexcept;

    //! Delegate functions for gui drawing
    std::map<std::string, draw_fn_t> delegates;

private:

    struct mesh_asset_t
    {
        vierkant::MeshPtr mesh = nullptr;
        vierkant::BufferPtr vertex_buffer = nullptr;
        vierkant::BufferPtr index_buffer = nullptr;
    };

    struct imgui_assets_t
    {
        vierkant::drawable_t drawable = {};
        vierkant::ImagePtr font_texture = nullptr;
        std::vector<std::vector<mesh_asset_t>> frame_assets;
        vierkant::mouse_delegate_t mouse_delegate = {};
        vierkant::key_delegate_t key_delegate = {};
        std::chrono::steady_clock::time_point time_point = std::chrono::steady_clock::now();
    };

    ImGuiIO& get_io();

    static mesh_asset_t create_mesh_assets(const vierkant::DevicePtr &device);

    bool create_device_objects(const vierkant::DevicePtr &device);

    ImGuiContext *m_imgui_context = nullptr;

    ImPlotContext *m_implot_context = nullptr;

    imgui_assets_t m_imgui_assets = {};

    std::unordered_map<int, ImGuiKey> m_key_map;
};

}// namespace