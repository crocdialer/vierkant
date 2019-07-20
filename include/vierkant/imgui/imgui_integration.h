#include "vierkant/Window.hpp"
#include "vierkant/Renderer.hpp"
#include "imgui.h"
#include "ImGuizmo.h"

namespace vierkant::gui {

class Context
{
public:

    using draw_fn_t = std::function<void()>;

    //! Delegate functions for gui drawing
    std::map<std::string, draw_fn_t> delegates;

    explicit Context(const vierkant::WindowPtr &w);

    Context() = default;

    Context(const Context &) = delete;

    Context(Context &&other) noexcept;

    ~Context();

    Context &operator=(Context other);

    void render(vierkant::Renderer &renderer);

    friend void swap(Context &lhs, Context& rhs) noexcept;

private:

    struct mesh_asset_t
    {
        vierkant::MeshPtr mesh = nullptr;
        vierkant::BufferPtr vertex_buffer = nullptr;
        vierkant::BufferPtr index_buffer = nullptr;
    };

    struct imgui_assets_t
    {
        vierkant::Renderer::drawable_t drawable = {};
        vierkant::ImagePtr font_texture = nullptr;
        std::vector<std::vector<mesh_asset_t>> frame_assets;
        std::chrono::steady_clock::time_point time_point = std::chrono::steady_clock::now();
    };

    mesh_asset_t create_window_assets(const vierkant::DevicePtr &device);

    bool create_device_objects(const vierkant::DevicePtr &device);

    ImGuiContext* m_imgui_context = nullptr;

    imgui_assets_t m_imgui_assets = {};
};

}// namespace