//
// Created by crocdialer on 4/20/18.
//

#include <cmath>


#include <vierkant/imgui/imgui_util.h>
#include <vierkant/PBRDeferred.hpp>
#include <vierkant/PBRPathTracer.hpp>
#include <vierkant/GBuffer.hpp>

#include "imgui_internal.h"
#include "vierkant/Visitor.hpp"

using namespace crocore;

namespace vierkant::gui
{

void draw_material_ui(const MaterialPtr &material);

const ImVec4 gray(.6, .6, .6, 1.);

void draw_application_ui(const crocore::ApplicationPtr &app, const vierkant::WindowPtr &window)
{
    int corner = 0;
    bool is_open = true;
    bool is_fullscreen = window->fullscreen();
    bool v_sync = window->swapchain().v_sync();
    VkSampleCountFlagBits msaa_current = window->swapchain().sample_count();

    auto create_swapchain = [app, window](VkSampleCountFlagBits sample_count, bool v_sync)
    {
        app->main_queue().post([window, sample_count, v_sync]()
                               {
                                   window->create_swapchain(window->swapchain().device(), sample_count, v_sync);
                               });
    };

    ImGuiIO &io = ImGui::GetIO();
    bool is_child_window = ImGui::GetCurrentContext()->CurrentWindowStack.Size > 1;

    if(!is_child_window)
    {
        float bg_alpha = .35f;
        const float DISTANCE = 10.0f;
        ImVec2 window_pos = ImVec2((corner & 1) ? io.DisplaySize.x - DISTANCE : DISTANCE,
                                   (corner & 2) ? io.DisplaySize.y - DISTANCE : DISTANCE);
        ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        ImGui::SetNextWindowBgAlpha(bg_alpha);

        ImGui::Begin("about: blank", &is_open,
                     (corner != -1 ? ImGuiWindowFlags_NoMove : 0) | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    }

    const char *log_items[] = {"Trace", "Debug", "Info", "Warn", "Error", "Critical", "Off"};
    int log_level = static_cast<int>(spdlog::get_level());

    if(ImGui::Combo("log level", &log_level, log_items, IM_ARRAYSIZE(log_items)))
    {
        spdlog::set_level(spdlog::level::level_enum(log_level));
    }
    ImGui::Spacing();
    ImGui::Text("time: %s", crocore::secs_to_time_str(app->application_time()).c_str());
    ImGui::Spacing();

    ImGui::Text("%.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
    ImGui::SameLine();

    if(ImGui::Checkbox("fullscreen", &is_fullscreen))
    {
        size_t monitor_index = window->monitor_index();
        window->set_fullscreen(is_fullscreen, monitor_index);
    }
    ImGui::SameLine();

    if(ImGui::Checkbox("vsync", &v_sync))
    {
        create_swapchain(window->swapchain().sample_count(), v_sync);
        app->loop_throttling = !v_sync;
    }
    if(!v_sync)
    {
        auto target_fps = static_cast<float>(app->target_loop_frequency);
        if(ImGui::SliderFloat("fps", &target_fps, 0.f, 1000.f)){ app->target_loop_frequency = target_fps; }
    }
    ImGui::Spacing();

    auto clear_color = window->swapchain().framebuffers().front().clear_color;

    if(ImGui::ColorEdit4("clear color", clear_color.float32))
    {
        for(auto &framebuffer: window->swapchain().framebuffers()){ framebuffer.clear_color = clear_color; }
    }

    VkSampleCountFlagBits const msaa_levels[] = {VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_2_BIT,
                                                 VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT};
    const char *msaa_items[] = {"None", "MSAA 2x", "MSAA 4x", "MSAA 8x"};
    int msaa_index = 0;

    for(auto lvl: msaa_levels)
    {
        if(msaa_current == lvl){ break; }
        msaa_index++;
    }

    if(ImGui::Combo("multisampling", &msaa_index, msaa_items, IM_ARRAYSIZE(msaa_items)))
    {
        create_swapchain(msaa_levels[msaa_index], v_sync);
    }

    ImGui::Spacing();
    auto loop_time = app->current_loop_time();
    ImGui::Text("fps: %.1f (%.1f ms)", 1.f / loop_time, loop_time * 1000.f);

    if(!is_child_window){ ImGui::End(); }
}

void draw_logger_ui(const std::deque<std::pair<std::string, spdlog::level::level_enum>> &items)
{
    constexpr char window_name[] = "log";
    constexpr int corner = 2;
    const float DISTANCE = 10.0f;

    ImGuiIO &io = ImGui::GetIO();

    auto w = ImGui::FindWindowByName(window_name);
    bool is_minimized = w != nullptr && w->Collapsed;

    ImVec2 window_pos = ImVec2((corner & 1) ? io.DisplaySize.x - DISTANCE : DISTANCE,
                               (corner & 2) ? io.DisplaySize.y - DISTANCE : DISTANCE);
    ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);

    float min_width = is_minimized ? 100 : io.DisplaySize.x - 2 * DISTANCE;
    float max_height = 2 * io.DisplaySize.y / 3.f;

    ImGui::SetNextWindowSizeConstraints(ImVec2(min_width, 0),
                                        ImVec2(min_width, max_height));
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);

    if(ImGui::Begin(window_name, nullptr,
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        uint32_t color_white = 0xFFFFFFFF;
        uint32_t color_error = 0xFF6666FF;
        uint32_t color_warn = 0xFF09AADD;
        uint32_t color_debug = 0xFFFFCCDD;

        ImGuiListClipper clipper(static_cast<int>(items.size()));

        while(clipper.Step())
        {
            for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                const auto &[msg, log_level] = items[i];
                uint32_t color = color_white;
                if(log_level == spdlog::level::err){ color = color_error; }
                else if(log_level == spdlog::level::warn){ color = color_warn; }
                else if(log_level == spdlog::level::debug){ color = color_debug; }

                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::Text("%s", msg.c_str());
                ImGui::PopStyleColor();
            }

        }
        if(ImGui::GetScrollY() >= ImGui::GetScrollMaxY()){ ImGui::SetScrollHereY(); }
    }
    ImGui::End();

//    ImGui::PopStyleColor(ImGuiCol_TitleBg);
}

void draw_images_ui(const std::vector<vierkant::ImagePtr> &images)
{
    constexpr char window_name[] = "textures";
    bool is_child_window = ImGui::GetCurrentContext()->CurrentWindowStack.Size > 1;

    if(!is_child_window){ ImGui::Begin(window_name); }

    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 uv_0(0, 0), uv_1(1, 1);

    for(const auto &tex: images)
    {
        if(tex)
        {
            ImVec2 sz(w, w / (tex->width() / (float) tex->height()));
            ImGui::Image((ImTextureID) (tex.get()), sz, uv_0, uv_1);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }
    }
    // end window
    if(!is_child_window){ ImGui::End(); }
}

void draw_scene_renderer_ui_intern(const PBRDeferredPtr &pbr_renderer, const CameraPtr &cam)
{
    int res[2] = {static_cast<int>(pbr_renderer->settings.resolution.x),
                  static_cast<int>(pbr_renderer->settings.resolution.y)};
    if(ImGui::InputInt2("resolution", res) &&
       res[0] > 0 && res[1] > 0){ pbr_renderer->settings.resolution = {res[0], res[1]}; }

    ImGui::Checkbox("skybox", &pbr_renderer->settings.draw_skybox);
    ImGui::Checkbox("disable material", &pbr_renderer->settings.disable_material);
    ImGui::Checkbox("debug draw ids", &pbr_renderer->settings.debug_draw_ids);
    ImGui::Checkbox("frustum culling", &pbr_renderer->settings.frustum_culling);
    ImGui::Checkbox("occlusion culling", &pbr_renderer->settings.occlusion_culling);
    ImGui::Checkbox("enable lod", &pbr_renderer->settings.enable_lod);
    ImGui::Checkbox("indirect draw", &pbr_renderer->settings.indirect_draw);
    ImGui::Checkbox("meshlet pipeline", &pbr_renderer->settings.use_meshlet_pipeline);
    ImGui::Checkbox("tesselation", &pbr_renderer->settings.tesselation);
    ImGui::Checkbox("wireframe", &pbr_renderer->settings.wireframe);
    ImGui::Checkbox("taa", &pbr_renderer->settings.use_taa);
    ImGui::Checkbox("fxaa", &pbr_renderer->settings.use_fxaa);
    ImGui::SliderFloat("environment", &pbr_renderer->settings.environment_factor, 0.f, 5.f);
    ImGui::Checkbox("tonemap", &pbr_renderer->settings.tonemap);
    ImGui::Checkbox("bloom", &pbr_renderer->settings.bloom);
    ImGui::Checkbox("motionblur", &pbr_renderer->settings.motionblur);

    // motionblur gain
    ImGui::SliderFloat("motionblur gain", &pbr_renderer->settings.motionblur_gain, 0.f, 10.f);

    // exposure
    ImGui::SliderFloat("exposure", &pbr_renderer->settings.exposure, 0.f, 10.f);

    // gamma
    ImGui::SliderFloat("gamma", &pbr_renderer->settings.gamma, 0.f, 10.f);

    if(pbr_renderer)
    {
        auto extent = pbr_renderer->lighting_buffer().extent();

        if(ImGui::TreeNode("statistics"))
        {
            auto stats = pbr_renderer->statistics();

            const auto &draw_result = stats.back().draw_cull_result;

            std::vector<PBRDeferred::statistics_t> values(stats.begin(), stats.end());
            auto max_axis_x = static_cast<double>(pbr_renderer->settings.timing_history_size);

            // drawcall/culling plots
            if(ImGui::TreeNode("drawcalls"))
            {
                if(ImPlot::BeginPlot("##drawcalls"))
                {

                    float bg_alpha = .0f;
                    ImVec4 *implot_colors = ImPlot::GetStyle().Colors;
                    implot_colors[ImPlotCol_FrameBg] = ImVec4(0, 0, 0, bg_alpha);

                    uint32_t max_draws =
                            (std::max_element(values.begin(), values.end(), [](const auto &lhs, const auto &rhs)
                            {
                                return lhs.draw_cull_result.draw_count < rhs.draw_cull_result.draw_count;
                            }))->draw_cull_result.draw_count;

                    ImPlot::SetupAxes("frames", "count", ImPlotAxisFlags_None, ImPlotAxisFlags_NoLabel);
                    ImPlot::SetupAxesLimits(0, max_axis_x, 0, max_draws, ImPlotCond_Always);

                    ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.5f);
                    ImPlot::PlotShaded(
                            "frustum culled",
                            reinterpret_cast<const uint32_t *>(
                                    (uint8_t *) values.data() +
                                    offsetof(PBRDeferred::statistics_t, draw_cull_result.num_frustum_culled)),
                            static_cast<int>(values.size()),
                            0.0, 1.0, 0.0, 0, 0, sizeof(PBRDeferred::statistics_t));
                    ImPlot::PlotShaded(
                            "occluded",
                            reinterpret_cast<const uint32_t *>(
                                    (uint8_t *) values.data() +
                                    offsetof(PBRDeferred::statistics_t, draw_cull_result.num_occlusion_culled)),
                            static_cast<int>(values.size()), 0.0, 1.0, 0.0, 0, 0, sizeof(PBRDeferred::statistics_t));
                    ImPlot::PopStyleVar();
                    ImPlot::EndPlot();
                }
                ImGui::TreePop();
            }

            ImGui::BulletText("drawcount: %d", draw_result.draw_count);
            ImGui::BulletText("num_triangles: %d", draw_result.num_triangles);
            ImGui::BulletText("num_frustum_culled: %d", draw_result.num_frustum_culled);
            ImGui::BulletText("num_occlusion_culled: %d", draw_result.num_occlusion_culled);
            ImGui::Separator();
            ImGui::Spacing();

            if(ImGui::TreeNode("timings"))
            {
                if(ImPlot::BeginPlot("##pbr_timings"))
                {
                    double max_ms = (std::max_element(values.begin(), values.end(), [](const auto &lhs, const auto &rhs)
                    {
                        return lhs.timings.total_ms < rhs.timings.total_ms;
                    }))->timings.total_ms;

                    ImPlot::SetupAxes("frames", "ms", ImPlotAxisFlags_None, ImPlotAxisFlags_NoLabel);
                    ImPlot::SetupAxesLimits(0, max_axis_x, 0, max_ms, ImPlotCond_Always);
                    ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.5f);

                    auto *ptr = reinterpret_cast<double *>((uint8_t *) values.data() +
                                                           offsetof(PBRDeferred::statistics_t, timings.total_ms));
                    ImPlot::PlotShaded("total ms", ptr, static_cast<int>(values.size()), 0.0, 1.0, 0.0, 0, 0,
                                       sizeof(PBRDeferred::statistics_t));
                    ImPlot::PopStyleVar();
                    ImPlot::EndPlot();
                }
                ImGui::TreePop();
            }
            const auto &last = stats.back().timings;
            ImGui::BulletText("g_buffer_pre: %.3f ms", last.g_buffer_pre_ms);
            ImGui::BulletText("depth_pyramid: %.3f ms", last.depth_pyramid_ms);
            ImGui::BulletText("culling: %.3f ms", last.culling_ms);
            ImGui::BulletText("g_buffer_post: %.3f ms", last.g_buffer_post_ms);
            ImGui::BulletText("lighting: %.3f ms", last.lighting_ms);
            ImGui::BulletText("taa: %.3f ms", last.taa_ms);
            ImGui::TreePop();
        }

