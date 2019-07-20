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

    /**
     * @brief   Create a new gui::Context with provided device.
     *
     * @param   device  a shared vierkant::Device to create the gui-assets with.
     */
    explicit Context(const vierkant::DevicePtr &device);

    Context() = default;

    Context(const Context &) = delete;

    Context(Context &&other) noexcept;

    ~Context();

    Context &operator=(Context other);

    /**
     * @brief   Render the gui using a provided renderer.
     *          Will invoke the current draw-delegate objects to create all gui elements.
     *
     * @param   renderer    a vierkant::Renderer that is used to stage drawables for the gui.
     */
    void render(vierkant::Renderer &renderer);

    const vierkant::mouse_delegate_t &mouse_delegate() const;

    const vierkant::key_delegate_t &key_delegate() const;

    friend void swap(Context &lhs, Context &rhs) noexcept;

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
        vierkant::mouse_delegate_t mouse_delegate = {};
        vierkant::key_delegate_t key_delegate = {};
        std::chrono::steady_clock::time_point time_point = std::chrono::steady_clock::now();
    };

    mesh_asset_t create_mesh_assets(const vierkant::DevicePtr &device);

    bool create_device_objects(const vierkant::DevicePtr &device);

    ImGuiContext *m_imgui_context = nullptr;

    imgui_assets_t m_imgui_assets = {};
};

}// namespace