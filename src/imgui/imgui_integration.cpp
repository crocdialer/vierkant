#include "vierkant/imgui/imgui_integration.h"
#include "imgui_internal.h"
#include <crocore/Area.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/Pipeline.hpp>

namespace vierkant::gui
{

// 1 float per second
using float_sec_t = std::chrono::duration<float, std::chrono::seconds::period>;

void set_style()
{
    //    ImVec4* colors = ImGui::GetStyle().Colors;
    //    colors[ImGuiCol_Text]                   = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    //    colors[ImGuiCol_TextDisabled]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    //    colors[ImGuiCol_WindowBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.70f);
    //    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    //    colors[ImGuiCol_PopupBg]                = ImVec4(0.11f, 0.11f, 0.14f, 0.92f);
    //    colors[ImGuiCol_Border]                 = ImVec4(0.50f, 0.50f, 0.50f, 0.50f);
    //    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    //    colors[ImGuiCol_FrameBg]                = ImVec4(0.43f, 0.43f, 0.43f, 0.39f);
    //    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.47f, 0.47f, 0.69f, 0.40f);
    //    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.42f, 0.41f, 0.64f, 0.69f);
    //    colors[ImGuiCol_TitleBg]                = ImVec4(0.27f, 0.27f, 0.54f, 0.83f);
    //    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.32f, 0.32f, 0.63f, 0.87f);
    //    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.40f, 0.40f, 0.80f, 0.20f);
    //    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.40f, 0.40f, 0.55f, 0.80f);
    //    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.20f, 0.25f, 0.30f, 0.60f);
    //    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.40f, 0.40f, 0.80f, 0.30f);
    //    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.40f, 0.40f, 0.80f, 0.40f);
    //    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.41f, 0.39f, 0.80f, 0.60f);
    //    colors[ImGuiCol_CheckMark]              = ImVec4(0.90f, 0.90f, 0.90f, 0.50f);
    //    colors[ImGuiCol_SliderGrab]             = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
    //    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.41f, 0.39f, 0.80f, 0.60f);
    //    colors[ImGuiCol_Button]                 = ImVec4(0.35f, 0.40f, 0.61f, 0.62f);
    //    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.40f, 0.48f, 0.71f, 0.79f);
    //    colors[ImGuiCol_ButtonActive]           = ImVec4(0.46f, 0.54f, 0.80f, 1.00f);
    //    colors[ImGuiCol_Header]                 = ImVec4(0.40f, 0.40f, 0.90f, 0.45f);
    //    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.45f, 0.45f, 0.90f, 0.80f);
    //    colors[ImGuiCol_HeaderActive]           = ImVec4(0.53f, 0.53f, 0.87f, 0.80f);
    //    colors[ImGuiCol_Separator]              = ImVec4(0.50f, 0.50f, 0.50f, 0.60f);
    //    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.60f, 0.60f, 0.70f, 1.00f);
    //    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.70f, 0.70f, 0.90f, 1.00f);
    //    colors[ImGuiCol_ResizeGrip]             = ImVec4(1.00f, 1.00f, 1.00f, 0.16f);
    //    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.78f, 0.82f, 1.00f, 0.60f);
    //    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.78f, 0.82f, 1.00f, 0.90f);
    //    colors[ImGuiCol_Tab]                    = ImVec4(0.34f, 0.34f, 0.68f, 0.79f);
    //    colors[ImGuiCol_TabHovered]             = ImVec4(0.45f, 0.45f, 0.90f, 0.80f);
    //    colors[ImGuiCol_TabActive]              = ImVec4(0.40f, 0.40f, 0.73f, 0.84f);
    //    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.28f, 0.28f, 0.57f, 0.82f);
    //    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.35f, 0.35f, 0.65f, 0.84f);
    //    colors[ImGuiCol_PlotLines]              = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    //    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    //    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    //    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    //    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.00f, 0.00f, 1.00f, 0.35f);
    //    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    //    colors[ImGuiCol_NavHighlight]           = ImVec4(0.45f, 0.45f, 0.90f, 0.80f);
    //    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    //    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    //    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
}


void mouse_press(ImGuiContext *ctx, const MouseEvent &e);

void mouse_release(ImGuiContext *ctx, const MouseEvent &e);

void mouse_wheel(ImGuiContext *ctx, const MouseEvent &e);

void mouse_move(ImGuiContext *ctx, const MouseEvent &e);

void key_press(ImGuiContext *ctx, const KeyEvent &e, const std::unordered_map<int, ImGuiKey> &keymap);

void key_release(ImGuiContext *ctx, const KeyEvent &e, const std::unordered_map<int, ImGuiKey> &keymap);

void character_input(ImGuiContext *ctx, uint32_t c);

///////////////////////////////////////////////////////////////////////////////////////////////////

Context::mesh_asset_t Context::create_mesh_assets(const vierkant::DevicePtr &device)
{
    // dynamic vertexbuffer objects
    auto vertex_buffer = vierkant::Buffer::create(device, nullptr, 0,
                                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                  VMA_MEMORY_USAGE_CPU_TO_GPU);

    auto index_buffer = vierkant::Buffer::create(device, nullptr, 0,
                                                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

    auto mesh = vierkant::Mesh::create();

    // vertex attrib -> position
    vierkant::vertex_attrib_t position_attrib;
    position_attrib.offset = offsetof(ImDrawVert, pos);
    position_attrib.stride = sizeof(ImDrawVert);
    position_attrib.buffer = vertex_buffer;
    position_attrib.buffer_offset = 0;
    position_attrib.format = vierkant::format<glm::vec2>();
    mesh->vertex_attribs[vierkant::Mesh::ATTRIB_POSITION] = position_attrib;

    // vertex attrib -> color
    vierkant::vertex_attrib_t color_attrib;
    color_attrib.offset = offsetof(ImDrawVert, col);
    color_attrib.stride = sizeof(ImDrawVert);
    color_attrib.buffer = vertex_buffer;
    color_attrib.buffer_offset = 0;
    color_attrib.format = VK_FORMAT_R8G8B8A8_UNORM;
    mesh->vertex_attribs[vierkant::Mesh::ATTRIB_COLOR] = color_attrib;

    // vertex attrib -> tex coords
    vierkant::vertex_attrib_t tex_coord_attrib;
    tex_coord_attrib.offset = offsetof(ImDrawVert, uv);
    tex_coord_attrib.stride = sizeof(ImDrawVert);
    tex_coord_attrib.buffer = vertex_buffer;
    tex_coord_attrib.buffer_offset = 0;
    tex_coord_attrib.format = vierkant::format<glm::vec2>();
    mesh->vertex_attribs[vierkant::Mesh::ATTRIB_TEX_COORD] = tex_coord_attrib;

    mesh->vertex_buffer = vertex_buffer;

    // index buffer
    mesh->index_buffer = index_buffer;
    mesh->index_type = index_type<ImDrawIdx>();

    return {mesh, vertex_buffer, index_buffer};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

ImVec4 im_vec_cast(const glm::vec3 &v) { return {v.x, v.y, v.z, 1.f}; }

///////////////////////////////////////////////////////////////////////////////////////////////////

Context::Context(const vierkant::DevicePtr &device, const create_info_t &create_info)
    : m_imgui_context(ImGui::CreateContext()), m_implot_context(ImPlot::CreateContext())
{
    ImGui::SetCurrentContext(m_imgui_context);
    ImPlot::SetCurrentContext(m_implot_context);

    // Setup back-end capabilities flags
    ImGuiIO &io = ImGui::GetIO();

    // option to disable ini-files
    io.IniFilename = create_info.ini_file ? io.IniFilename : nullptr;

    // We can honor GetMouseCursor() values
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;

    // support >64k vertices with 16bit indices
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    // enable window-docking
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    std::unordered_map<int, ImGuiKey> keymap;
    keymap[Key::_LEFT_CONTROL] = ImGuiKey_LeftCtrl;
    keymap[Key::_RIGHT_CONTROL] = ImGuiKey_RightCtrl;
    keymap[Key::_LEFT_ALT] = ImGuiKey_LeftAlt;
    keymap[Key::_RIGHT_ALT] = ImGuiKey_RightAlt;
    keymap[Key::_LEFT_SHIFT] = ImGuiKey_LeftShift;
    keymap[Key::_RIGHT_SHIFT] = ImGuiKey_RightShift;
    keymap[Key::_TAB] = ImGuiKey_Tab;
    keymap[Key::_LEFT] = ImGuiKey_LeftArrow;
    keymap[Key::_RIGHT] = ImGuiKey_RightArrow;
    keymap[Key::_UP] = ImGuiKey_UpArrow;
    keymap[Key::_DOWN] = ImGuiKey_DownArrow;
    keymap[Key::_PAGE_UP] = ImGuiKey_PageUp;
    keymap[Key::_PAGE_DOWN] = ImGuiKey_PageDown;
    keymap[Key::_HOME] = ImGuiKey_Home;
    keymap[Key::_END] = ImGuiKey_End;
    keymap[Key::_INSERT] = ImGuiKey_Insert;
    keymap[Key::_DELETE] = ImGuiKey_Delete;
    keymap[Key::_BACKSPACE] = ImGuiKey_Backspace;
    keymap[Key::_SPACEBAR] = ImGuiKey_Space;
    keymap[Key::_ENTER] = ImGuiKey_Enter;
    keymap[Key::_ESCAPE] = ImGuiKey_Escape;
    keymap[Key::_A] = ImGuiKey_A;
    keymap[Key::_C] = ImGuiKey_C;
    keymap[Key::_V] = ImGuiKey_V;
    keymap[Key::_X] = ImGuiKey_X;
    keymap[Key::_Y] = ImGuiKey_Y;
    keymap[Key::_Z] = ImGuiKey_Z;

    if(!create_info.font_data.empty())
    {
        ImFontConfig font_cfg;
        font_cfg.FontData = const_cast<uint8_t *>(create_info.font_data.data());
        font_cfg.FontDataSize = static_cast<int>(create_info.font_data.size());
        font_cfg.FontDataOwnedByAtlas = false;

        // additionally apply ui-scale to font-size
        font_cfg.SizePixels = create_info.font_size * create_info.ui_scale;
        font_cfg.GlyphRanges = io.Fonts->GetGlyphRangesDefault();
        io.Fonts->AddFont(&font_cfg);
    }

    ImGuiStyle &im_style = ImGui::GetStyle();
    set_style();
    im_style.ScaleAllSizes(create_info.ui_scale);

    auto &mouse_delegate = m_imgui_assets.mouse_delegate;
    mouse_delegate.mouse_press = [ctx = m_imgui_context](const MouseEvent &e) { mouse_press(ctx, e); };
    mouse_delegate.mouse_release = [ctx = m_imgui_context](const MouseEvent &e) { mouse_release(ctx, e); };
    mouse_delegate.mouse_wheel = [ctx = m_imgui_context](const MouseEvent &e) { mouse_wheel(ctx, e); };
    mouse_delegate.mouse_move = [ctx = m_imgui_context](const MouseEvent &e) { mouse_move(ctx, e); };

    auto &key_delegate = m_imgui_assets.key_delegate;
    key_delegate.key_press = [ctx = m_imgui_context, keymap](const KeyEvent &e) { key_press(ctx, e, keymap); };
    key_delegate.key_release = [ctx = m_imgui_context, keymap](const KeyEvent &e) { key_release(ctx, e, keymap); };
    key_delegate.character_input = [ctx = m_imgui_context](uint32_t c) { character_input(ctx, c); };

    create_device_objects(device);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Context::Context(Context &&other) noexcept : Context() { swap(*this, other); }

///////////////////////////////////////////////////////////////////////////////////////////////////

Context::~Context()
{
    if(m_imgui_context) { ImGui::DestroyContext(m_imgui_context); }
    if(m_implot_context) { ImPlot::DestroyContext(m_implot_context); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Context &Context::operator=(Context other)
{
    swap(*this, other);
    return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

ImGuiIO &Context::get_io()
{
    ImGui::SetCurrentContext(m_imgui_context);
    ImPlot::SetCurrentContext(m_implot_context);
    return ImGui::GetIO();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Context::update(double time_delta, const glm::vec2 &size)
{
    ImGuiIO &io = get_io();

    // Setup display size (every frame to accommodate for window resizing)
    io.DisplaySize = {size.x, size.y};
    io.DisplayFramebufferScale = ImVec2(1.f, 1.f);

    // start the frame. will update the io.WantCaptureMouse, io.WantCaptureKeyboard flags
    ImGui::NewFrame();

    // signal begin frame to ImGuizmo
    ImGuizmo::BeginFrame();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    // update time step
    io.DeltaTime = static_cast<float>(time_delta);

    int fb_width = (int) (io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fb_height = (int) (io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if(fb_width == 0 || fb_height == 0) { return; }

    // fire draw delegates
    for(const auto &[name, delegate]: delegates)
    {
        if(delegate.enabled && delegate.fn) { delegate.fn(); }
    }
    // create imgui drawlists
    ImGui::Render();
    ImGui::EndFrame();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Context::draw_gui(vierkant::Rasterizer &renderer)
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    ImGuiIO &io = get_io();

    ImDrawData *draw_data = ImGui::GetDrawData();
    draw_data->ScaleClipRects(io.DisplayFramebufferScale);

    // provide enough frameslots
    m_imgui_assets.frame_assets.resize(renderer.num_concurrent_frames());
    auto &mesh_assets = m_imgui_assets.frame_assets[renderer.current_index()];

    // provide enough mesh_assets (1 vertex/index buffer per window)
    for(auto i = static_cast<int32_t>(mesh_assets.size()); i < draw_data->CmdListsCount; ++i)
    {
        mesh_assets.push_back(create_mesh_assets(renderer.device()));
    }

    vierkant::matrix_struct_t matrices = {};
    matrices.projection = glm::orthoRH(0.f, io.DisplaySize.x, 0.f, io.DisplaySize.y, 0.0f, 1.0f);

    // Draw
    for(int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList *cmd_list = draw_data->CmdLists[n];
        uint32_t base_index = 0;

        // upload data
        mesh_assets[n].vertex_buffer->set_data(cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        mesh_assets[n].index_buffer->set_data(cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));

        for(int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
            if(pcmd->UserCallback) { pcmd->UserCallback(cmd_list, pcmd); }
            else
            {
                auto tex = vierkant::ImagePtr(reinterpret_cast<vierkant::Image *>(pcmd->TextureId),
                                              [](vierkant::Image *) {});

                // create a new drawable
                auto drawable = m_imgui_assets.drawable;
                drawable.mesh = mesh_assets[n].mesh;
                drawable.matrices = matrices;
                drawable.descriptors[vierkant::Rasterizer::BINDING_TEXTURES].images = {tex};
                drawable.vertex_offset = static_cast<int32_t>(pcmd->VtxOffset);
                drawable.base_index = base_index;
                drawable.num_indices = pcmd->ElemCount;

                auto &scissor = drawable.pipeline_format.scissor;
                scissor.offset = {static_cast<int32_t>(pcmd->ClipRect.x), static_cast<int32_t>(pcmd->ClipRect.y)};
                scissor.extent = {static_cast<uint32_t>(pcmd->ClipRect.z - pcmd->ClipRect.x),
                                  static_cast<uint32_t>(pcmd->ClipRect.w - pcmd->ClipRect.y)};

                renderer.stage_drawable(std::move(drawable));
            }
            base_index += pcmd->ElemCount;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const vierkant::mouse_delegate_t &Context::mouse_delegate() const { return m_imgui_assets.mouse_delegate; }

///////////////////////////////////////////////////////////////////////////////////////////////////

const vierkant::key_delegate_t &Context::key_delegate() const { return m_imgui_assets.key_delegate; }

///////////////////////////////////////////////////////////////////////////////////////////////////

Context::CaptureFlags Context::capture_flags() const
{
    ImGuiIO &io = m_imgui_context->IO;
    Context::CaptureFlags flags = 0;
    if(io.WantCaptureMouse) { flags |= Context::WantCaptureMouse; }
    if(io.WantCaptureKeyboard) { flags |= Context::WantCaptureKeyboard; }
    if(io.WantTextInput) { flags |= Context::WantTextInput; }
    if(io.WantSetMousePos) { flags |= Context::WantSetMousePos; }
    if(io.WantSaveIniSettings) { flags |= Context::WantSaveIniSettings; }
    return flags;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Context::create_device_objects(const vierkant::DevicePtr &device)
{
    // font texture
    ImGuiIO &io = ImGui::GetIO();
    unsigned char *pixels;
    int width, height, num_components;
    io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height, &num_components);

    vierkant::Image::Format fmt = {};
    fmt.format = vierkant::format<uint8_t>();
    fmt.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    fmt.component_swizzle = {VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE,
                             VK_COMPONENT_SWIZZLE_R};
    m_imgui_assets.font_texture = vierkant::Image::create(device, pixels, fmt);
    io.Fonts->TexID = reinterpret_cast<ImTextureID>(m_imgui_assets.font_texture.get());

    // create dummy mesh instance
    auto mesh = create_mesh_assets(device).mesh;

    // pipeline format
    vierkant::graphics_pipeline_info_t pipeline_fmt = {};
    pipeline_fmt.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipeline_fmt.shader_stages = vierkant::create_shader_stages(device, vierkant::ShaderType::UNLIT_TEXTURE);
    pipeline_fmt.attribute_descriptions = vierkant::create_attribute_descriptions(mesh->vertex_attribs);
    pipeline_fmt.binding_descriptions = vierkant::create_binding_descriptions(mesh->vertex_attribs);
    pipeline_fmt.depth_write = false;
    pipeline_fmt.depth_test = false;
    pipeline_fmt.blend_state.blendEnable = true;
    pipeline_fmt.cull_mode = VK_CULL_MODE_NONE;
    pipeline_fmt.dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    auto &drawable = m_imgui_assets.drawable;
    drawable.pipeline_format = std::move(pipeline_fmt);
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void swap(Context &lhs, Context &rhs) noexcept
{
    std::swap(lhs.m_imgui_context, rhs.m_imgui_context);
    std::swap(lhs.m_implot_context, rhs.m_implot_context);
    std::swap(lhs.m_imgui_assets, rhs.m_imgui_assets);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void mouse_press(ImGuiContext *ctx, const MouseEvent &e)
{
    ImGuiIO &io = ctx->IO;
    if(e.is_left()) { io.AddMouseButtonEvent(0, true); }
    else if(e.is_middle()) { io.AddMouseButtonEvent(2, true); }
    else if(e.is_right()) { io.AddMouseButtonEvent(1, true); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void mouse_release(ImGuiContext *ctx, const MouseEvent &e)
{
    ImGuiIO &io = ctx->IO;
    if(e.is_left()) { io.AddMouseButtonEvent(0, false); }
    else if(e.is_middle()) { io.AddMouseButtonEvent(2, false); }
    else if(e.is_right()) { io.AddMouseButtonEvent(1, false); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void mouse_wheel(ImGuiContext *ctx, const MouseEvent &e)
{
    ImGuiIO &io = ctx->IO;
    io.AddMouseWheelEvent(static_cast<float>(e.wheel_increment().x), static_cast<float>(e.wheel_increment().y));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void mouse_move(ImGuiContext *ctx, const MouseEvent &e)
{
    ImGuiIO &io = ctx->IO;
    io.AddMousePosEvent(static_cast<float>(e.get_x()), static_cast<float>(e.get_y()));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void key_press(ImGuiContext *ctx, const KeyEvent &e, const std::unordered_map<int, ImGuiKey> &keymap)
{
    ImGuiIO &io = ctx->IO;
    auto it = keymap.find(e.code());
    if(it != keymap.end())
    {
        switch(it->second)
        {
            case ImGuiKey_LeftCtrl:
            case ImGuiKey_RightCtrl: io.AddKeyEvent(ImGuiMod_Ctrl, true); break;
            case ImGuiKey_LeftShift:
            case ImGuiKey_RightShift: io.AddKeyEvent(ImGuiMod_Shift, true); break;
            case ImGuiKey_LeftAlt:
            case ImGuiKey_RightAlt: io.AddKeyEvent(ImGuiMod_Alt, true); break;
            case ImGuiKey_LeftSuper:
            case ImGuiKey_RightSuper: io.AddKeyEvent(ImGuiMod_Super, true); break;
            default: break;
        }
        io.AddKeyEvent(it->second, true);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void key_release(ImGuiContext *ctx, const KeyEvent &e, const std::unordered_map<int, ImGuiKey> &keymap)
{
    ImGuiIO &io = ctx->IO;
    auto it = keymap.find(e.code());
    if(it != keymap.end())
    {
        switch(it->second)
        {
            case ImGuiKey_LeftCtrl:
            case ImGuiKey_RightCtrl: io.AddKeyEvent(ImGuiMod_Ctrl, false); break;
            case ImGuiKey_LeftShift:
            case ImGuiKey_RightShift: io.AddKeyEvent(ImGuiMod_Shift, false); break;
            case ImGuiKey_LeftAlt:
            case ImGuiKey_RightAlt: io.AddKeyEvent(ImGuiMod_Alt, false); break;
            case ImGuiKey_LeftSuper:
            case ImGuiKey_RightSuper: io.AddKeyEvent(ImGuiMod_Super, false); break;
            default: break;
        }
        io.AddKeyEvent(it->second, false);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void character_input(ImGuiContext *ctx, uint32_t c)
{
    ImGuiIO &io = ctx->IO;
    if(c > 0 && c < 0x10000) { io.AddInputCharacter((unsigned short) c); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

}// namespace vierkant::gui