        if(ImGui::TreeNode("g-buffer", "g-buffer (%d)", vierkant::G_BUFFER_SIZE))
        {
            std::vector<vierkant::ImagePtr> images(vierkant::G_BUFFER_SIZE);

            for(uint32_t i = 0; i < vierkant::G_BUFFER_SIZE; ++i)
            {
                images[i] = pbr_renderer->g_buffer().color_attachment(i);
            }
            vierkant::gui::draw_images_ui(images);

            ImGui::TreePop();
        }
        if(ImGui::TreeNode("lighting buffer", "lighting buffer (%d x %d)", extent.width, extent.height))
        {
            vierkant::gui::draw_images_ui({pbr_renderer->bsdf_lut()});
            vierkant::gui::draw_images_ui({pbr_renderer->lighting_buffer().color_attachment()});

            ImGui::TreePop();
        }
    }
}

void draw_scene_renderer_ui_intern(const PBRPathTracerPtr &path_tracer, const CameraPtr &cam)
{
    int res[2] = {static_cast<int>(path_tracer->settings.resolution.x),
                  static_cast<int>(path_tracer->settings.resolution.y)};
    if(ImGui::InputInt2("resolution", res) &&
       res[0] > 0 && res[1] > 0){ path_tracer->settings.resolution = {res[0], res[1]}; }

    ImGui::Text("%s", (std::to_string(path_tracer->current_batch()) + " / ").c_str());
    ImGui::SameLine();
    int max_num_batches = static_cast<int>(path_tracer->settings.max_num_batches);
    int num_samples = static_cast<int>(path_tracer->settings.num_samples);
    int max_trace_depth = static_cast<int>(path_tracer->settings.max_trace_depth);

    if(ImGui::InputInt("num batches", &max_num_batches) &&
       max_num_batches >= 0){ path_tracer->settings.max_num_batches = max_num_batches; }
    if(ImGui::InputInt("spp (samples/pixel)", &num_samples) &&
       num_samples >= 0){ path_tracer->settings.num_samples = num_samples; }
    if(ImGui::InputInt("max bounces", &max_trace_depth) &&
       max_trace_depth >= 0){ path_tracer->settings.max_trace_depth = max_trace_depth; }

    ImGui::Checkbox("skybox", &path_tracer->settings.draw_skybox);
    ImGui::Checkbox("disable material", &path_tracer->settings.disable_material);
    ImGui::Checkbox("denoiser", &path_tracer->settings.denoising);
    ImGui::Checkbox("tonemap", &path_tracer->settings.tonemap);
    ImGui::Checkbox("bloom", &path_tracer->settings.bloom);
    ImGui::SliderFloat("environment_factor", &path_tracer->settings.environment_factor, 0.f, 5.f);

    // exposure
    ImGui::SliderFloat("exposure", &path_tracer->settings.exposure, 0.f, 10.f);

    // gamma
    ImGui::SliderFloat("gamma", &path_tracer->settings.gamma, 0.f, 10.f);

    ImGui::Checkbox("depth of field", &path_tracer->settings.depth_of_field);

    if(path_tracer->settings.depth_of_field)
    {
        // aperture
        constexpr float f_stop_min = 0.1f, f_stop_max = 128.f;

        float f_stop = clamp(1.f / path_tracer->settings.aperture, f_stop_min, f_stop_max);

        if(ImGui::SliderFloat("f-stop", &f_stop, f_stop_min, f_stop_max))
        {
            path_tracer->settings.aperture = 1.f / f_stop;
        }

        // focal distance
        ImGui::SliderFloat("focal distance", &path_tracer->settings.focal_distance, cam->near(), cam->far());
    }
}

