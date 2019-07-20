#include <crocore/Area.hpp>
#include <crocore/Image.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/Pipeline.hpp>
#include "vierkant/imgui/imgui_integration.h"
#include "imgui_internal.h"

namespace vierkant::gui {

// 1 double per second
using double_sec_t = std::chrono::duration<double, std::chrono::seconds::period>;

static const glm::vec4 COLOR_WHITE(1), COLOR_BLACK(0, 0, 0, 1), COLOR_GRAY(.6, .6, .6, 1.),
        COLOR_RED(1, 0, 0, 1), COLOR_GREEN(0, 1, 0, 1), COLOR_BLUE(0, 0, 1, 1),
        COLOR_YELLOW(1, 1, 0, 1), COLOR_PURPLE(1, 0, 1, 1), COLOR_ORANGE(1, .5, 0, 1),
        COLOR_OLIVE(.5, .5, 0, 1), COLOR_DARK_RED(.6, 0, 0, 1);


void mouse_press(ImGuiContext *ctx, const MouseEvent &e);

void mouse_release(ImGuiContext *ctx, const MouseEvent &e);

void mouse_wheel(ImGuiContext *ctx, const MouseEvent &e);

void mouse_move(ImGuiContext *ctx, const MouseEvent &e);

void key_press(ImGuiContext *ctx, const KeyEvent &e);

void key_release(ImGuiContext *ctx, const KeyEvent &e);

void character_input(ImGuiContext *ctx, uint32_t c);

///////////////////////////////////////////////////////////////////////////////////////////////////

Context::mesh_asset_t Context::create_mesh_assets(const vierkant::DevicePtr &device)
{
    // dynamic vertexbuffer objects
    auto vertex_buffer = vierkant::Buffer::create(device, nullptr, 0,
                                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                  VMA_MEMORY_USAGE_CPU_TO_GPU);

    auto index_buffer = vierkant::Buffer::create(device, nullptr, 0,
                                                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

    auto mesh = vierkant::Mesh::create();
    mesh->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // vertex attrib -> position
    vierkant::Mesh::attrib_t position_attrib;
    position_attrib.location = 0;
    position_attrib.offset = offsetof(ImDrawVert, pos);
    position_attrib.stride = sizeof(ImDrawVert);
    position_attrib.buffer = vertex_buffer;
    position_attrib.buffer_offset = 0;
    position_attrib.format = vierkant::format<glm::vec2>();
    mesh->vertex_attribs.push_back(position_attrib);

    // vertex attrib -> color
    vierkant::Mesh::attrib_t color_attrib;
    color_attrib.location = 1;
    color_attrib.offset = offsetof(ImDrawVert, col);
    color_attrib.stride = sizeof(ImDrawVert);
    color_attrib.buffer = vertex_buffer;
    color_attrib.buffer_offset = 0;
    color_attrib.format = VK_FORMAT_R8G8B8A8_UNORM;
    mesh->vertex_attribs.push_back(color_attrib);

    // vertex attrib -> tex coords
    vierkant::Mesh::attrib_t tex_coord_attrib;
    tex_coord_attrib.location = 2;
    tex_coord_attrib.offset = offsetof(ImDrawVert, uv);
    tex_coord_attrib.stride = sizeof(ImDrawVert);
    tex_coord_attrib.buffer = vertex_buffer;
    tex_coord_attrib.buffer_offset = 0;
    tex_coord_attrib.format = vierkant::format<glm::vec2>();
    mesh->vertex_attribs.push_back(tex_coord_attrib);

    // index buffer
    mesh->index_buffer = index_buffer;
    mesh->index_type = VK_INDEX_TYPE_UINT16;

    return {mesh, vertex_buffer, index_buffer};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const ImVec4 im_vec_cast(const glm::vec3 &the_vec)
{
    auto tmp = glm::vec4(the_vec, 1.f);
    return *reinterpret_cast<const ImVec4 *>(&tmp);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Context::Context(const vierkant::DevicePtr &device, const std::string &font, float font_size) :
        m_imgui_context(ImGui::CreateContext())
{
    ImGui::SetCurrentContext(m_imgui_context);

    // Setup back-end capabilities flags
    ImGuiIO &io = ImGui::GetIO();

    // We can honor GetMouseCursor() values
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;

    // Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array.
    io.KeyMap[ImGuiKey_Tab] = Key::_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = Key::_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = Key::_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = Key::_UP;
    io.KeyMap[ImGuiKey_DownArrow] = Key::_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = Key::_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown] = Key::_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home] = Key::_HOME;
    io.KeyMap[ImGuiKey_End] = Key::_END;
    io.KeyMap[ImGuiKey_Insert] = Key::_INSERT;
    io.KeyMap[ImGuiKey_Delete] = Key::_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = Key::_BACKSPACE;
    io.KeyMap[ImGuiKey_Space] = Key::_SPACE;
    io.KeyMap[ImGuiKey_Enter] = Key::_ENTER;
    io.KeyMap[ImGuiKey_Escape] = Key::_ESCAPE;
    io.KeyMap[ImGuiKey_A] = Key::_A;
    io.KeyMap[ImGuiKey_C] = Key::_C;
    io.KeyMap[ImGuiKey_V] = Key::_V;
    io.KeyMap[ImGuiKey_X] = Key::_X;
    io.KeyMap[ImGuiKey_Y] = Key::_Y;
    io.KeyMap[ImGuiKey_Z] = Key::_Z;

    if(!font.empty())
    {
        // add custom font
        io.Fonts->AddFontFromFileTTF(font.c_str(), font_size, nullptr, io.Fonts->GetGlyphRangesDefault());
    }

    ImGuiStyle &im_style = ImGui::GetStyle();
    im_style.Colors[ImGuiCol_TitleBgActive] = im_vec_cast(COLOR_ORANGE.rgb * 0.5f);
    im_style.Colors[ImGuiCol_FrameBg] = im_vec_cast(COLOR_WHITE.rgb * 0.07f);
    im_style.Colors[ImGuiCol_FrameBgHovered] = im_style.Colors[ImGuiCol_FrameBgActive] =
            im_vec_cast(COLOR_ORANGE.rgb * 0.5f);
    im_style.ScaleAllSizes(1.5f);

    auto &mouse_delegate = m_imgui_assets.mouse_delegate;
    mouse_delegate.mouse_press = [ctx = m_imgui_context](const MouseEvent &e) { mouse_press(ctx, e); };
    mouse_delegate.mouse_release = [ctx = m_imgui_context](const MouseEvent &e) { mouse_release(ctx, e); };
    mouse_delegate.mouse_wheel = [ctx = m_imgui_context](const MouseEvent &e) { mouse_wheel(ctx, e); };
    mouse_delegate.mouse_move = [ctx = m_imgui_context](const MouseEvent &e) { mouse_move(ctx, e); };

    auto &key_delegate = m_imgui_assets.key_delegate;
    key_delegate.key_press = [ctx = m_imgui_context](const KeyEvent &e) { key_press(ctx, e); };
    key_delegate.key_release = [ctx = m_imgui_context](const KeyEvent &e) { key_release(ctx, e); };
    key_delegate.character_input = [ctx = m_imgui_context](uint32_t c) { character_input(ctx, c); };

    create_device_objects(device);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Context::Context(Context &&other) noexcept:
        Context()
{
    swap(*this, other);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Context::~Context()
{
    if(m_imgui_context){ ImGui::DestroyContext(m_imgui_context); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Context &Context::operator=(Context other)
{
    swap(*this, other);
    return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Context::render(vierkant::Renderer &renderer)
{
    ImGui::SetCurrentContext(m_imgui_context);

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    ImGuiIO &io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    io.DisplaySize = {renderer.viewport.width, renderer.viewport.height};
    io.DisplayFramebufferScale = ImVec2(1.f, 1.f);

    // start the frame. will update the io.WantCaptureMouse, io.WantCaptureKeyboard flags
    ImGui::NewFrame();

    // signal begin frame to ImGuizmo
    ImGuizmo::BeginFrame();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    // update time step
    auto now = std::chrono::steady_clock::now();
    io.DeltaTime = double_sec_t(now - m_imgui_assets.time_point).count();
    m_imgui_assets.time_point = now;

    int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if(fb_width == 0 || fb_height == 0){ return; }

    // fire draw delegates
    for(auto &p : delegates){ if(p.second){ p.second(); }}

    // create imgui drawlists
    ImGui::Render();
    ImDrawData *draw_data = ImGui::GetDrawData();
    draw_data->ScaleClipRects(io.DisplayFramebufferScale);

    // provide enough frameslots
    m_imgui_assets.frame_assets.resize(renderer.num_indices());
    auto &mesh_assets = m_imgui_assets.frame_assets[renderer.current_index()];

    // provide enough mesh_assets (1 vertex/index buffer per window)
    for(int32_t i = mesh_assets.size(); i < draw_data->CmdListsCount; ++i)
    {
        mesh_assets.push_back(create_mesh_assets(renderer.device()));
    }

    Renderer::matrix_struct_t matrices = {};
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
            if(pcmd->UserCallback){ pcmd->UserCallback(cmd_list, pcmd); }
            else
            {
                auto tex = vierkant::ImagePtr(static_cast<vierkant::Image *>(pcmd->TextureId),
                                              [](vierkant::Image *) {});

                // create a new drawable
                auto drawable = m_imgui_assets.drawable;
                drawable.mesh = mesh_assets[n].mesh;
                drawable.matrices = matrices;
                drawable.descriptors[1].image_samplers = {tex};
                drawable.base_index = base_index;
                drawable.num_indices = pcmd->ElemCount;

                auto &scissor = drawable.pipeline_format.scissor;
                scissor.offset = {static_cast<int32_t>(pcmd->ClipRect.x),
                                  static_cast<int32_t>(pcmd->ClipRect.y)};
                scissor.extent = {static_cast<uint32_t>(pcmd->ClipRect.z - pcmd->ClipRect.x),
                                  static_cast<uint32_t>(pcmd->ClipRect.w - pcmd->ClipRect.y)};

                renderer.stage_drawable(drawable);
            }
            base_index += pcmd->ElemCount;
        }
    }
    ImGui::EndFrame();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const vierkant::mouse_delegate_t &Context::mouse_delegate() const
{
    return m_imgui_assets.mouse_delegate;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const vierkant::key_delegate_t &Context::key_delegate() const
{
    return m_imgui_assets.key_delegate;
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
    io.Fonts->TexID = m_imgui_assets.font_texture.get();

    // create dummy mesh instance
    auto mesh = create_mesh_assets(device).mesh;

    // pipeline format
    vierkant::Pipeline::Format pipeline_fmt = {};
    pipeline_fmt.shader_stages = vierkant::shader_stages(device, vierkant::ShaderType::UNLIT_TEXTURE);
    pipeline_fmt.attribute_descriptions = vierkant::attribute_descriptions(mesh);
    pipeline_fmt.binding_descriptions = vierkant::binding_descriptions(mesh);
    pipeline_fmt.depth_write = false;
    pipeline_fmt.depth_test = false;
    pipeline_fmt.blending = true;
    pipeline_fmt.cull_mode = VK_CULL_MODE_NONE;
    pipeline_fmt.dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    // descriptors
    vierkant::descriptor_t desc_ubo = {};
    desc_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_ubo.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
    desc_ubo.binding = 0;

    vierkant::descriptor_t desc_texture = {};
    desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_texture.binding = 1;

    auto &drawable = m_imgui_assets.drawable;
    drawable.mesh = mesh;
    drawable.pipeline_format = std::move(pipeline_fmt);
    drawable.descriptors = {desc_ubo, desc_texture};
    drawable.descriptor_set_layout = vierkant::create_descriptor_set_layout(device, drawable.descriptors);
    drawable.pipeline_format.descriptor_set_layouts = {drawable.descriptor_set_layout.get()};
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void swap(Context &lhs, Context &rhs) noexcept
{
    std::swap(lhs.m_imgui_context, rhs.m_imgui_context);
    std::swap(lhs.m_imgui_assets, rhs.m_imgui_assets);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void mouse_press(ImGuiContext *ctx, const MouseEvent &e)
{
    ImGuiIO &io = ctx->IO;
    if(e.is_left()){ io.MouseDown[0] = true; }
    else if(e.is_middle()){ io.MouseDown[1] = true; }
    else if(e.is_right()){ io.MouseDown[2] = true; }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void mouse_release(ImGuiContext *ctx, const MouseEvent &e)
{
    ImGuiIO &io = ctx->IO;
    if(e.is_left()){ io.MouseDown[0] = false; }
    else if(e.is_middle()){ io.MouseDown[1] = false; }
    else if(e.is_right()){ io.MouseDown[2] = false; }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void mouse_wheel(ImGuiContext *ctx, const MouseEvent &e)
{
    ImGuiIO &io = ctx->IO;
    io.MouseWheelH += e.wheel_increment().x;
    io.MouseWheel += e.wheel_increment().y;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void mouse_move(ImGuiContext *ctx, const MouseEvent &e)
{
    ImGuiIO &io = ctx->IO;
    io.MousePos = {static_cast<float>(e.get_x()), static_cast<float>(e.get_y())};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void key_press(ImGuiContext *ctx, const KeyEvent &e)
{
    ImGuiIO &io = ctx->IO;
    io.KeysDown[e.code()] = true;
    io.KeyCtrl = io.KeysDown[Key::_LEFT_CONTROL] || io.KeysDown[Key::_RIGHT_CONTROL];
    io.KeyShift = io.KeysDown[Key::_LEFT_SHIFT] || io.KeysDown[Key::_RIGHT_SHIFT];
    io.KeyAlt = io.KeysDown[Key::_LEFT_ALT] || io.KeysDown[Key::_RIGHT_ALT];
    io.KeySuper = io.KeysDown[Key::_LEFT_SUPER] || io.KeysDown[Key::_RIGHT_SUPER];
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void key_release(ImGuiContext *ctx, const KeyEvent &e)
{
    ImGuiIO &io = ctx->IO;
    io.KeysDown[e.code()] = false;
    io.KeyCtrl = io.KeysDown[Key::_LEFT_CONTROL] || io.KeysDown[Key::_RIGHT_CONTROL];
    io.KeyShift = io.KeysDown[Key::_LEFT_SHIFT] || io.KeysDown[Key::_RIGHT_SHIFT];
    io.KeyAlt = io.KeysDown[Key::_LEFT_ALT] || io.KeysDown[Key::_RIGHT_ALT];
    io.KeySuper = io.KeysDown[Key::_LEFT_SUPER] || io.KeysDown[Key::_RIGHT_SUPER];
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void character_input(ImGuiContext *ctx, uint32_t c)
{
    ImGuiIO &io = ctx->IO;
    if(c > 0 && c < 0x10000){ io.AddInputCharacter((unsigned short)c); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

}//namespace
