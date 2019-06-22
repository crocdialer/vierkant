//#include "gl/Mesh.hpp"
//#include "app/imgui/imgui.h"
//#include "app/imgui/imgui_util.h"
//#include "imgui_integration.h"
//
//namespace kinski {
//namespace gui {
//
//// app instance
//static kinski::App *g_app = nullptr;
//static double g_Time = 0.0f;
//static bool g_mouse_pressed[3] = {false, false, false};
//
//// gl assets
//static kinski::gl::MeshPtr g_mesh;
//static kinski::gl::Texture g_font_texture;
//static kinski::gl::Buffer g_vertex_buffer;
//static kinski::gl::Buffer g_index_buffer;
//
//void render()
//{
//    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
//    ImGuiIO &io = ImGui::GetIO();
//    int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
//    int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
//    if(fb_width == 0 || fb_height == 0){ return; }
//
//    ImGui::Render();
//    ImDrawData *draw_data = ImGui::GetDrawData();
//    draw_data->ScaleClipRects(io.DisplayFramebufferScale);
//
//    gl::ScopedMatrixPush push_modelview(gl::MODEL_VIEW_MATRIX), push_proj(gl::PROJECTION_MATRIX);
//    gl::load_identity(gl::MODEL_VIEW_MATRIX);
//    gl::load_matrix(gl::PROJECTION_MATRIX, glm::ortho(0.f, io.DisplaySize.x, io.DisplaySize.y, 0.f));
//
//    // Draw
//    for(int n = 0; n < draw_data->CmdListsCount; n++)
//    {
//        const ImDrawList *cmd_list = draw_data->CmdLists[n];
//        auto &entry = g_mesh->entries()[0];
//        entry.base_vertex = 0;
//        entry.base_index = 0;
//        entry.num_vertices = cmd_list->VtxBuffer.Size;
//        entry.num_indices = cmd_list->IdxBuffer.Size;
//        entry.primitive_type = GL_TRIANGLES;
//
//        // upload data
//        g_vertex_buffer.set_data(cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
//        g_index_buffer.set_data(cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
//
//        for(int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
//        {
//            const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
//            if(pcmd->UserCallback){ pcmd->UserCallback(cmd_list, pcmd); }
//            else
//            {
//                auto &tex = *reinterpret_cast<kinski::gl::Texture *>(pcmd->TextureId);
//                g_mesh->material()->add_texture(tex);
//
//                crocore::Area_<uint32_t> rect = {static_cast<uint32_t>(pcmd->ClipRect.x),
//                                                 static_cast<uint32_t>(pcmd->ClipRect.y),
//                                                 static_cast<uint32_t>(pcmd->ClipRect.z - pcmd->ClipRect.x),
//                                                 static_cast<uint32_t>(pcmd->ClipRect.w - pcmd->ClipRect.y)};
//
//                g_mesh->material()->set_scissor_rect(rect);
//                entry.num_indices = pcmd->ElemCount;
//                kinski::gl::draw_mesh(g_mesh);
//            }
//            entry.base_index += pcmd->ElemCount;
//        }
//    }
//}
//
//void mouse_press(const MouseEvent &e)
//{
//    if(e.is_left()){ g_mouse_pressed[0] = true; }
//    else if(e.is_middle()){ g_mouse_pressed[1] = true; }
//    else if(e.is_right()){ g_mouse_pressed[2] = true; }
//}
//
//void mouse_wheel(const MouseEvent &e)
//{
//    ImGuiIO &io = ImGui::GetIO();
//    io.MouseWheelH += e.wheel_increment().x;
//    io.MouseWheel += e.wheel_increment().y;
//}
//
//void key_press(const KeyEvent &e)
//{
//    ImGuiIO &io = ImGui::GetIO();
//    io.KeysDown[e.code()] = true;
//    io.KeyCtrl = io.KeysDown[Key::_LEFT_CONTROL] || io.KeysDown[Key::_RIGHT_CONTROL];
//    io.KeyShift = io.KeysDown[Key::_LEFT_SHIFT] || io.KeysDown[Key::_RIGHT_SHIFT];
//    io.KeyAlt = io.KeysDown[Key::_LEFT_ALT] || io.KeysDown[Key::_RIGHT_ALT];
//    io.KeySuper = io.KeysDown[Key::_LEFT_SUPER] || io.KeysDown[Key::_RIGHT_SUPER];
//}
//
//void key_release(const KeyEvent &e)
//{
//    ImGuiIO &io = ImGui::GetIO();
//    io.KeysDown[e.code()] = false;
//    io.KeyCtrl = io.KeysDown[Key::_LEFT_CONTROL] || io.KeysDown[Key::_RIGHT_CONTROL];
//    io.KeyShift = io.KeysDown[Key::_LEFT_SHIFT] || io.KeysDown[Key::_RIGHT_SHIFT];
//    io.KeyAlt = io.KeysDown[Key::_LEFT_ALT] || io.KeysDown[Key::_RIGHT_ALT];
//    io.KeySuper = io.KeysDown[Key::_LEFT_SUPER] || io.KeysDown[Key::_RIGHT_SUPER];
//}
//
//void char_callback(uint32_t c)
//{
//    ImGuiIO &io = ImGui::GetIO();
//    if(c > 0 && c < 0x10000){ io.AddInputCharacter((unsigned short)c); }
//}
//
//bool create_device_objects()
//{
//    // buffer objects
//    g_vertex_buffer = kinski::gl::Buffer(GL_ARRAY_BUFFER, GL_STREAM_DRAW);
//    g_vertex_buffer.set_stride(sizeof(ImDrawVert));
//    g_index_buffer = kinski::gl::Buffer(GL_ELEMENT_ARRAY_BUFFER, GL_STREAM_DRAW);
//
//    // font texture
//    ImGuiIO &io = ImGui::GetIO();
//    unsigned char *pixels;
//    int width, height, num_components;
//    io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height, &num_components);
//
//#if defined(KINSKI_ARM)
//    GLint tex_format = GL_LUMINANCE_ALPHA;
//
//    // create data
//    size_t num_bytes = width * height * 2;
//    auto luminance_alpha_data = std::unique_ptr<uint8_t>(new uint8_t[num_bytes]);
//    uint8_t *src_ptr = static_cast<uint8_t*>(pixels);
//    uint8_t *out_ptr = luminance_alpha_data.get(), *data_end = luminance_alpha_data.get() + num_bytes;
//
//    for (; out_ptr < data_end; out_ptr += 2, ++src_ptr)
//    {
//        out_ptr[0] = 255;
//        out_ptr[1] = *src_ptr;
//    }
//
//    // create a new texture object for our glyphs
//    gl::Texture::Format fmt;
//    fmt.internal_format = tex_format;
//    g_font_texture = gl::Texture(luminance_alpha_data.get(), tex_format, width, height, fmt);
//#else
//    auto font_img = crocore::Image_<uint8_t>::create(pixels, width, height, num_components, true);
//    g_font_texture = kinski::gl::create_texture_from_image(font_img, false, false);
//    g_font_texture.set_flipped(false);
//    g_font_texture.set_swizzle(GL_ONE, GL_ONE, GL_ONE, GL_RED);
//#endif
//
//    io.Fonts->TexID = &g_font_texture;
//
//    // create mesh instance
//    g_mesh = kinski::gl::Mesh::create();
//
//    // add texture
//    auto &mat = g_mesh->material();
//    mat->add_texture(g_font_texture, kinski::gl::Texture::Usage::COLOR);
//    mat->set_depth_test(false);
//    mat->set_depth_write(false);
//    mat->set_blending(true);
//    mat->set_culling(kinski::gl::Material::CULL_NONE);
//
//    // vertex attrib -> position
//    kinski::gl::Mesh::VertexAttrib position_attrib;
//    position_attrib.type = GL_FLOAT;
//    position_attrib.size = 2;
//    position_attrib.name = "a_vertex";
//    position_attrib.buffer = g_vertex_buffer;
//    position_attrib.stride = sizeof(ImDrawVert);
//    position_attrib.offset = offsetof(ImDrawVert, pos);
//    position_attrib.normalize = false;
//
//    // vertex attrib -> color
//    kinski::gl::Mesh::VertexAttrib color_attrib;
//    color_attrib.type = GL_UNSIGNED_BYTE;
//    color_attrib.size = 4;
//    color_attrib.name = "a_color";
//    color_attrib.buffer = g_vertex_buffer;
//    color_attrib.stride = sizeof(ImDrawVert);
//    color_attrib.offset = offsetof(ImDrawVert, col);
//    color_attrib.normalize = true;
//
//    // vertex attrib -> texcoords
//    kinski::gl::Mesh::VertexAttrib tex_coord_attrib;
//    tex_coord_attrib.type = GL_FLOAT;
//    tex_coord_attrib.size = 2;
//    tex_coord_attrib.name = "a_texCoord";
//    tex_coord_attrib.buffer = g_vertex_buffer;
//    tex_coord_attrib.stride = sizeof(ImDrawVert);
//    tex_coord_attrib.offset = offsetof(ImDrawVert, uv);
//    tex_coord_attrib.normalize = false;
//
//    g_mesh->add_vertex_attrib(position_attrib);
//    g_mesh->add_vertex_attrib(color_attrib);
//    g_mesh->add_vertex_attrib(tex_coord_attrib);
//    g_mesh->set_index_buffer(g_index_buffer);
//    return true;
//}
//
//void invalidate_device_objects()
//{
//    g_mesh.reset();
//    g_vertex_buffer.reset();
//    g_index_buffer.reset();
//    g_font_texture.reset();
//}
//
//bool init(kinski::App *the_app)
//{
//    g_app = the_app;
//
//    // Setup back-end capabilities flags
//    ImGuiIO &io = ImGui::GetIO();
//    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;   // We can honor GetMouseCursor() values (optional)
////    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;    // We can honor io.WantSetMousePos requests (optional, rarely used)
//
//    // Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array.
//    io.KeyMap[ImGuiKey_Tab] = Key::_TAB;
//    io.KeyMap[ImGuiKey_LeftArrow] = Key::_LEFT;
//    io.KeyMap[ImGuiKey_RightArrow] = Key::_RIGHT;
//    io.KeyMap[ImGuiKey_UpArrow] = Key::_UP;
//    io.KeyMap[ImGuiKey_DownArrow] = Key::_DOWN;
//    io.KeyMap[ImGuiKey_PageUp] = Key::_PAGE_UP;
//    io.KeyMap[ImGuiKey_PageDown] = Key::_PAGE_DOWN;
//    io.KeyMap[ImGuiKey_Home] = Key::_HOME;
//    io.KeyMap[ImGuiKey_End] = Key::_END;
//    io.KeyMap[ImGuiKey_Insert] = Key::_INSERT;
//    io.KeyMap[ImGuiKey_Delete] = Key::_DELETE;
//    io.KeyMap[ImGuiKey_Backspace] = Key::_BACKSPACE;
//    io.KeyMap[ImGuiKey_Space] = Key::_SPACE;
//    io.KeyMap[ImGuiKey_Enter] = Key::_ENTER;
//    io.KeyMap[ImGuiKey_Escape] = Key::_ESCAPE;
//    io.KeyMap[ImGuiKey_A] = Key::_A;
//    io.KeyMap[ImGuiKey_C] = Key::_C;
//    io.KeyMap[ImGuiKey_V] = Key::_V;
//    io.KeyMap[ImGuiKey_X] = Key::_X;
//    io.KeyMap[ImGuiKey_Y] = Key::_Y;
//    io.KeyMap[ImGuiKey_Z] = Key::_Z;
//
//    ImGuiStyle &im_style = ImGui::GetStyle();
//    im_style.Colors[ImGuiCol_TitleBgActive] = gui::im_vec_cast(gl::COLOR_ORANGE.rgb * 0.5f);
//    im_style.Colors[ImGuiCol_FrameBg] = gui::im_vec_cast(gl::COLOR_WHITE.rgb * 0.07f);
//    im_style.Colors[ImGuiCol_FrameBgHovered] = im_style.Colors[ImGuiCol_FrameBgActive] =
//            gui::im_vec_cast(gl::COLOR_ORANGE.rgb * 0.5f);
//    return true;
//}
//
//void shutdown()
//{
//    // Destroy OpenGL objects
//    invalidate_device_objects();
//}
//
//void new_frame()
//{
//    if(!g_mesh){ create_device_objects(); }
//
//    ImGuiIO &io = ImGui::GetIO();
//
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
//
//    for(int i = 0; i < 3; i++){ g_mouse_pressed[i] = false; }
//
//    // start the frame. will update the io.WantCaptureMouse, io.WantCaptureKeyboard flags
//    ImGui::NewFrame();
//
//    // signal begin frame to ImGuizmo
//    ImGuizmo::BeginFrame();
//    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
//}
//
//void end_frame()
//{
//    ImGui::EndFrame();
//}
//
//}
//}//namespace
