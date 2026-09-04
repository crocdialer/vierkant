//
// Created by crocdialer on 2/11/22.
//

#include "vierkant_ed.hpp"

#include "ImGuiFileDialog.h"
#include "vierkant/Visitor.hpp"

#include <crocore/filesystem.hpp>
#include <glm/gtc/random.hpp>
#include <vierkant/imgui/imgui_util.h>

ImGuiFileDialog g_file_dialog;
constexpr char g_imgui_file_dialog_load_key[] = "imgui_file_dialog_load_key";
constexpr char g_imgui_file_dialog_import_key[] = "imgui_file_dialog_import_key";
constexpr char g_imgui_file_dialog_import_as_mesh_lib_key[] = "g_imgui_file_dialog_import_as_mesh_lib_key";
constexpr char g_imgui_file_dialog_save_key[] = "imgui_file_dialog_save_key";

bool DEMO_GUI = false;

struct ui_state_t
{
    glm::ivec2 last_click;
};

void VierkantEd::toggle_ortho_camera()
{
    auto *cam_cmp = m_editor_camera->get_component_ptr<vierkant::camera_component_t>();
    assert(cam_cmp);

    const bool ortho = cam_cmp->projection == vierkant::camera_component_t::ORTHO;
    cam_cmp->projection = ortho ? vierkant::camera_component_t::PERSPECTIVE : vierkant::camera_component_t::ORTHO;

    if(!ortho)
    {
        cam_cmp->ortho.near_ = 0.f;
        cam_cmp->ortho.far_ = 10000.f;
    }

    // the extents are derived from the camera-control, see transform_cb
    m_camera_control.current->transform_cb(m_camera_control.current->transform());
}

