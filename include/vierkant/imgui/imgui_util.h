#pragma once

#include <crocore/Component.hpp>
#include <vierkant/Image.hpp>
#include <vierkant/imgui/imgui_integration.h>

class JoystickState;

namespace vierkant::gui{

//! draw a generic kinski::Component using ImGui
void draw_component_ui(const crocore::ComponentConstPtr &the_component);

void draw_textures_ui(const std::vector<vierkant::ImagePtr*> &the_textures);

//void draw_material_ui(const gl::MaterialPtr &the_mat);
//void draw_materials_ui(const std::vector<gl::MaterialPtr> &the_materials);
//
//void draw_light_component_ui(const LightComponentPtr &the_component);
//
//void draw_warp_component_ui(const WarpComponentPtr &the_component);
//
//void draw_object3D_ui(const gl::Object3DPtr &the_object,
//                      const gl::CameraConstPtr &the_camera = nullptr);
//
//void draw_mesh_ui(const gl::MeshPtr &the_mesh);
//
//void draw_scenegraph_ui(const gl::SceneConstPtr &the_scene, std::set<gl::Object3DPtr>* the_selection = nullptr);
//
//void process_joystick_input(const std::vector<JoystickState> &the_joystick_states);

}// namespaces