void draw_scene_renderer_ui(const SceneRendererPtr &scene_renderer, const CameraPtr &cam)
{
    constexpr char window_name[] = "scene_renderer";
    bool is_child_window = ImGui::GetCurrentContext()->CurrentWindowStack.Size > 1;

    if(is_child_window){ ImGui::BeginChild(window_name); }
    else{ ImGui::Begin(window_name); }

    vierkant::dof_settings_t *settings_dof = nullptr;

    if(auto pbr_renderer = std::dynamic_pointer_cast<vierkant::PBRDeferred>(scene_renderer))
    {
        settings_dof = &pbr_renderer->settings.dof;

        draw_scene_renderer_ui_intern(pbr_renderer, cam);
    }
    else if(auto path_tracer = std::dynamic_pointer_cast<vierkant::PBRPathTracer>(scene_renderer))
    {
        draw_scene_renderer_ui_intern(path_tracer, cam);
    }

    ImGui::Separator();

    // dof
    if(settings_dof)
    {
        auto &dof = *settings_dof;

        ImGui::PushID(std::addressof(dof));
        ImGui::Checkbox("", reinterpret_cast<bool *>(std::addressof(dof.enabled)));
        ImGui::PopID();

        ImGui::SameLine();
        if(!dof.enabled){ ImGui::PushStyleColor(ImGuiCol_Text, gray); }

        if(ImGui::TreeNode("dof"))
        {

            ImGui::Checkbox("autofocus", reinterpret_cast<bool *>(std::addressof(dof.auto_focus)));
            ImGui::SliderFloat("focal distance (m)", &dof.focal_depth, 0.f, cam->far());
            ImGui::SliderFloat("focal length (mm)", &dof.focal_length, 0.f, 280.f);
            ImGui::SliderFloat("f-stop", &dof.fstop, 0.f, 180.f);
            ImGui::InputFloat("circle of confusion (mm)", &dof.circle_of_confusion_sz, 0.001f, .01f);
            ImGui::SliderFloat("gain", &dof.gain, 0.f, 10.f);
            ImGui::SliderFloat("color fringe", &dof.fringe, 0.f, 10.f);
            ImGui::SliderFloat("max blur", &dof.max_blur, 0.f, 10.f);
            ImGui::Checkbox("debug focus", reinterpret_cast<bool *>(std::addressof(dof.debug_focus)));
            ImGui::TreePop();
        }
        if(!dof.enabled){ ImGui::PopStyleColor(); }
    }

    // end window
    if(is_child_window){ ImGui::EndChild(); }
    else{ ImGui::End(); }
}

