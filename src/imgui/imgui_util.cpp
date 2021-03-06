//
// Created by crocdialer on 4/20/18.
//

#include <cmath>

#include <vierkant/MeshNode.hpp>

#include <vierkant/imgui/imgui_util.h>
#include <vierkant/PBRDeferred.hpp>

#include "imgui_internal.h"

using namespace crocore;

namespace vierkant::gui
{

const ImVec4 gray(.6, .6, .6, 1.);

// int
void draw_property_ui(const Property_<int>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(auto ranged_prop = std::dynamic_pointer_cast<RangedProperty<int>>(the_property))
    {
        if(ImGui::SliderInt(prop_name.c_str(), &ranged_prop->value(), ranged_prop->range().first,
                            ranged_prop->range().second))
        {
            the_property->notify_observers();
        }
    }
    else
    {
        if(ImGui::InputInt(prop_name.c_str(), &the_property->value(), 1, 10, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            the_property->notify_observers();
        }
    }
}

void draw_property_ui(const Property_<uint32_t>::Ptr &the_property)
{
    std::string prop_name = the_property->name();
    int val = *the_property;

    if(auto ranged_prop = std::dynamic_pointer_cast<RangedProperty<uint32_t>>(the_property))
    {
        if(ImGui::SliderInt(prop_name.c_str(), &val, ranged_prop->range().first,
                            ranged_prop->range().second))
        {
            *the_property = val;
        }
    }
    else
    {
        if(ImGui::InputInt(prop_name.c_str(), &val, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            *the_property = val;
        }
    }
}

// float
void draw_property_ui(const Property_<float>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(auto ranged_prop = std::dynamic_pointer_cast<RangedProperty<float>>(the_property))
    {
        if(ImGui::SliderFloat(prop_name.c_str(), &ranged_prop->value(), ranged_prop->range().first,
                              ranged_prop->range().second))
        {
            the_property->notify_observers();
        }
    }
    else
    {
        if(ImGui::InputFloat(prop_name.c_str(), &the_property->value(), 0.f, 0.f,
                             (std::abs(the_property->value()) < 1.f) ? 5 : 2,
                             ImGuiInputTextFlags_EnterReturnsTrue))
        {
            the_property->notify_observers();
        }
    }
}

// bool
void draw_property_ui(const Property_<bool>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(ImGui::Checkbox(prop_name.c_str(), &the_property->value()))
    {
        the_property->notify_observers();
    }
}

// gl::vec2
void draw_property_ui(const Property_<glm::vec2>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(ImGui::InputFloat2(prop_name.c_str(), &the_property->value()[0], 2, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        the_property->notify_observers();
    }
}

// gl::vec3
void draw_property_ui(const Property_<glm::vec3>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(ImGui::InputFloat3(prop_name.c_str(), &the_property->value()[0], 2, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        the_property->notify_observers();
    }
}

// gl::ivec2
void draw_property_ui(const Property_<glm::ivec2>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(ImGui::InputInt2(prop_name.c_str(), &the_property->value()[0], ImGuiInputTextFlags_EnterReturnsTrue))
    {
        the_property->notify_observers();
    }
}

// gl::ivec3
void draw_property_ui(const Property_<glm::ivec3>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(ImGui::InputInt3(prop_name.c_str(), &the_property->value()[0], ImGuiInputTextFlags_EnterReturnsTrue))
    {
        the_property->notify_observers();
    }
}

// gl::Color (a.k.a. gl::vec4)
void draw_property_ui(const Property_<glm::vec4>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(ImGui::ColorEdit4(prop_name.c_str(), (float *) &the_property->value()))
    {
        the_property->notify_observers();
    }
}

// std::string
void draw_property_ui(const Property_<std::string>::Ptr &the_property)
{
    std::string prop_name = the_property->name();
    constexpr size_t buf_size = 512;
    char text_buf[buf_size];
    strcpy(text_buf, the_property->value().c_str());

    if(ImGui::InputText(prop_name.c_str(), text_buf, buf_size, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        the_property->value(text_buf);
    }
}

// std::vector<std::string>>
void draw_property_ui(const Property_<std::vector<std::string>>::Ptr &the_property)
{
    std::string prop_name = the_property->name();
    std::vector<std::string> &array = the_property->value();

    if(ImGui::TreeNode(prop_name.c_str()))
    {
        for(size_t i = 0; i < array.size(); ++i)
        {
            size_t buf_size = array[i].size() + 4;
            char text_buf[buf_size];

            strcpy(text_buf, array[i].c_str());

            if(ImGui::InputText(to_string(i).c_str(), text_buf, buf_size,
                                ImGuiInputTextFlags_EnterReturnsTrue))
            {
                array[i] = text_buf;
                the_property->notify_observers();
            }
        }
        ImGui::TreePop();
    }
}

// std::vector<float>>
void draw_property_ui(const Property_<std::vector<float>>::Ptr &the_property)
{
    std::string prop_name = the_property->name();
    std::vector<float> &array = the_property->value();

    if(ImGui::TreeNode(prop_name.c_str()))
    {
        for(size_t i = 0; i < array.size(); ++i)
        {
            if(ImGui::InputFloat(std::to_string(i).c_str(), &array[i], 0.f, 0.f,
                                 (std::abs(array[i]) < 1.f) ? 5 : 2,
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                the_property->notify_observers();
            }
        }
        ImGui::TreePop();
    }
}

// generic
void draw_property_ui(const PropertyPtr &the_property)
{
    std::string prop_name = the_property->name();

    if(ImGui::TreeNode(prop_name.c_str()))
    {
        ImGui::Text("%s", prop_name.c_str());
        ImGui::TreePop();
    }

}

void draw_component_ui(const ComponentConstPtr &the_component)
{
    ImGui::Begin(the_component->name().c_str());

    for(auto &p : the_component->get_property_list())
    {
        // skip non-tweakable properties
        if(!p->tweakable()){ continue; }

        if(p->is_of_type<bool>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<bool>>(p);
            draw_property_ui(cast_prop);
        }
        else if(p->is_of_type<int>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<int>>(p);
            draw_property_ui(cast_prop);
        }
        else if(p->is_of_type<uint32_t>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<uint32_t>>(p);
            draw_property_ui(cast_prop);
        }
        else if(p->is_of_type<float>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<float>>(p);
            draw_property_ui(cast_prop);
        }
        else if(p->is_of_type<glm::vec2>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<glm::vec2>>(p);
            draw_property_ui(cast_prop);
        }
        else if(p->is_of_type<glm::vec3>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<glm::vec3>>(p);
            draw_property_ui(cast_prop);
        }
        else if(p->is_of_type<glm::ivec2>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<glm::ivec2>>(p);
            draw_property_ui(cast_prop);
        }
        else if(p->is_of_type<glm::ivec3>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<glm::ivec3>>(p);
            draw_property_ui(cast_prop);
        }
        else if(p->is_of_type<glm::vec4>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<glm::vec4>>(p);
            draw_property_ui(cast_prop);
        }
        else if(p->is_of_type<std::string>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<std::string>>(p);
            draw_property_ui(cast_prop);
        }
        else if(p->is_of_type<std::vector<std::string>>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<std::vector<std::string>>>(p);
            draw_property_ui(cast_prop);
        }
        else if(p->is_of_type<std::vector<float>>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<std::vector<float>>>(p);
            draw_property_ui(cast_prop);
        }
        else{ draw_property_ui(p); }
    }

    ImGui::End();
}

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

    float bg_alpha = .35f;
    const float DISTANCE = 10.0f;
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 window_pos = ImVec2((corner & 1) ? io.DisplaySize.x - DISTANCE : DISTANCE,
                               (corner & 2) ? io.DisplaySize.y - DISTANCE : DISTANCE);
    ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    ImGui::SetNextWindowBgAlpha(bg_alpha);

    if(ImGui::Begin("Example: Simple overlay", &is_open,
                    (corner != -1 ? ImGuiWindowFlags_NoMove : 0) | ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        ImGui::Text(app->name().c_str());
        ImGui::Separator();

        const char *log_items[] = {"Disabled", "Print", "Fatal", "Error", "Warning", "Info", "Debug", "Trace_1",
                                   "Trace_2", "Trace_3"};
        int log_level = (int) crocore::g_logger.severity();

        if(ImGui::Combo("log level", &log_level, log_items, IM_ARRAYSIZE(log_items)))
        {
            crocore::g_logger.set_severity(crocore::Severity(log_level));
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
        }

        ImGui::Spacing();

        auto clear_color = window->swapchain().framebuffers().front().clear_color;

        if(ImGui::ColorEdit4("clear color", clear_color.float32))
        {
            for(auto &framebuffer : window->swapchain().framebuffers()){ framebuffer.clear_color = clear_color; }
        }

        VkSampleCountFlagBits const msaa_levels[] = {VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_2_BIT,
                                                     VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT};
        const char *msaa_items[] = {"None", "MSAA 2x", "MSAA 4x", "MSAA 8x"};
        int msaa_index = 0;

        for(auto lvl : msaa_levels)
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
    }
    ImGui::End();
}

void draw_images_ui(const std::vector<vierkant::ImagePtr> &images)
{
    constexpr char window_name[] = "textures";
    bool is_child_window = ImGui::GetCurrentContext()->CurrentWindowStack.Size > 1;

    if(!is_child_window){ ImGui::Begin(window_name); }

    const float w = ImGui::GetContentRegionAvailWidth();
    const ImVec2 uv_0(0, 0), uv_1(1, 1);

    for(const auto &tex : images)
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

void draw_scene_renderer_ui(const SceneRendererPtr &scene_renderer, const CameraPtr &cam)
{
    constexpr char window_name[] = "renderer";
    bool is_child_window = ImGui::GetCurrentContext()->CurrentWindowStack.Size > 1;

    if(is_child_window){ ImGui::BeginChild(window_name); }
    else{ ImGui::Begin(window_name); }

    if(cam)
    {
        if(ImGui::TreeNode("camera"))
        {
            auto perspective_cam = std::dynamic_pointer_cast<vierkant::PerspectiveCamera>(cam);

            if(perspective_cam)
            {
                // fov
                float fov = perspective_cam->fov();
                if(ImGui::SliderFloat("fov", &fov, 0.f, 180.f)){ perspective_cam->set_fov(fov); }

                // clipping planes
                float clipping[2] = {perspective_cam->near(), perspective_cam->far()};
                if(ImGui::InputFloat2("clipping near/far", clipping))
                {
                    perspective_cam->set_clipping(clipping[0], clipping[1]);
                }

                // exposure
                ImGui::SliderFloat("exposure", &scene_renderer->settings.exposure, 0.f, 10.f);

                // gamma
                ImGui::SliderFloat("gamma", &scene_renderer->settings.gamma, 0.f, 10.f);

                // dof
                auto &dof = scene_renderer->settings.dof;

                ImGui::PushID(std::addressof(dof));
                ImGui::Checkbox("", reinterpret_cast<bool*>(std::addressof(dof.enabled)));
                ImGui::PopID();

                ImGui::SameLine();
                if(!dof.enabled){ ImGui::PushStyleColor(ImGuiCol_Text, gray); }

                if(ImGui::TreeNode("dof"))
                {

                    ImGui::Checkbox("autofocus", reinterpret_cast<bool*>(std::addressof(dof.auto_focus)));
                    ImGui::SliderFloat("focal distance (m)", &dof.focal_depth, 0.f, cam->far());
                    ImGui::SliderFloat("focal length (mm)", &dof.focal_length, 0.f, 280.f);
                    ImGui::SliderFloat("f-stop", &dof.fstop, 0.f, 180.f);
                    ImGui::InputFloat("circle of confusion (mm)", &dof.circle_of_confusion_sz, 0.001f, .01f);
                    ImGui::SliderFloat("gain", &dof.gain, 0.f, 10.f);
                    ImGui::SliderFloat("color fringe", &dof.fringe, 0.f, 10.f);
                    ImGui::SliderFloat("max blur", &dof.max_blur, 0.f, 10.f);
                    ImGui::Checkbox("debug focus", reinterpret_cast<bool*>(std::addressof(dof.debug_focus)));
                    ImGui::TreePop();
                }
                if(!dof.enabled){ ImGui::PopStyleColor(); }
            }

            ImGui::TreePop();
        }

        ImGui::Separator();
        ImGui::Spacing();
    }

    ImGui::Checkbox("skybox", &scene_renderer->settings.draw_skybox);
    ImGui::Checkbox("grid", &scene_renderer->settings.draw_grid);
    ImGui::Checkbox("disable material", &scene_renderer->settings.disable_material);
    ImGui::Checkbox("fxaa", &scene_renderer->settings.use_fxaa);
    ImGui::Checkbox("bloom", &scene_renderer->settings.use_bloom);

    auto pbr_renderer = std::dynamic_pointer_cast<vierkant::PBRDeferred>(scene_renderer);

    if(pbr_renderer)
    {
        auto extent = pbr_renderer->lighting_buffer().extent();

        if(ImGui::TreeNode("g-buffer", "g-buffer (%d)", vierkant::PBRDeferred::G_BUFFER_SIZE))
        {
            std::vector<vierkant::ImagePtr> images(vierkant::PBRDeferred::G_BUFFER_SIZE);

            for(uint32_t i = 0; i < vierkant::PBRDeferred::G_BUFFER_SIZE; ++i)
            {
                images[i] = pbr_renderer->g_buffer().color_attachment(i);
            }
            vierkant::gui::draw_images_ui(images);

            ImGui::TreePop();
        }
        if(ImGui::TreeNode("lighting buffer", "lighting buffer (%d x %d)", extent.width, extent.height))
        {
            vierkant::gui::draw_images_ui({pbr_renderer->lighting_buffer().color_attachment()});

            ImGui::TreePop();
        }
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
    bool is_enabled = obj->enabled();
    if(ImGui::Checkbox("", &is_enabled)){ obj->set_enabled(is_enabled); }
    ImGui::SameLine();

    if(!is_enabled){ ImGui::PushStyleColor(ImGuiCol_Text, gray); }

    if(obj->children().empty())
    {
        node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen; // ImGuiTreeNodeFlags_Bullet
        ImGui::TreeNodeEx((void *) (uintptr_t) obj->id(), node_flags, "%s", obj->name().c_str());
        if(ImGui::IsItemClicked()){ ret = obj; }
    }
    else
    {
        bool is_open = ImGui::TreeNodeEx((void *) (uintptr_t) obj->id(), node_flags, "%s",
                                         obj->name().c_str());
        if(ImGui::IsItemClicked()){ ret = obj; }

        if(is_open)
        {
            for(auto &c : obj->children())
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
        // draw a tree for the scene-objects
        auto clicked_obj = draw_scenegraph_ui_helper(scene->root(), selection);

        // add / remove an object from selection
        if(clicked_obj && selection)
        {
            if(ImGui::GetIO().KeyCtrl)
            {
                if(selection->count(clicked_obj)){ selection->erase(clicked_obj); }
                else{ selection->insert(clicked_obj); }
            }
            else
            {
                selection->clear();
                selection->insert(clicked_obj);
            }
        }

        ImGui::Separator();

        if(selection){ for(auto &obj : *selection){ draw_object_ui(obj, camera); }}
    }

    // end window
    if(is_child_window){ ImGui::EndChild(); }
    else{ ImGui::End(); }
}

void draw_material_ui(const MaterialPtr &material)
{
    const float w = ImGui::GetContentRegionAvailWidth();

    // base color
    if(material->textures.count(vierkant::Material::TextureType::Color))
    {
        auto img = material->textures[vierkant::Material::TextureType::Color];
        ImVec2 sz(w, w / (img->width() / (float) img->height()));
        ImGui::BulletText("base color (%d x %d)", img->width(), img->height());
        ImGui::Image((ImTextureID) (img.get()), sz);
        ImGui::Separator();
    }
    else{ ImGui::ColorEdit4("base color", glm::value_ptr(material->color)); }

    // emissive color
    if(material->textures.count(vierkant::Material::TextureType::Emission))
    {
        auto img = material->textures[vierkant::Material::TextureType::Emission];
        ImVec2 sz(w, w / (img->width() / (float) img->height()));
        ImGui::BulletText("emission color (%d x %d)", img->width(), img->height());
        ImGui::Image((ImTextureID) (img.get()), sz);
        ImGui::Separator();
    }
    else{ ImGui::ColorEdit4("emission color", glm::value_ptr(material->emission)); }

    // normalmap
    if(material->textures.count(vierkant::Material::TextureType::Normal))
    {
        auto img = material->textures[vierkant::Material::TextureType::Normal];
        ImVec2 sz(w, w / (img->width() / (float) img->height()));
        ImGui::BulletText("normals (%d x %d)", img->width(), img->height());
        ImGui::Image((ImTextureID) (img.get()), sz);
        ImGui::Separator();
    }

    // ambient-occlusion / roughness / metalness
    if(material->textures.count(vierkant::Material::TextureType::Ao_rough_metal))
    {
        auto img = material->textures[vierkant::Material::TextureType::Ao_rough_metal];
        ImVec2 sz(w, w / (img->width() / (float) img->height()));
        ImGui::BulletText("ambient-occlusion / roughness / metalness (%d x %d)", img->width(), img->height());
        ImGui::Image((ImTextureID) (img.get()), sz);
        ImGui::Separator();
    }
    else
    {
        // roughness
        ImGui::SliderFloat("roughness", &material->roughness, 0.f, 1.f);

        // metalness
        ImGui::SliderFloat("metalness", &material->metalness, 0.f, 1.f);

        // ambient occlusion
        ImGui::SliderFloat("ambient occlusion", &material->ambient, 0.f, 1.f);
    }

    // two-sided
    ImGui::Checkbox("two-sided", &material->two_sided);

    // blending
    ImGui::Checkbox("blending", &material->blending);
}

void draw_mesh_ui(const vierkant::MeshNodePtr &node)
{
    if(!node || !node->mesh){ return; }

    auto mesh = node->mesh;

    size_t num_vertices = 0, num_faces = 0;

    for(auto &e : mesh->entries)
    {
        num_vertices += e.num_vertices;
        num_faces += e.num_indices / 3;
    }
    ImGui::Separator();
    ImGui::BulletText("%d vertices", num_vertices);
    ImGui::BulletText("%d faces", num_faces);
    ImGui::BulletText("%d bones", vierkant::nodes::num_nodes_in_hierarchy(mesh->root_bone));
    ImGui::Separator();
    ImGui::Spacing();

    // entries
    if(!mesh->entries.empty() && ImGui::TreeNode("entries", "entries (%d)", mesh->entries.size()))
    {
        size_t index = 0;
        std::hash<vierkant::MeshPtr> hash;

        for(auto &e : mesh->entries)
        {
            int mesh_id = hash(mesh);

            // push object id
            ImGui::PushID(mesh_id + index);
            ImGui::Checkbox("", &e.enabled);
            ImGui::SameLine();

            const ImVec4 gray(.6, .6, .6, 1.);
            if(!e.enabled){ ImGui::PushStyleColor(ImGuiCol_Text, gray); }

            if(ImGui::TreeNodeEx((void *) (mesh_id + index), 0, "entry %d", index))
            {
                std::stringstream ss;
                ss << "vertices: " << std::to_string(e.num_vertices) << "\n";
                ss << "faces: " << std::to_string(e.num_indices / 3) << "\n";
                ss << "material index: " << std::to_string(e.material_index);

                ImGui::Text("%s", ss.str().c_str());
                ImGui::Separator();

                // material ui
                draw_material_ui(node->mesh->materials[e.material_index]);

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
    if(!mesh->entries.empty() && ImGui::TreeNode("materials", "materials (%d)", mesh->materials.size()))
    {
        for(uint32_t i = 0; i < mesh->materials.size(); ++i)
        {
            auto &mat = mesh->materials[i];

            if(mat && ImGui::TreeNode((void *) (mat.get()), "material %d", i))
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
    if(!mesh->node_animations.empty() && ImGui::TreeNode("animation"))
    {
        auto &animation = mesh->node_animations[mesh->animation_index];

        // animation index
        int animation_index = mesh->animation_index;

        if(ImGui::SliderInt("index", &animation_index, 0, static_cast<int>(mesh->node_animations.size() - 1)))
        {
            mesh->animation_index = animation_index;
        }

        // animation speed
        if(ImGui::SliderFloat("speed", &mesh->animation_speed, -3.f, 3.f)){}
        ImGui::SameLine();
        if(ImGui::Checkbox("play", &animation.playing)){}

        float current_time = animation.current_time / animation.ticks_per_sec;
        float duration = animation.duration / animation.ticks_per_sec;

        // animation current time / max time
        if(ImGui::SliderFloat(("/ " + crocore::to_string(duration, 2) + " s").c_str(),
                              &current_time, 0.f, duration))
        {
            animation.current_time = current_time * animation.ticks_per_sec;
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
    size_t buf_size = object->name().size() + 4;
    char text_buf[buf_size];
    strcpy(text_buf, object->name().c_str());

    if(ImGui::InputText("name", text_buf, IM_ARRAYSIZE(text_buf), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        object->set_name(text_buf);
    }

    ImGui::Separator();

    // transform
    if(object && ImGui::TreeNode("transform"))
    {
        glm::mat4 transform = object->transform();
        glm::vec3 position = transform[3].xyz();
        glm::vec3 rotation = glm::degrees(glm::eulerAngles(glm::quat_cast(transform)));
        glm::vec3 scale = glm::vec3(length(transform[0]), length(transform[1]), length(transform[2]));

        bool changed = ImGui::InputFloat3("position", glm::value_ptr(position), 3);
        changed = ImGui::InputFloat3("rotation", glm::value_ptr(rotation), 3) || changed;
        changed = ImGui::InputFloat3("scale", glm::value_ptr(scale), 3) || changed;

        if(changed)
        {
            auto m = glm::mat4_cast(glm::quat(glm::radians(rotation)));
            m[3] = glm::vec4(position, 1.f);
            m = glm::scale(m, scale);
            object->set_transform(m);
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
    auto mesh = std::dynamic_pointer_cast<vierkant::MeshNode>(object);
    if(mesh){ draw_mesh_ui(mesh); }

    if(is_child_window){ ImGui::EndChild(); }
    else{ ImGui::End(); }

    // imguizmo drawing is in fact another window
    if(draw_guizmo && camera && (current_gizmo != gizmo_inactive))
    {
        glm::mat4 transform = object->global_transform();
        bool is_ortho = std::dynamic_pointer_cast<const vierkant::OrthoCamera>(camera).get();
        auto z_val = transform[3].z;

        ImGuizmo::SetOrthographic(is_ortho);
        auto proj = camera->projection_matrix();
        proj[1][1] *= -1;
        ImGuizmo::Manipulate(glm::value_ptr(camera->view_matrix()), glm::value_ptr(proj),
                             ImGuizmo::OPERATION(current_gizmo), ImGuizmo::WORLD, glm::value_ptr(transform));
        if(is_ortho){ transform[3].z = z_val; }
        object->set_global_transform(transform);
    }
}

}// namespace vierkant::gui

