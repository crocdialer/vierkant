#include <crocore/Area.hpp>
#include <crocore/Image.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/Pipeline.hpp>
#include "vierkant/imgui/imgui_integration.h"

namespace vierkant {

static const glm::vec4 COLOR_WHITE(1), COLOR_BLACK(0, 0, 0, 1), COLOR_GRAY(.6, .6, .6, 1.),
        COLOR_RED(1, 0, 0, 1), COLOR_GREEN(0, 1, 0, 1), COLOR_BLUE(0, 0, 1, 1),
        COLOR_YELLOW(1, 1, 0, 1), COLOR_PURPLE(1, 0, 1, 1), COLOR_ORANGE(1, .5, 0, 1),
        COLOR_OLIVE(.5, .5, 0, 1), COLOR_DARK_RED(.6, 0, 0, 1);

//static double g_Time = 0.0f;
static bool g_mouse_pressed[3] = {false, false, false};

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

static imgui_assets_t g_imgui_assets = {};

mesh_asset_t create_window_assets(const vierkant::DevicePtr &device)
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
    vierkant::Mesh::VertexAttrib position_attrib;
    position_attrib.location = 0;
    position_attrib.offset = offsetof(ImDrawVert, pos);
    position_attrib.stride = sizeof(ImDrawVert);
    position_attrib.buffer = vertex_buffer;
    position_attrib.buffer_offset = 0;
    position_attrib.format = vierkant::format<glm::vec2>();
    mesh->vertex_attribs.push_back(position_attrib);

    // vertex attrib -> color
    vierkant::Mesh::VertexAttrib color_attrib;
    color_attrib.location = 1;
    color_attrib.offset = offsetof(ImDrawVert, col);
    color_attrib.stride = sizeof(ImDrawVert);
    color_attrib.buffer = vertex_buffer;
    color_attrib.buffer_offset = 0;
    color_attrib.format = VK_FORMAT_R8G8B8A8_UNORM;
    mesh->vertex_attribs.push_back(color_attrib);

    // vertex attrib -> tex coords
    vierkant::Mesh::VertexAttrib tex_coord_attrib;
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

const ImVec2 &im_vec_cast(const glm::vec2 &the_vec)
{
    return *reinterpret_cast<const ImVec2 *>(&the_vec);
}

const ImVec4 &im_vec_cast(const glm::vec4 &the_vec)
{
    return *reinterpret_cast<const ImVec4 *>(&the_vec);
}

const ImVec4 im_vec_cast(const glm::vec3 &the_vec)
{
    auto tmp = glm::vec4(the_vec, 1.f);
    return *reinterpret_cast<const ImVec4 *>(&tmp);
}

void render(vierkant::Renderer &renderer)
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    ImGuiIO &io = ImGui::GetIO();
    int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if(fb_width == 0 || fb_height == 0){ return; }

    ImGui::Render();
    ImDrawData *draw_data = ImGui::GetDrawData();
    draw_data->ScaleClipRects(io.DisplayFramebufferScale);

    // provide enough frameslots
    g_imgui_assets.frame_assets.resize(renderer.num_indices());
    auto &mesh_assets = g_imgui_assets.frame_assets[renderer.current_index()];

    // provide enough mesh_assets (1 vertex/index buffer per window)
    for(int32_t i = mesh_assets.size(); i < draw_data->CmdListsCount; ++i)
    {
        mesh_assets.push_back(create_window_assets(renderer.device()));
    }

    Renderer::matrix_struct_t matrices = {};
    matrices.projection = glm::orthoRH(0.f, io.DisplaySize.x, io.DisplaySize.y, 0.f, 0.0f, 1.0f);
    matrices.projection[1][1] *= -1;

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
                auto &tex = *reinterpret_cast<vierkant::ImagePtr *>(pcmd->TextureId);

                // create a new drawable
                auto drawable = g_imgui_assets.drawable;
                drawable.mesh = mesh_assets[n].mesh;
                drawable.matrices = matrices;
                drawable.descriptors[1].image_samplers = {tex};
                drawable.base_index = base_index;
                drawable.num_indices = pcmd->ElemCount;

                renderer.scissor.offset = {static_cast<int32_t>(pcmd->ClipRect.x),
                                           static_cast<int32_t>(pcmd->ClipRect.y)};
                renderer.scissor.extent = {static_cast<uint32_t>(pcmd->ClipRect.z - pcmd->ClipRect.x),
                                           static_cast<uint32_t>(pcmd->ClipRect.w - pcmd->ClipRect.y)};

                renderer.stage_drawable(drawable);
            }
            base_index += pcmd->ElemCount;
        }
    }
}

void mouse_press(const MouseEvent &e)
{
    if(e.is_left()){ g_mouse_pressed[0] = true; }
    else if(e.is_middle()){ g_mouse_pressed[1] = true; }
    else if(e.is_right()){ g_mouse_pressed[2] = true; }
}