vierkant::Object3DPtr draw_scenegraph_ui_helper(const vierkant::Object3DPtr &obj,
                                                const std::set<vierkant::Object3DPtr> *selection)
{
    vierkant::Object3DPtr ret;
    ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    node_flags |= selection && selection->count(obj) ? ImGuiTreeNodeFlags_Selected : 0;

    // push object id
    ImGui::PushID(obj->id());
    bool is_enabled = obj->enabled;
    if(ImGui::Checkbox("", &is_enabled)){ obj->enabled = is_enabled; }
    ImGui::SameLine();

    if(!is_enabled){ ImGui::PushStyleColor(ImGuiCol_Text, gray); }

    if(obj->children.empty())
    {
        node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen; // ImGuiTreeNodeFlags_Bullet
        ImGui::TreeNodeEx((void *) (uintptr_t) obj->id(), node_flags, "%s", obj->name.c_str());
        if(ImGui::IsItemClicked()){ ret = obj; }
    }
    else
    {
        bool is_open = ImGui::TreeNodeEx((void *) (uintptr_t) obj->id(), node_flags, "%s",
                                         obj->name.c_str());
        if(ImGui::IsItemClicked()){ ret = obj; }

        if(is_open)
        {
            for(auto &c: obj->children)
            {
                auto clicked_obj = draw_scenegraph_ui_helper(c, selection);
                if(!ret){ ret = clicked_obj; }
            }
            ImGui::TreePop();
        }
    }
    if(!is_enabled){ ImGui::PopStyleColor(); }
    ImGui::PopID();
    return ret;
}