void VierkantEd::create_ui()
{
    m_ui_state = {new ui_state_t, std::default_delete<ui_state_t>()};

    auto center_selected_objects = [this] {
        if(!editor_camera_active()) { return; }
        vierkant::AABB aabb;
        for(const auto &obj: m_selected_objects)
        {
            if(auto obj_aabb = obj->aabb(); obj_aabb.valid()) { aabb += obj_aabb.transform(obj->global_transform()); }
            else
            {
                auto p = obj->global_transform().translation;
                aabb += vierkant::AABB(p, p);
            }
        }
        if(!aabb.valid()) { return; }
        m_camera_control.orbit->look_at = aabb.center();
        if(m_camera_control.orbit->transform_cb)
        {
            m_camera_control.orbit->transform_cb(m_camera_control.orbit->transform());
        }
    };

    // create a KeyDelegate
    vierkant::key_delegate_t key_delegate = {};
    key_delegate.key_press = [this, center_selected_objects](const vierkant::KeyEvent &e) {
        if(!m_settings.draw_ui || !(m_gui_context.capture_flags() & vierkant::gui::Context::WantCaptureKeyboard))
        {
            if(e.is_control_down())
            {
                switch(e.code())
                {
                    // save settings and scene
                    case vierkant::Key::_S:
                        save_settings(m_settings);
                        save_scene();
                        break;

                    // copy
                    case vierkant::Key::_C: m_copy_objects = m_selected_objects; break;

                    // cut
                    case vierkant::Key::_X:
                        m_copy_objects = m_selected_objects;
                        for(const auto &obj: m_selected_objects) { m_scene->remove_object(obj); }
                        break;

                    // paste
                    case vierkant::Key::_V:
                    {
                        auto copy_dst = m_selected_objects.empty() ? m_scene->root() : *m_selected_objects.begin();
                        auto clones = clone_objects(m_copy_objects);
                        for(const auto &cloned_obj: clones) { copy_dst->add_child(cloned_obj); }
                        break;
                    }

                    // group
                    case vierkant::Key::_G:
                    {
                        auto group = m_object_store->create_object();
                        group->name = "group";
                        m_scene->add_object(group);
                        for(const auto &sel_obj: m_selected_objects) { group->add_child(sel_obj); }
                        break;
                    }

                    case vierkant::Key::_A:
                    {
                        // select all
                        auto obj_view = m_scene->registry()->view<vierkant::Object3D *, vierkant::mesh_component_t>();
                        for(const auto &[entity, obj, mesh_cmp]: obj_view.each())
                        {
                            m_selected_objects.insert(obj->shared_from_this());
                        }
                        break;
                    }
                    default: break;
                }
                return;
            }

            switch(e.code())
            {
                case vierkant::Key::_Q: m_settings.current_guizmo = vierkant::gui::GuizmoType::INACTIVE; break;
                case vierkant::Key::_W: m_settings.current_guizmo = vierkant::gui::GuizmoType::TRANSLATE; break;
                case vierkant::Key::_E: m_settings.current_guizmo = vierkant::gui::GuizmoType::ROTATE; break;
                case vierkant::Key::_R: m_settings.current_guizmo = vierkant::gui::GuizmoType::SCALE; break;

                case vierkant::Key::_ESCAPE: running = false; break;

                case vierkant::Key::_TAB: m_settings.draw_ui = !m_settings.draw_ui; break;

                case vierkant::Key::_F:
                {
                    size_t monitor_index = m_window->monitor_index();
                    m_window->set_fullscreen(!m_window->fullscreen(), monitor_index);
                }
                break;
                case vierkant::Key::_H:
                {
                    m_window->set_cursor_visible(!m_window->cursor_visible());
                }
                break;
                case vierkant::Key::_C:
                {
                    if(m_camera_control.current == m_camera_control.orbit)
                    {
                        m_camera_control.current = m_camera_control.fly;
                    }
                    else
                    {
                        m_camera_control.current = m_camera_control.orbit;
                    }
                    m_editor_camera->set_transform(m_camera_control.current->transform());
                    if(m_path_tracer) { m_path_tracer->reset_accumulator(); }
                    break;
                }

                case vierkant::Key::_G: m_settings.draw_grid = !m_settings.draw_grid; break;

                case vierkant::Key::_P:
                    if(m_scene_renderer == m_pbr_renderer)
                    {
                        m_scene_renderer = m_path_tracer ? m_path_tracer : m_scene_renderer;
                    }
                    else
                    {
                        m_scene_renderer = m_pbr_renderer;
                    }
                    break;

                case vierkant::Key::_B: m_settings.draw_aabbs = !m_settings.draw_aabbs; break;

                case vierkant::Key::_N: m_settings.draw_node_hierarchy = !m_settings.draw_node_hierarchy; break;

                case vierkant::Key::_M:
                    if(m_pbr_renderer->settings.debug_draw_flags == vierkant::Rasterizer::DRAW_ID)
                    {
                        m_pbr_renderer->settings.debug_draw_flags = vierkant::Rasterizer::LOD;
                    }
                    else
                    {
                        m_pbr_renderer->settings.debug_draw_flags =
                                m_pbr_renderer->settings.debug_draw_flags ? 0 : vierkant::Rasterizer::DRAW_ID;
                    }
                    break;

                case vierkant::Key::_O:
                    if(editor_camera_active()) { toggle_ortho_camera(); }
                    break;

                case vierkant::Key::_PERIOD:
                {
                    center_selected_objects();
                    break;
                }

                case vierkant::Key::_DELETE:
                case vierkant::Key::_BACKSPACE:
                    for(const auto &obj: m_selected_objects)
                    {
                        // deleting the camera we look through would leave it detached from the graph
                        if(obj == m_render_camera) { m_render_camera = m_editor_camera; }
                        m_scene->remove_object(obj);
                    }
                    m_selected_objects.clear();
                    break;
                default: break;
            }
        }
    };
    m_window->key_delegates[name()] = key_delegate;

    vierkant::joystick_delegate_t joystick_delegate = {};

    // the gamepad belongs to the character while it is driven, these bindings would fight it
    joystick_delegate.enabled = [this]() { return !m_settings.character_input; };
    joystick_delegate.joystick_cb = [this, center_selected_objects](const auto &joysticks) {
        if(!joysticks.empty())
        {
            auto &js = joysticks.front();
            for(auto &[input, event]: js.input_events())
            {
                spdlog::trace("{}: {} {}", js.name(), vierkant::to_string(input),
                              (event == vierkant::Joystick::Event::BUTTON_PRESS ? " pressed" : " released"));

                if(event == vierkant::Joystick::Event::BUTTON_PRESS)
                {
                    switch(input)
                    {
                        case vierkant::Joystick::Input::BUTTON_MENU: m_settings.draw_ui = !m_settings.draw_ui; break;

                        case vierkant::Joystick::Input::BUTTON_X: m_settings.draw_grid = !m_settings.draw_grid; break;

                        case vierkant::Joystick::Input::BUTTON_Y:
                            if(m_scene_renderer == m_pbr_renderer)
                            {
                                m_scene_renderer = m_path_tracer ? m_path_tracer : m_scene_renderer;
                            }
                            else
                            {
                                m_scene_renderer = m_pbr_renderer;
                            }
                            break;

                        case vierkant::Joystick::Input::BUTTON_A:
                            if(m_pbr_renderer->settings.debug_draw_flags == vierkant::Rasterizer::DRAW_ID)
                            {
                                m_pbr_renderer->settings.debug_draw_flags = vierkant::Rasterizer::LOD;
                            }
                            else
                            {
                                m_pbr_renderer->settings.debug_draw_flags =
                                        m_pbr_renderer->settings.debug_draw_flags ? 0 : vierkant::Rasterizer::DRAW_ID;
                            }

                            break;

                        case vierkant::Joystick::Input::BUTTON_B:
                            m_pbr_renderer->settings.use_meshlet_pipeline =
                                    !m_pbr_renderer->settings.use_meshlet_pipeline;
                            break;

                        case vierkant::Joystick::Input::DPAD_RIGHT: toggle_ortho_camera(); break;

                        case vierkant::Joystick::Input::BUTTON_STICK_LEFT: center_selected_objects(); break;

                        case vierkant::Joystick::Input::BUTTON_BACK:
                            if(m_camera_control.current == m_camera_control.orbit)
                            {
                                m_camera_control.current = m_camera_control.fly;
                            }
                            else
                            {
                                m_camera_control.current = m_camera_control.orbit;
                            }
                            m_editor_camera->set_transform(m_camera_control.current->transform());
                            if(m_path_tracer) { m_path_tracer->reset_accumulator(); }
                            break;

                        default: break;
                    }
                }
            }
        }
        if(vierkant::joystick_active(joysticks)) { m_window->set_cursor_visible(false); }
    };
    m_window->joystick_delegates[name()] = joystick_delegate;

    // try to fetch a font-file via http
    //    auto http_response = netzer::http::get(g_font_url);
    //    if(http_response.status_code != 200) { spdlog::warn("failed fetching a font from: {}", g_font_url); }

    // create a gui and add a draw-delegate
    vierkant::gui::Context::create_info_t gui_create_info = {};
    gui_create_info.ini_file = true;
    gui_create_info.ui_scale = m_settings.ui_scale;
    if(!m_settings.font_url.empty())
    {
        try
        {
            gui_create_info.font_data = crocore::filesystem::read_binary_file(m_settings.font_url);
        } catch(std::exception &e) { spdlog::warn(e.what()); }
    }
    gui_create_info.font_size = m_settings.ui_font_scale;
    m_gui_context = vierkant::gui::Context(m_device, gui_create_info);

    float bg_alpha = .3f, bg_alpha_active = .9f;
    ImVec4 *colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, bg_alpha);
    colors[ImGuiCol_TitleBg] = ImVec4(0, 0, 0, bg_alpha);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0, 0, 0, bg_alpha_active);

    m_gui_context.delegates["application"].fn = [this] {
        int corner = 0;

        ImVec2 window_pos(0, 0);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);

        ImGui::Begin("about: blank", nullptr,
                     (corner != -1 ? ImGuiWindowFlags_NoMove : 0) | ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus);

        if(ImGui::BeginMenuBar())
        {
            if(ImGui::BeginMenu(name().c_str()))
            {
                ImGui::Separator();
                ImGui::Spacing();

                if(ImGui::MenuItem("save"))
                {
                    save_settings(m_settings);
                    save_scene();
                }

                if(ImGui::MenuItem("save as ..."))
                {
                    IGFD::FileDialogConfig config;
                    config.path = ".";
                    if(!m_settings.recent_files.empty())
                    {
                        config.path = crocore::filesystem::get_directory_part(*m_settings.recent_files.rbegin());
                    }
                    config.flags = ImGuiFileDialogFlags_DisableCreateDirectoryButton;
                    constexpr char filter_str[] = "vierkant-scene (*.json){.json}";
                    g_file_dialog.OpenDialog(g_imgui_file_dialog_save_key, "save scene ...", filter_str, config);
                }

                ImGui::Separator();
                ImGui::Spacing();

                //! file-load/import filter
                constexpr char filter_str[] =
                        "supported (*.gltf *.glb *.obj *.hdr *.jpg *.png *.json){.gltf, .glb, .obj, .hdr, "
                        ".jpg, .png, .json},all {.*}";
                auto get_file_dialog_config = [this] {
                    IGFD::FileDialogConfig config;
                    config.path = ".";
                    if(!m_settings.recent_files.empty())
                    {
                        config.path = crocore::filesystem::get_directory_part(*m_settings.recent_files.rbegin());
                    }
                    config.flags = ImGuiFileDialogFlags_DisableCreateDirectoryButton;
                    return config;
                };
                if(ImGui::MenuItem("load ..."))
                {
                    g_file_dialog.OpenDialog(g_imgui_file_dialog_load_key, "load model/image/scene ...", filter_str,
                                             get_file_dialog_config());
                }

                if(ImGui::MenuItem("import ..."))
                {
                    g_file_dialog.OpenDialog(g_imgui_file_dialog_import_key, "import model/image/scene ...", filter_str,
                                             get_file_dialog_config());
                }

                if(ImGui::MenuItem("import as mesh-library ..."))
                {
                    g_file_dialog.OpenDialog(g_imgui_file_dialog_import_as_mesh_lib_key,
                                             "import model as mesh-library ...", filter_str, get_file_dialog_config());
                }

                if(ImGui::MenuItem("reload"))
                {
                    spdlog::warn("menu: reload");
                    if(auto settings = load_settings()) { m_settings = std::move(*settings); }
                    create_camera_controls();
                    if(m_settings.path_tracing) { m_scene_renderer = m_path_tracer; }
                    else
                    {
                        m_scene_renderer = m_pbr_renderer;
                    }
                }
                ImGui::Separator();
                ImGui::Spacing();

                if(ImGui::BeginMenu("recent files"))
                {
                    for(const auto &f: m_settings.recent_files)
                    {
                        auto file_name = crocore::filesystem::get_filename_part(f);
                        ImGui::PushID(f.c_str());
                        if(ImGui::MenuItem(file_name.c_str()))
                        {
                            ImGui::PopID();
                            spdlog::debug("menu: open recent file -> {}", f);
                            load_file(f, false);
                            break;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                ImGui::Spacing();

                if(ImGui::BeginMenu("settings"))
                {
                    const char *log_items[] = {"Trace", "Debug", "Info", "Warn", "Error", "Critical", "Off"};
                    int log_level = static_cast<int>(spdlog::get_level());

                    if(ImGui::Combo("log level", &log_level, log_items, IM_ARRAYSIZE(log_items)))
                    {
                        spdlog::set_level(static_cast<spdlog::level::level_enum>(log_level));
                    }

                    ImGui::Checkbox("draw grid", &m_settings.draw_grid);
                    ImGui::Checkbox("draw aabbs", &m_settings.draw_aabbs);
                    ImGui::Checkbox("draw view-controls", &m_settings.ui_draw_view_controls);
                    ImGui::Checkbox("physics debug-draw", &m_settings.draw_physics);
                    ImGui::Checkbox("draw node hierarchy", &m_settings.draw_node_hierarchy);
                    ImGui::Checkbox("texture compression", &m_settings.texture_compression);
                    ImGui::Checkbox("opacity micromaps", &m_settings.opacity_micromaps);
                    ImGui::Checkbox("remap indices", &m_settings.mesh_buffer_params.remap_indices);
                    ImGui::Checkbox("optimize vertex cache", &m_settings.mesh_buffer_params.optimize_vertex_cache);
                    ImGui::Checkbox("generate mesh-LODs", &m_settings.mesh_buffer_params.generate_lods);
                    ImGui::Checkbox("generate meshlets", &m_settings.mesh_buffer_params.generate_meshlets);
                    ImGui::Checkbox("cache mesh-bundles", &m_settings.cache_mesh_bundles);
                    ImGui::Checkbox("zip-compress bundles", &m_settings.cache_zip_archive);

                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::Text("transform-space");
                    ImGui::SameLine();
                    if(ImGui::RadioButton("world", m_settings.guizmo_space == vierkant::gui::GuizmoSpace::WORLD))
                    {
                        m_settings.guizmo_space = vierkant::gui::GuizmoSpace::WORLD;
                    }
                    ImGui::SameLine();

                    if(ImGui::RadioButton("local", m_settings.guizmo_space == vierkant::gui::GuizmoSpace::LOCAL))
                    {
                        m_settings.guizmo_space = vierkant::gui::GuizmoSpace::LOCAL;
                    }

                    ImGui::Separator();
                    ImGui::Spacing();

                    if(ImGui::RadioButton("none", m_settings.object_overlay_mode == vierkant::ObjectOverlayMode::None))
                    {
                        m_settings.object_overlay_mode = vierkant::ObjectOverlayMode::None;
                    }
                    ImGui::SameLine();

                    if(ImGui::RadioButton("mask", m_settings.object_overlay_mode == vierkant::ObjectOverlayMode::Mask))
                    {
                        m_settings.object_overlay_mode = vierkant::ObjectOverlayMode::Mask;
                    }
                    ImGui::SameLine();

                    if(ImGui::RadioButton("silhoutte",
                                          m_settings.object_overlay_mode == vierkant::ObjectOverlayMode::Silhouette))
                    {
                        m_settings.object_overlay_mode = vierkant::ObjectOverlayMode::Silhouette;
                    }

                    ImGui::Separator();
                    ImGui::Spacing();

                    // the camera-controls drive the editor-camera, which is not what we render
                    // while a scene-camera is picked. say so instead of going quietly dead.
                    if(!editor_camera_active())
                    {
                        ImGui::TextUnformatted("camera-controls inactive: rendering through");
                        ImGui::SameLine();
                        ImGui::TextUnformatted(m_render_camera->name.c_str());
                        ImGui::Spacing();
                    }
                    ImGui::BeginDisabled(!editor_camera_active());

                    // camera control select
                    bool refresh = false;

                    if(ImGui::RadioButton("orbit", m_camera_control.current == m_camera_control.orbit))
                    {
                        m_camera_control.current = m_camera_control.orbit;
                        refresh = true;
                    }
                    ImGui::SameLine();

                    if(ImGui::RadioButton("fly", m_camera_control.current == m_camera_control.fly))
                    {
                        m_camera_control.current = m_camera_control.fly;
                        refresh = true;
                    }
                    ImGui::SameLine();

                    const auto *cam_cmp = m_editor_camera->get_component_ptr<vierkant::camera_component_t>();
                    assert(cam_cmp);
                    bool ortho = cam_cmp->projection == vierkant::camera_component_t::ORTHO;

                    if(ImGui::Checkbox("ortho", &ortho)) { toggle_ortho_camera(); }

                    ImGui::SliderFloat("move speed", &m_camera_control.fly->move_speed, 0.1f, 100.f);
                    ImGui::EndDisabled();

                    // routes gamepad-input to the first character in the scene, leaves the camera alone
                    ImGui::Checkbox("character input", &m_settings.character_input);

                    if(m_settings.character_input)
                    {
                        ImGui::Checkbox("body follows view-yaw", &m_settings.body_use_view_yaw);
                    }

                    if(refresh)
                    {
                        m_editor_camera->set_transform(m_camera_control.current->transform());
                        if(m_path_tracer) { m_path_tracer->reset_accumulator(); }
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                ImGui::Spacing();

                if(ImGui::BeginMenu("add"))
                {
                    if(ImGui::Button("physics boxes (25)"))
                    {
                        auto box_mesh = m_scene->asset_provider()->primitive_mesh(vierkant::primitive_type::BOX);
                        auto cubes = m_scene->any_object_by_name("cubes");
                        if(!cubes)
                        {
                            auto new_group = m_object_store->create_object();
                            new_group->name = "cubes";
                            m_scene->add_object(new_group);
                            cubes = new_group.get();
                        }

                        for(uint32_t i = 0; box_mesh && i < 25; ++i)
                        {
                            auto new_obj = m_scene->create_mesh_object({box_mesh});
                            new_obj->name = spdlog::fmt_lib::format("cube_{}", new_obj->id() % 1000);
                            new_obj->set_transform({.translation = glm::vec3(0.f, 10.f, 0.f) + glm::ballRand(1.f)});
                            vierkant::object_component auto &cmp =
                                    new_obj->add_component<vierkant::physics_component_t>();
                            vierkant::collision::box_t box = {box_mesh->entries.front().bounding_box.half_extents()};
                            cmp.shape = box;
                            cmp.mass = 1.f;

                            // add to group
                            cubes->add_child(new_obj);
                            cubes->name = spdlog::fmt_lib::format("cubes ({})", cubes->children.size());
                        }
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                ImGui::Spacing();
                if(ImGui::MenuItem("quit")) { running = false; }
                ImGui::EndMenu();
            }

            if(ImGui::BeginMenu("display"))
            {
                vierkant::gui::draw_application_ui(std::static_pointer_cast<Application>(shared_from_this()), m_window);
                ImGui::EndMenu();
            }

            if(ImGui::BeginMenu("renderer"))
            {
                const bool is_path_tracer = m_scene_renderer == m_path_tracer;

                if(ImGui::RadioButton("pbr-deferred", !is_path_tracer)) { m_scene_renderer = m_pbr_renderer; }
                ImGui::SameLine();
                if(ImGui::RadioButton("pathtracer", is_path_tracer))
                {
                    m_scene_renderer = m_path_tracer ? m_path_tracer : m_scene_renderer;
                }
                ImGui::Spacing();
                vierkant::gui::draw_scene_renderer_settings_ui(m_scene_renderer);
                ImGui::EndMenu();
            }

            if(ImGui::BeginMenu("stats"))
            {
                const auto loop_time = current_loop_time();
                ImGui::Text("fps: %.1f (%.1f ms)", 1.f / loop_time, loop_time * 1000.f);
                ImGui::Spacing();
                ImGui::Text("time: %s | frame: %d",
                            crocore::secs_to_time_str(static_cast<float>(application_time())).c_str(),
                            static_cast<uint32_t>(m_window->num_frames()));
                ImGui::Spacing();

                if(ImGui::TreeNode("memory-budget"))
                {
                    constexpr double mega_bytes = 1 << 20;
                    const auto budgets = m_device->memory_budgets();

                    for(uint32_t i = 0; i < budgets.size(); ++i)
                    {
                        const auto &budget = budgets[i];
                        if(!budget.block_bytes) { continue; }
                        ImGui::Text("heap %u: %.0f MB used | %.0f MB reserved | %.0f MB budget", i,
                                    static_cast<double>(budget.allocation_bytes) / mega_bytes,
                                    static_cast<double>(budget.block_bytes) / mega_bytes,
                                    static_cast<double>(budget.budget) / mega_bytes);
                    }
                    ImGui::TreePop();
                }
                ImGui::Spacing();

                vierkant::gui::draw_scene_renderer_statistics_ui(m_scene_renderer);
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }
        ImGui::End();
    };

    // renderer window
    m_gui_context.delegates["file_dialog"].fn = [this] {
        // display
        ImGuiIO &io = ImGui::GetIO();
        ImGuiWindowFlags flags = 0;
        auto min_size = io.DisplaySize * 0.5f;

        auto p = std::filesystem::path(g_file_dialog.GetCurrentPath()) /
                 std::filesystem::path(g_file_dialog.GetCurrentFileName());

        // load dialog
        if(g_file_dialog.Display(g_imgui_file_dialog_load_key, flags, min_size))
        {
            if(g_file_dialog.IsOk())
            {
                // clear scene, load file as one object
                load_file(p.string(), true);
            }
            g_file_dialog.Close();
        }

        // import dialog
        else if(g_file_dialog.Display(g_imgui_file_dialog_import_key, flags, min_size))
        {
            if(g_file_dialog.IsOk())
            {
                // import file into scene, as one object
                load_file(p.string(), false);
            }
            g_file_dialog.Close();
        }

        // import as mesh-library dialog
        else if(g_file_dialog.Display(g_imgui_file_dialog_import_as_mesh_lib_key, flags, min_size))
        {
            if(g_file_dialog.IsOk())
            {
                // import file into scene, as a library of objects
                add_to_recent_files(p);
                load_model_params_t load_params = {p};
                load_params.clear_scene = false;
                load_params.mesh_library = true;
                load_params.normalize_size = false;
                load_model(load_params);
            }
            g_file_dialog.Close();
        }

        // save dialog
        else if(g_file_dialog.Display(g_imgui_file_dialog_save_key, flags, min_size))
        {
            if(g_file_dialog.IsOk())
            {
                // save scene
                save_scene(p.string());
            }
            g_file_dialog.Close();
        };
    };

    // log window
    m_gui_context.delegates["logger"].fn = [&log_queue = m_log_queue, &mutex = m_log_queue_mutex] {
        std::shared_lock lock(mutex);
        vierkant::gui::draw_logger_ui(log_queue);
    };

    // scenegraph window
    m_gui_context.delegates["scenegraph"].fn = [this] {
        constexpr int corner = 1;
        const float DISTANCE = 10.0f;
        ImGuiIO &io = ImGui::GetIO();
        ImVec2 window_pos = ImVec2((corner & 1) ? io.DisplaySize.x - DISTANCE : DISTANCE,
                                   (corner & 2) ? io.DisplaySize.y - DISTANCE : DISTANCE);
        ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
        ImGui::SetNextWindowSize(ImVec2(440, 650), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);

        {
            ImGui::Begin("scene");
            if(ImGui::InputFloat("playback speed", &m_settings.playback_speed, 0.05f, .25f))
            {
                m_settings.playback_speed = std::max(0.f, m_settings.playback_speed);
            }
            ImGui::BulletText("%s", std::format("frame: {}", m_scene->current_frame()).c_str());
            ImGui::Checkbox("playing", &m_settings.animation_playback);
            ImGui::SameLine();
            ImGui::Checkbox("simulate", &m_settings.physics_playback);
            ImGui::Spacing();

            vierkant::gui::draw_scene_ui(m_scene, m_render_camera, &m_selected_objects);
            ImGui::End();
        }
    };

    // object/view manipulation
    m_gui_context.delegates["guizmo"].fn = [this] {
        if(!m_selected_objects.empty())
        {
            vierkant::gui::draw_transform_guizmo(m_selected_objects, m_render_camera, m_settings.current_guizmo,
                                                 m_settings.guizmo_space);
        }

        if(m_settings.ui_draw_view_controls)
        {
            auto view = vierkant::mat4_cast(vierkant::camera::view_transform(m_render_camera.get()));
            const glm::vec2 sz = {150, 150};
            glm::vec2 pos = {(static_cast<float>(m_window->size().x) - sz.x) / 2.f, 0.f};
            if(ImGuizmo::ViewManipulate(glm::value_ptr(view), 1.f, {pos.x, pos.y}, {sz.x, sz.y}, 0x00000000))
            {
                auto transform = vierkant::inverse(vierkant::transform_cast(view));
                glm::vec3 pitch_yaw = glm::eulerAngles(transform.rotation);

                // account for roll and negative angles
                auto sng_x = static_cast<float>(crocore::sgn(-pitch_yaw.x));
                float sng_y = 1.f - 2.f * std::abs(pitch_yaw.z) * glm::one_over_pi<float>();
                pitch_yaw.x += std::abs(pitch_yaw.z) * sng_x;
                pitch_yaw.y = std::fmod(glm::two_pi<float>() + std::abs(pitch_yaw.z) + pitch_yaw.y * sng_y,
                                        glm::two_pi<float>());

                if(m_camera_control.current == m_camera_control.orbit)
                {
                    m_camera_control.orbit->spherical_coords = pitch_yaw;
                }
                else
                {
                    m_camera_control.fly->spherical_coords = pitch_yaw;
                }

                if(m_camera_control.current->transform_cb)
                {
                    m_camera_control.current->transform_cb(m_camera_control.current->transform());
                }
            }
        }
    };

    // imgui demo window
    m_gui_context.delegates["demo"].fn = [] {
        if(DEMO_GUI) { ImGui::ShowDemoWindow(&DEMO_GUI); }
        if(DEMO_GUI) { ImPlot::ShowDemoWindow(&DEMO_GUI); }
    };

    // attach gui input-delegates to window
    m_window->key_delegates["gui"] = m_gui_context.key_delegate();
    m_window->mouse_delegates["gui"] = m_gui_context.mouse_delegate();

    create_camera_controls();

    vierkant::mouse_delegate_t simple_mouse = {};
    simple_mouse.mouse_press = [this](const vierkant::MouseEvent &e) {
        if(!m_settings.draw_ui || !(m_gui_context.capture_flags() & vierkant::gui::Context::WantCaptureMouse))
        {
            if(e.is_right())
            {
                m_selected_objects.clear();
                m_selected_indices.clear();
            }
            else if(e.is_left())
            {
                // only store last click
                m_ui_state->last_click = e.position();
            }
        }
    };
    simple_mouse.mouse_release = [this](const vierkant::MouseEvent &e) {
        if(!m_settings.draw_ui || !(m_gui_context.capture_flags() & vierkant::gui::Context::WantCaptureMouse))
        {
            if(e.is_left())
            {
                // clear selection area
                m_selection_area.reset();

                auto current_click = glm::clamp(e.position(), glm::ivec2(0), m_window->size() - 1);
                glm::vec2 tl = {std::min<int>(current_click.x, m_ui_state->last_click.x),
                                std::min<int>(current_click.y, m_ui_state->last_click.y)};
                glm::vec2 size = glm::abs(current_click - m_ui_state->last_click);
                auto picked_ids =
                        m_scene_renderer->pick(tl / glm::vec2(m_window->size()), size / glm::vec2(m_window->size()));

                std::unordered_set<vierkant::Object3D *> picked_objects;

                for(uint32_t i = 0; i < picked_ids.size(); ++i)
                {
                    auto draw_idx = picked_ids[i];
                    vierkant::Object3D *picked_object = nullptr;
                    if(const auto &overlay_asset = m_overlay_assets[m_window->swapchain().image_index()];
                       overlay_asset.object_by_index_fn)
                    {
                        auto [object_id, sub_entry] = overlay_asset.object_by_index_fn(draw_idx);
                        picked_object = m_scene->object_by_id(object_id);
                        picked_objects.insert(picked_object);
                    }
                    spdlog::trace("picked object({}/{}): {}", i + 1, picked_ids.size(), picked_object->name);
                    m_selected_indices.insert(draw_idx);
                }

                // start new selection
                if(!e.is_control_down() && !picked_objects.empty()) { m_selected_objects.clear(); }

                for(auto *po: picked_objects)
                {
                    auto picked_object = po->shared_from_this();
                    if(e.is_control_down() && m_selected_objects.contains(picked_object))
                    {
                        m_selected_objects.erase(picked_object);
                    }
                    else
                    {
                        m_selected_objects.insert(picked_object);
                    }
                }
            }
        }
    };

    simple_mouse.mouse_move = [this](const vierkant::MouseEvent &) {
        if(m_window->cursor_mode() != vierkant::Window::CursorMode::Captured) { m_window->set_cursor_visible(true); }
    };

    simple_mouse.mouse_drag = [this](const vierkant::MouseEvent &e) {
        if(!m_settings.draw_ui || !(m_gui_context.capture_flags() & vierkant::gui::Context::WantCaptureMouse))
        {
            if(e.is_left())
            {
                glm::ivec2 tl = {std::min<int>(e.get_x(), m_ui_state->last_click.x),
                                 std::min<int>(e.get_y(), m_ui_state->last_click.y)};
                glm::ivec2 size = glm::abs(e.position() - m_ui_state->last_click);
                float scale = m_window->content_scale().y;
                m_selection_area = {
                        static_cast<int>(scale * tl.x),
                        static_cast<int>(scale * tl.y),
                        static_cast<int>(scale * size.x),
                        static_cast<int>(scale * size.y),
                };
            }
        }
    };
    m_window->mouse_delegates["simple_mouse"] = simple_mouse;

    // attach drag/drop mouse-delegate
    vierkant::mouse_delegate_t file_drop_delegate = {};
    file_drop_delegate.file_drop = [this](const vierkant::MouseEvent &, const std::vector<std::string> &files) {
        auto &f = files.back();
        load_file(f, false);
    };
    m_window->mouse_delegates["filedrop"] = file_drop_delegate;
}

void VierkantEd::create_camera_controls()
{
    // restore settings
    m_camera_control.orbit = m_settings.orbit_camera;
    m_camera_control.orbit->screen_size = m_window->size();
    m_camera_control.orbit->enabled = true;

    m_camera_control.fly = m_settings.fly_camera;

    if(m_settings.camera_control == CameraControlMode::Fly) { m_camera_control.current = m_camera_control.fly; }
    else
    {
        m_camera_control.current = m_camera_control.orbit;
    }

    // viewport-camera: an editor-layer member of the scene-graph, so the scene-ui's camera-tab
    // lists it next to the scene-cameras. its layer keeps it out of rendering, physics and saving.
    if(m_editor_camera) { m_scene->remove_object(m_editor_camera); }
    m_editor_camera = m_object_store->create_object();
    m_editor_camera->add_component<vierkant::camera_component_t>();
    m_editor_camera->name = "editor-camera";
    m_editor_camera->layers = vierkant::LAYER_EDITOR;
    m_scene->add_object(m_editor_camera);
    m_render_camera = m_editor_camera;

    // attach arcball mouse delegate
    auto arcball_delegeate = m_camera_control.orbit->mouse_delegate();
    arcball_delegeate.enabled = [this]() {
        bool is_active = m_camera_control.current == m_camera_control.orbit && editor_camera_active();
        bool ui_captured =
                m_settings.draw_ui && m_gui_context.capture_flags() & vierkant::gui::Context::WantCaptureMouse;
        return is_active && !ui_captured;
    };
    m_window->mouse_delegates["orbit"] = std::move(arcball_delegeate);
    {
        auto orbit_js = m_camera_control.orbit->joystick_delegate();
        orbit_js.enabled = [this]() {
            return m_camera_control.current == m_camera_control.orbit && !m_settings.character_input &&
                   editor_camera_active();
        };
        m_window->joystick_delegates["orbit"] = std::move(orbit_js);
    }

    auto flycamera_delegeate = m_camera_control.fly->mouse_delegate();
    flycamera_delegeate.enabled = [this]() {
        bool is_active = m_camera_control.current == m_camera_control.fly && editor_camera_active();
        bool ui_captured =
                m_settings.draw_ui && m_gui_context.capture_flags() & vierkant::gui::Context::WantCaptureMouse;
        return is_active && !ui_captured;
    };
    m_window->mouse_delegates["flycamera"] = std::move(flycamera_delegeate);

    auto fly_key_delegeate = m_camera_control.fly->key_delegate();
    fly_key_delegeate.enabled = [this]() {
        bool is_active = m_camera_control.current == m_camera_control.fly && editor_camera_active();
        bool ui_captured =
                m_settings.draw_ui && m_gui_context.capture_flags() & vierkant::gui::Context::WantCaptureMouse;
        return is_active && !ui_captured;
    };
    m_window->key_delegates["flycamera"] = std::move(fly_key_delegeate);
    {
        auto fly_js = m_camera_control.fly->joystick_delegate();
        vierkant::joystick_delegate_t custom_fly_js = {};
        custom_fly_js.enabled = [this]() {
            return m_camera_control.current == m_camera_control.fly && !m_settings.character_input &&
                   editor_camera_active();
        };
        custom_fly_js.joystick_cb = [this, fly_cb = fly_js.joystick_cb](const auto &joysticks) {
            m_fly_joystick_states = joysticks;
            if(fly_cb) { fly_cb(joysticks); }
        };
        m_window->joystick_delegates["flycamera"] = std::move(custom_fly_js);
    }

    {
        // gamepad-only: mouse/keyboard keep driving the camera-control
        auto player_js = m_player_control->joystick_delegate();
        player_js.enabled = [this]() { return m_settings.character_input; };
        m_window->joystick_delegates["player"] = std::move(player_js);
    }

    // update camera with arcball
    auto transform_cb = [this](const vierkant::transform_t &transform) {
        m_editor_camera->set_global_transform(transform);

        // with a drift-budget set, the path-tracer sizes its own window from the camera-motion
        if(m_path_tracer && m_path_tracer->settings.max_accumulation_drift <= 0.f)
        {
            m_path_tracer->reset_accumulator();
        }

        if(m_camera_control.current == m_camera_control.orbit)
        {
            auto *cam_cmp = m_editor_camera->get_component_ptr<vierkant::camera_component_t>();
            assert(cam_cmp);

            if(cam_cmp->projection == vierkant::camera_component_t::ORTHO)
            {
                // frame the ortho-view like the perspective-view would be framed at the orbit-pivot
                float aspect = m_window->aspect_ratio();
                float size = m_camera_control.orbit->distance * std::tan(0.5f * cam_cmp->physical.fovx() / aspect);

                cam_cmp->ortho.top = size;
                cam_cmp->ortho.bottom = -size;
                cam_cmp->ortho.left = -size * aspect;
                cam_cmp->ortho.right = size * aspect;
            }
        }
    };
    m_camera_control.orbit->transform_cb = transform_cb;
    m_camera_control.fly->transform_cb = transform_cb;

    // toggle ortho
    if(m_settings.ortho_camera) { toggle_ortho_camera(); }

    // update camera from current
    m_editor_camera->set_transform(m_camera_control.current->transform());
}

void VierkantEd::update_js(double time_delta)
{
    // the fly-delegate stops updating the states while the character consumes the gamepad
    if(m_camera_control.current == m_camera_control.fly && !m_settings.character_input && editor_camera_active() &&
       !m_fly_joystick_states.empty())
    {
        const auto &js_state = m_fly_joystick_states.front();
        constexpr float deadzone_thresh = 0.008f;
        auto trigger = js_state.trigger();
        float throttle = trigger.y - trigger.x;
        if(std::abs(throttle) > deadzone_thresh)
        {
            auto *cam_cmp = m_editor_camera->get_component_ptr<vierkant::camera_component_t>();
            if(cam_cmp->projection == vierkant::camera_component_t::PERSPECTIVE)
            {
                auto *params = &cam_cmp->physical;
                constexpr float focus_sensitivity = 2.f;
                params->focal_distance = std::clamp(params->focal_distance * std::exp(static_cast<float>(time_delta) *
                                                                                      focus_sensitivity * throttle),
                                                    params->clipping_distances.x, params->clipping_distances.y);
                if(m_path_tracer) { m_path_tracer->reset_accumulator(); }
            }
        }
    }
}