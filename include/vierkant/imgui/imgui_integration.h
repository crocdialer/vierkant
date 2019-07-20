#include "vierkant/Window.hpp"
#include "vierkant/Renderer.hpp"
#include "imgui.h"
#include "ImGuizmo.h"

namespace vierkant::gui {

class Context
{
public:

    explicit Context(const vierkant::WindowPtr &w);

    Context() = default;

    Context(const Context &) = delete;

    Context(Context &&other) noexcept;

    ~Context();

    Context &operator=(Context other);

    void render(vierkant::Renderer &renderer);

    void new_frame(const glm::vec2 &size, float delta_time);

    void set_current();

    friend void swap(Context &lhs, Context& rhs) noexcept;

private:

    struct mesh_asset_t
    {
        vierkant::MeshPtr mesh;
        vierkant::BufferPtr vertex_buffer;
        vierkant::BufferPtr index_buffer;
    };

    struct imgui_assets_t
    {
        vierkant::Renderer::drawable_t drawable;
        vierkant::ImagePtr font_texture;
        std::vector<std::vector<mesh_asset_t>> frame_assets;
    };

    mesh_asset_t create_window_assets(const vierkant::DevicePtr &device);

    bool create_device_objects(const vierkant::DevicePtr &device);

    ImGuiContext* m_imgui_context = nullptr;

    imgui_assets_t m_imgui_assets = {};
};

}// namespace