void draw_scene_ui(const SceneConstPtr &scene, const vierkant::CameraConstPtr &camera,
                   std::set<vierkant::Object3DPtr> *selection)
{
    constexpr char window_name[] = "scene";
    bool is_child_window = ImGui::GetCurrentContext()->CurrentWindowStack.Size > 1;
    bool is_open = false;

    if(is_child_window){ is_open = ImGui::BeginChild(window_name); }
    else{ is_open = ImGui::Begin(window_name); }

    if(is_open)
    {
        ImGui::BeginTabBar("scene_tabs");
        if(ImGui::BeginTabItem("scene"))
        {
            // draw a tree for the scene-objects
            auto clicked_obj = draw_scenegraph_ui_helper(scene->root(), selection);

            // add / remove an object from selection
            if(clicked_obj && selection)
            {
                if(ImGui::GetIO().KeyCtrl)
                {
                    if(selection->contains(clicked_obj)){ selection->erase(clicked_obj); }
                    else{ selection->insert(clicked_obj); }
                }
                else
                {
                    selection->clear();
                    selection->insert(clicked_obj);
                }
            }
            ImGui::Separator();
            if(selection){ for(auto &obj: *selection){ draw_object_ui(obj, camera); }}

            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("materials"))
        {
            std::unordered_set<vierkant::MaterialPtr> material_map;
            std::vector<vierkant::MaterialPtr> materials;

            auto view = scene->registry()->view<vierkant::MeshPtr>();

            // uniquely gather all materials in order
            for(const auto &[entity, mesh] : view.each())
            {
                if(mesh)
                {
                    for(const auto &m: mesh->materials)
                    {
                        if(!material_map.contains(m))
                        {
                            materials.push_back(m);
                            material_map.insert(m);
                        }
                    }
                }
            }
            for(uint32_t i = 0; i < materials.size(); ++i)
            {
                const auto &mat = materials[i];
                auto mat_name = mat->name.empty() ? std::to_string(i) : mat->name;

                if(mat && ImGui::TreeNode((void *) (mat.get()), "%s", mat_name.c_str()))
                {
                    draw_material_ui(mat);
                    ImGui::Separator();
                    ImGui::TreePop();
                }
            }

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // end window
    if(is_child_window){ ImGui::EndChild(); }
    else{ ImGui::End(); }
}

void draw_material_ui(const MaterialPtr &material)
{
    const float w = ImGui::GetContentRegionAvail().x;

    auto draw_texture = [&material, w](vierkant::Material::TextureType type, const std::string &text)
    {
        auto it = material->textures.find(type);

        if(it != material->textures.end())
        {
            const auto &img = it->second;
            bool is_bc7 = img->format().format == VK_FORMAT_BC7_UNORM_BLOCK ||
                          img->format().format == VK_FORMAT_BC7_SRGB_BLOCK;
            ImVec2 sz(w, w / (img->width() / (float) img->height()));
            ImGui::BulletText("%s (%d x %d%s)", text.c_str(), img->width(), img->height(), is_bc7 ? ", BC7" : "");
            ImGui::Image((ImTextureID) (img.get()), sz);
            ImGui::Separator();
        }
    };

    ImGui::BulletText("name: %s", material->name.c_str());
    ImGui::Separator();

    // base color
    ImGui::ColorEdit4("base color", glm::value_ptr(material->color));
    draw_texture(vierkant::Material::TextureType::Color, "base color");

    // emissive color
    ImGui::ColorEdit3("emission color", glm::value_ptr(material->emission));
    ImGui::InputFloat("emissive strength", &material->emission.w);
    draw_texture(vierkant::Material::TextureType::Emission, "emission");

    // normalmap
    draw_texture(vierkant::Material::TextureType::Normal, "normals");

    ImGui::Separator();

    // roughness
    ImGui::SliderFloat("roughness", &material->roughness, 0.f, 1.f);

    // metalness
    ImGui::SliderFloat("metalness", &material->metalness, 0.f, 1.f);

    // occlusion
    ImGui::SliderFloat("occlusion", &material->occlusion, 0.f, 1.f);

    // ambient-occlusion / roughness / metalness
    draw_texture(vierkant::Material::TextureType::Ao_rough_metal, "occlusion / roughness / metalness");

    ImGui::Separator();

    ImGui::Text("blending/transmission");

    // blend-mode
    const char *blend_items[] = {"Opaque", "Blend", "Mask"};
    constexpr Material::BlendMode blend_modes[] = {Material::BlendMode::Opaque, Material::BlendMode::Blend,
                                                   Material::BlendMode::Mask};
    int blend_mode_index = 0;

    for(auto blend_mode: blend_modes)
    {
        if(material->blend_mode == blend_mode){ break; }
        blend_mode_index++;
    }

    if(ImGui::Combo("blend-mode", &blend_mode_index, blend_items, IM_ARRAYSIZE(blend_items)))
    {
        material->blend_mode = blend_modes[blend_mode_index];
    }

    if(material->blend_mode == Material::BlendMode::Mask)
    {
        // alpha-cutoff
        ImGui::SliderFloat("alpha-cutoff", &material->alpha_cutoff, 0.f, 1.f);
    }

    // two-sided
    ImGui::Checkbox("two-sided", &material->two_sided);

    // transmission
    ImGui::SliderFloat("transmission", &material->transmission, 0.f, 1.f);
    draw_texture(vierkant::Material::TextureType::Transmission, "transmission");

    // attenuation distance
    ImGui::InputFloat("attenuation distance", &material->attenuation_distance);

    // attenuation color
    ImGui::ColorEdit3("attenuation color", glm::value_ptr(material->attenuation_color));

    // index of refraction - ior
    ImGui::InputFloat("ior", &material->ior);

    // sheen
    ImGui::Separator();
    ImGui::Text("sheen");
    ImGui::ColorEdit3("sheen color", glm::value_ptr(material->sheen_color));
    ImGui::SliderFloat("sheen roughness", &material->sheen_roughness, 0.f, 1.f);

    // iridescence
    ImGui::Separator();
    ImGui::Text("iridescence");
    ImGui::SliderFloat("iridescence", &material->iridescence_factor, 0.f, 1.f);
    ImGui::InputFloat("iridescence-ior", &material->iridescence_ior);
    ImGui::InputFloat2("iridescence thickness", glm::value_ptr(material->iridescence_thickness_range));

    // clearcoat
    ImGui::Separator();
    ImGui::Text("clearcoat");
    ImGui::SliderFloat("clearcoat factor", &material->clearcoat_factor, 0.f, 1.f);
    ImGui::SliderFloat("clearcoat roughness", &material->clearcoat_roughness_factor, 0.f, 1.f);
}

void draw_mesh_ui(const vierkant::Object3DPtr &object, const vierkant::MeshPtr &mesh)
{
    if(!object || !mesh){ return; }

    size_t num_vertices = 0, num_faces = 0;

    for(const auto &e: mesh->entries)
    {
        num_vertices += e.num_vertices;
        num_faces += e.lods.empty() ? 0 : e.lods[0].num_indices / 3;
    }
    ImGui::Separator();
    ImGui::BulletText("%zu positions", num_vertices);
    ImGui::BulletText("%zu faces", num_faces);
    ImGui::BulletText("%d bones", vierkant::nodes::num_nodes_in_hierarchy(mesh->root_bone));
    ImGui::Separator();
    ImGui::Spacing();

    // entries
    if(!mesh->entries.empty() && ImGui::TreeNode("entries", "entries (%zu)", mesh->entries.size()))
    {
        size_t index = 0;
        std::hash<vierkant::MeshPtr> hash;

        for(auto &e: mesh->entries)
        {
            int mesh_id = static_cast<int>(hash(mesh));

            // push object id
            ImGui::PushID(static_cast<int>(mesh_id + index));
            ImGui::Checkbox("", &e.enabled);
            ImGui::SameLine();

            if(!e.enabled){ ImGui::PushStyleColor(ImGuiCol_Text, gray); }

            auto entry_name = e.name.empty() ? ("entry " + std::to_string(index)) : e.name;

            if(ImGui::TreeNodeEx((void *) (mesh_id + index), 0, "%s", entry_name.c_str()))
            {
                std::stringstream ss;
                ss << "positions: " << std::to_string(e.num_vertices) << "\n";
                ss << "faces: " << std::to_string(e.lods.empty() ? 0 : e.lods[0].num_indices / 3);
                ss << "lods: " << std::to_string(e.lods.size());
                ImGui::Text("%s", ss.str().c_str());

                int material_index = static_cast<int>(e.material_index);
                if(ImGui::InputInt("material-index", &material_index))
                {
                    e.material_index = std::min<uint32_t>(material_index, mesh->materials.size() - 1);
                }

                ImGui::Separator();

                // material ui
                draw_material_ui(mesh->materials[e.material_index]);

                ImGui::TreePop();
            }

            if(!e.enabled){ ImGui::PopStyleColor(); }
            ImGui::PopID();
            index++;
        }
        ImGui::Separator();
        ImGui::TreePop();
    }

    // materials
    if(!mesh->entries.empty() && ImGui::TreeNode("materials", "materials (%zu)", mesh->materials.size()))
    {
        for(uint32_t i = 0; i < mesh->materials.size(); ++i)
        {
            const auto &mat = mesh->materials[i];
            auto mat_name = mat->name.empty() ? std::to_string(i) : mat->name;

            if(mat && ImGui::TreeNode((void *) (mat.get()), "%s", mat_name.c_str()))
            {
                draw_material_ui(mat);
                ImGui::Separator();
                ImGui::TreePop();
            }
        }
        ImGui::Separator();
        ImGui::TreePop();
    }

    // animation
    if(!mesh->node_animations.empty() && ImGui::TreeNode("animation") && object->has_component<animation_state_t>())
    {
        auto &animation_state = object->get_component<animation_state_t>();

        // animation index
        int animation_index = static_cast<int>(animation_state.index);

        std::vector<const char *> animation_items;
        for(auto &anim: mesh->node_animations){ animation_items.push_back(anim.name.c_str()); }

        if(ImGui::Combo("name", &animation_index, animation_items.data(), animation_items.size()))
        {
            animation_state.index = animation_index;
        }

        auto &animation = mesh->node_animations[animation_state.index];

        // animation speed
        auto speed = static_cast<float>(animation_state.animation_speed);
        if(ImGui::SliderFloat("speed", &speed, -3.f, 3.f)){ animation_state.animation_speed = speed; }
        ImGui::SameLine();
        if(ImGui::Checkbox("play", &animation_state.playing)){}

        // interpolation-mode
        const char *interpolation_mode_items[] = {"Linear", "Step", "CubicSpline"};
        constexpr InterpolationMode interpolation_modes[] = {InterpolationMode::Linear, InterpolationMode::Step,
                                                             InterpolationMode::CubicSpline};
        int mode_index = 0;

        for(auto mode: interpolation_modes)
        {
            if(animation.interpolation_mode == mode){ break; }
            mode_index++;
        }

        if(ImGui::Combo("interpolation", &mode_index, interpolation_mode_items, IM_ARRAYSIZE(interpolation_mode_items)))
        {
            animation.interpolation_mode = interpolation_modes[mode_index];
        }

        float current_time = static_cast<float>(animation_state.current_time) / animation.ticks_per_sec;
        float duration = animation.duration / animation.ticks_per_sec;

        // animation current time / max time
        if(ImGui::SliderFloat(("/ " + crocore::to_string(duration, 2) + " s").c_str(),
                              &current_time, 0.f, duration))
        {
            animation_state.current_time = current_time * animation.ticks_per_sec;
        }
        ImGui::Separator();
        ImGui::TreePop();
    }
}

void draw_object_ui(const Object3DPtr &object, const vierkant::CameraConstPtr &camera)
{
    constexpr char window_name[] = "object";
    constexpr int32_t gizmo_inactive = -1;
    static int32_t current_gizmo = gizmo_inactive;

    bool is_child_window = ImGui::GetCurrentContext()->CurrentWindowStack.Size > 1;
    bool draw_guizmo = false;

    if(is_child_window){ ImGui::BeginChild(window_name); }
    else{ ImGui::Begin(window_name); }

    ImGui::BulletText("%s", window_name);
    ImGui::Spacing();

    // name
    size_t buf_size = object->name.size() + 4;
    char text_buf[buf_size];
    strcpy(text_buf, object->name.c_str());

    if(ImGui::InputText("name", text_buf, IM_ARRAYSIZE(text_buf), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        object->name = text_buf;
    }

    ImGui::Separator();

    // transform
    if(object && ImGui::TreeNode("transform"))
    {
        glm::mat4 transform = object->transform;
        glm::vec3 position = transform[3].xyz();
        glm::vec3 rotation = glm::degrees(glm::eulerAngles(glm::quat_cast(transform)));
        glm::vec3 scale = glm::vec3(length(transform[0]), length(transform[1]), length(transform[2]));

        constexpr char fmt[] = "%.4f";
        bool changed = ImGui::InputFloat3("position", glm::value_ptr(position), fmt);
        changed = ImGui::InputFloat3("rotation", glm::value_ptr(rotation), fmt) || changed;
        changed = ImGui::InputFloat3("scale", glm::value_ptr(scale), fmt) || changed;

        if(changed)
        {
            auto m = glm::mat4_cast(glm::quat(glm::radians(rotation)));
            m[3] = glm::vec4(position, 1.f);
            m = glm::scale(m, scale);
            object->transform = m;
        }

        ImGui::Separator();

        // imguizmo handle
        draw_guizmo = true;
        if(ImGui::RadioButton("None", current_gizmo == gizmo_inactive)){ current_gizmo = gizmo_inactive; }
        ImGui::SameLine();
        if(ImGui::RadioButton("Translate",
                              current_gizmo == ImGuizmo::TRANSLATE)){ current_gizmo = ImGuizmo::TRANSLATE; }
        ImGui::SameLine();
        if(ImGui::RadioButton("Rotate", current_gizmo == ImGuizmo::ROTATE)){ current_gizmo = ImGuizmo::ROTATE; }
        ImGui::SameLine();
        if(ImGui::RadioButton("Scale", current_gizmo == ImGuizmo::SCALE)){ current_gizmo = ImGuizmo::SCALE; }
        ImGui::Separator();

        ImGui::TreePop();
    }

    // cast to mesh
    if(object->has_component<vierkant::MeshPtr>()){ draw_mesh_ui(object, object->get_component<vierkant::MeshPtr>()); }

    if(is_child_window){ ImGui::EndChild(); }
    else{ ImGui::End(); }

    // imguizmo drawing is in fact another window
    if(draw_guizmo && camera && (current_gizmo != gizmo_inactive))
    {
        glm::mat4 transform = object->global_transform();
        bool is_ortho = std::dynamic_pointer_cast<const vierkant::OrthoCamera>(camera).get();
        auto z_val = transform[3].z;

        ImGuizmo::SetOrthographic(is_ortho);

        auto sz = ImGui::GetIO().DisplaySize;
        auto proj = glm::perspectiveRH(camera->fov(), sz.x / sz.y, camera->near(),
                                       camera->far());
        ImGuizmo::Manipulate(glm::value_ptr(camera->view_matrix()), glm::value_ptr(proj),
                             ImGuizmo::OPERATION(current_gizmo), ImGuizmo::WORLD, glm::value_ptr(transform));
        if(is_ortho){ transform[3].z = z_val; }
        object->set_global_transform(transform);
    }
}

}// namespace vierkant::gui