void mouse_wheel(const MouseEvent &e)
{
    ImGuiIO &io = ImGui::GetIO();
    io.MouseWheelH += e.wheel_increment().x;
    io.MouseWheel += e.wheel_increment().y;
}

void key_press(const KeyEvent &e)
{
    ImGuiIO &io = ImGui::GetIO();
    io.KeysDown[e.code()] = true;
    io.KeyCtrl = io.KeysDown[Key::_LEFT_CONTROL] || io.KeysDown[Key::_RIGHT_CONTROL];
    io.KeyShift = io.KeysDown[Key::_LEFT_SHIFT] || io.KeysDown[Key::_RIGHT_SHIFT];
    io.KeyAlt = io.KeysDown[Key::_LEFT_ALT] || io.KeysDown[Key::_RIGHT_ALT];
    io.KeySuper = io.KeysDown[Key::_LEFT_SUPER] || io.KeysDown[Key::_RIGHT_SUPER];
}

void key_release(const KeyEvent &e)
{
    ImGuiIO &io = ImGui::GetIO();
    io.KeysDown[e.code()] = false;
    io.KeyCtrl = io.KeysDown[Key::_LEFT_CONTROL] || io.KeysDown[Key::_RIGHT_CONTROL];
    io.KeyShift = io.KeysDown[Key::_LEFT_SHIFT] || io.KeysDown[Key::_RIGHT_SHIFT];
    io.KeyAlt = io.KeysDown[Key::_LEFT_ALT] || io.KeysDown[Key::_RIGHT_ALT];
    io.KeySuper = io.KeysDown[Key::_LEFT_SUPER] || io.KeysDown[Key::_RIGHT_SUPER];
}

void char_callback(uint32_t c)
{
    ImGuiIO &io = ImGui::GetIO();
    if(c > 0 && c < 0x10000){ io.AddInputCharacter((unsigned short)c); }
}

bool create_device_objects(vierkant::DevicePtr device)
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
    g_imgui_assets.font_texture = vierkant::Image::create(device, pixels, fmt);
    io.Fonts->TexID = &g_imgui_assets.font_texture;

    // create dummy mesh instance
    auto mesh = create_window_assets(device).mesh;

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

    auto &drawable = g_imgui_assets.drawable;
    drawable.mesh = mesh;
    drawable.pipeline_format = std::move(pipeline_fmt);
    drawable.descriptors = {desc_ubo, desc_texture};
    drawable.descriptor_set_layout = vierkant::create_descriptor_set_layout(device, drawable.descriptors);
    return true;
}

void invalidate_device_objects()
{
    g_imgui_assets = {};
}

bool init(vierkant::WindowPtr w)
{
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

    ImGuiStyle &im_style = ImGui::GetStyle();
    im_style.Colors[ImGuiCol_TitleBgActive] = im_vec_cast(COLOR_ORANGE.rgb * 0.5f);
    im_style.Colors[ImGuiCol_FrameBg] = im_vec_cast(COLOR_WHITE.rgb * 0.07f);
    im_style.Colors[ImGuiCol_FrameBgHovered] = im_style.Colors[ImGuiCol_FrameBgActive] =
            im_vec_cast(COLOR_ORANGE.rgb * 0.5f);
    return true;
}

void shutdown()
{
    // Destroy OpenGL objects
    invalidate_device_objects();
}

void new_frame()
{
    ImGuiIO &io = ImGui::GetIO();

//    // Setup display size (every frame to accommodate for window resizing)
//    io.DisplaySize = kinski::gui::im_vec_cast(kinski::gl::window_dimension());
//    io.DisplayFramebufferScale = ImVec2(1.f, 1.f);
//
//    // Setup time step
//    double current_time = g_app->get_application_time();
//    io.DeltaTime = g_Time > 0.0 ? (float)(current_time - g_Time) : (float)(1.0f / 60.0f);
//    g_Time = current_time;
//
//    // Setup inputs
//    if(g_app->display_gui()){ io.MousePos = kinski::gui::im_vec_cast(g_app->cursor_position()); }
//    else{ io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX); }
//
//    // If a mouse press event came, always pass it as "mouse held this frame"
//    // so we don't miss click-release events that are shorter than 1 frame.
//    auto mouse_state = g_app->mouse_state();
//    io.MouseDown[0] = g_mouse_pressed[0] || mouse_state.is_left_down();
//    io.MouseDown[1] = g_mouse_pressed[1] || mouse_state.is_middle_down();
//    io.MouseDown[2] = g_mouse_pressed[2] || mouse_state.is_right_down();

    for(bool &i : g_mouse_pressed){ i = false; }

    // start the frame. will update the io.WantCaptureMouse, io.WantCaptureKeyboard flags
    ImGui::NewFrame();

    // signal begin frame to ImGuizmo
    ImGuizmo::BeginFrame();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
}

void end_frame()
{
    ImGui::EndFrame();
}

}//namespace
