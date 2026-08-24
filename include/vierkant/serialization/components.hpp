//
// Created by crocdialer on 11/20/20.
//

#pragma once

//! serializers for vierkant types that other people's documents legitimately embed: engine
//! components and renderer/window settings. the asset-bundle format itself is deliberately not
//! here - it is private to vierkant, see src/serialization.
//!
//! this directory is excluded from the install-tree (cereal is not shipped), see scripts/check_install.sh

#include <cereal/cereal.hpp>

#include <vierkant/serialization/glm_cereal.hpp>
#include <vierkant/serialization/optional_nvp_cereal.hpp>

#include <crocore/NamedId.hpp>
#include <crocore/set_lru.hpp>

#include <vierkant/CameraControl.hpp>
#include <vierkant/Material.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/PBRDeferred.hpp>
#include <vierkant/PBRPathTracer.hpp>
#include <vierkant/Window.hpp>
#include <vierkant/camera_params.hpp>
#include <vierkant/punctual_light.hpp>
#include <vierkant/transform.hpp>

namespace crocore
{

template<class Archive, class T>
std::string save_minimal(Archive const &, const crocore::NamedUUID<T> &named_id)
{ return named_id.str(); }

template<class Archive, class T>
void load_minimal(Archive const &, crocore::NamedUUID<T> &named_id, const std::string &uuid_str)
{ named_id = crocore::NamedUUID<T>::from_string(uuid_str); }
template<class Archive, class T>
void serialize(Archive &archive, crocore::set_lru<T> &set_lru)
{
    std::vector<T> array(set_lru.begin(), set_lru.end());
    archive(array);
    set_lru = {array.begin(), array.end()};
}

}// namespace crocore

namespace vierkant
{

template<class Archive, class T>
void serialize(Archive &archive, vierkant::transform_t_<T> &t)
{
    archive(cereal::make_nvp("translation", t.translation), cereal::make_nvp("rotation", t.rotation),
            cereal::make_nvp("scale", t.scale));
}

template<class Archive>
void serialize(Archive &archive, vierkant::texture_data_t &tex_data)
{
    archive(cereal::make_nvp("texture_id", tex_data.texture_id), cereal::make_nvp("sampler_id", tex_data.sampler_id),
            cereal::make_nvp("texture_transform", tex_data.texture_transform));
}

template<class Archive>
void serialize(Archive &archive, vierkant::material_t &material)
{
    // fields use make_optional_nvp so authored scene-JSON tolerates future field add/remove
    // (missing key -> default). the binary model-bundle path reuses this same function; cereal's
    // binary NVP handler matches OptionalNameValuePair via its NameValuePair base and serializes
    // positionally, so field-order (unchanged) is all that matters there.
    archive(cereal::make_nvp("id", material.id), cereal::make_optional_nvp("name", material.name),
            cereal::make_optional_nvp("base_color", material.base_color, glm::vec4(1.f)),
            cereal::make_optional_nvp("emission", material.emission, glm::vec3(0.f)),
            cereal::make_optional_nvp("emissive_strength", material.emissive_strength, 1.f),
            cereal::make_optional_nvp("roughness", material.roughness, 1.f),
            cereal::make_optional_nvp("metalness", material.metalness, 0.f),
            cereal::make_optional_nvp("occlusion", material.occlusion, 1.f),
            cereal::make_optional_nvp("null_surface", material.null_surface, false),
            cereal::make_optional_nvp("twosided", material.twosided, false),
            cereal::make_optional_nvp("ior", material.ior, 1.5f),
            cereal::make_optional_nvp("dispersion", material.dispersion, 0.f),
            cereal::make_optional_nvp("attenuation_color", material.attenuation_color, glm::vec3(1.f)),
            cereal::make_optional_nvp("transmission", material.transmission, 0.f),
            cereal::make_optional_nvp("attenuation_distance", material.attenuation_distance,
                                      std::numeric_limits<float>::infinity()),
            cereal::make_optional_nvp("phase_asymmetry_g", material.phase_asymmetry_g, 0.f),
            cereal::make_optional_nvp("scatter_factor", material.scatter_factor, 0.f),
            cereal::make_optional_nvp("scatter_color", material.scatter_color, glm::vec3(1.f)),
            cereal::make_optional_nvp("diffuse_transmission", material.diffuse_transmission, 0.f),
            cereal::make_optional_nvp("diffuse_transmission_color", material.diffuse_transmission_color,
                                      glm::vec3(1.f)),
            cereal::make_optional_nvp("thickness", material.thickness, 1.f),
            cereal::make_optional_nvp("blend_mode", material.blend_mode, vierkant::BlendMode::Opaque),
            cereal::make_optional_nvp("alpha_cutoff", material.alpha_cutoff, 0.5f),
            cereal::make_optional_nvp("specular_factor", material.specular_factor, 1.f),
            cereal::make_optional_nvp("specular_color", material.specular_color, glm::vec3(1.f)),
            cereal::make_optional_nvp("clearcoat_factor", material.clearcoat_factor, 0.f),
            cereal::make_optional_nvp("clearcoat_roughness_factor", material.clearcoat_roughness_factor, 0.f),
            cereal::make_optional_nvp("sheen_color", material.sheen_color, glm::vec3(0.f)),
            cereal::make_optional_nvp("sheen_roughness", material.sheen_roughness, 0.f),
            cereal::make_optional_nvp("iridescence_factor", material.iridescence_factor, 0.f),
            cereal::make_optional_nvp("iridescence_ior", material.iridescence_ior, 1.3f),
            cereal::make_optional_nvp("iridescence_thickness_range", material.iridescence_thickness_range,
                                      glm::vec2(100.f, 400.f)),
            cereal::make_optional_nvp("texture_data", material.texture_data));
}

template<class Archive>
void serialize(Archive &archive, vierkant::texture_sampler_t &state)
{
    archive(cereal::make_nvp("min_filter", state.min_filter), cereal::make_nvp("mag_filter", state.mag_filter),
            cereal::make_nvp("address_mode_u", state.address_mode_u),
            cereal::make_nvp("address_mode_v", state.address_mode_v), cereal::make_nvp("transform", state.transform));
}

template<class Archive>
void serialize(Archive &archive, vierkant::mesh_buffer_params_t &params)
{
    archive(cereal::make_nvp("remap_indices", params.remap_indices),
            cereal::make_nvp("optimize_vertex_cache", params.optimize_vertex_cache),
            cereal::make_nvp("generate_lods", params.generate_lods),
            cereal::make_nvp("max_num_lods", params.max_num_lods),
            cereal::make_nvp("lod_shrink_factor", params.lod_shrink_factor),
            cereal::make_nvp("lod_target_error", params.lod_target_error),
            cereal::make_nvp("generate_meshlets", params.generate_meshlets),
            cereal::make_nvp("use_vertex_colors", params.use_vertex_colors),
            cereal::make_nvp("pack_vertices", params.pack_vertices),
            cereal::make_nvp("meshlet_max_vertices", params.meshlet_max_vertices),
            cereal::make_nvp("meshlet_max_triangles", params.meshlet_max_triangles),
            cereal::make_nvp("meshlet_cone_weight", params.meshlet_cone_weight));
}

template<class Archive>
void serialize(Archive &archive, vierkant::Window::create_info_t &createInfo)
{
    archive(cereal::make_nvp("size", createInfo.size), cereal::make_nvp("position", createInfo.position),
            cereal::make_nvp("fullscreen", createInfo.fullscreen), cereal::make_nvp("vsync", createInfo.vsync),
            cereal::make_optional_nvp("use_hdr", createInfo.use_hdr),
            cereal::make_optional_nvp("joysticks", createInfo.joysticks, true),
            cereal::make_nvp("monitor_index", createInfo.monitor_index),
            cereal::make_nvp("sample_count", createInfo.sample_count), cereal::make_nvp("title", createInfo.title));
}

template<class Archive>
void serialize(Archive &archive, vierkant::PBRDeferred::settings_t &render_settings)
{
    archive(cereal::make_nvp("resolution", render_settings.resolution),
            cereal::make_nvp("output_resolution", render_settings.output_resolution),
            cereal::make_nvp("disable_material", render_settings.disable_material),
            cereal::make_nvp("debug_draw_flags", render_settings.debug_draw_flags),
            cereal::make_nvp("frustum_culling", render_settings.frustum_culling),
            cereal::make_nvp("occlusion_culling", render_settings.occlusion_culling),
            cereal::make_nvp("enable_lod", render_settings.enable_lod),
            cereal::make_nvp("indirect_draw", render_settings.indirect_draw),
            cereal::make_nvp("use_meshlet_pipeline", render_settings.use_meshlet_pipeline),
            cereal::make_nvp("use_ray_queries", render_settings.use_ray_queries),
            cereal::make_nvp("tesselation", render_settings.tesselation),
            cereal::make_nvp("wireframe", render_settings.wireframe),
            cereal::make_nvp("draw_skybox", render_settings.draw_skybox),
            cereal::make_nvp("use_taa", render_settings.use_taa),
            cereal::make_nvp("use_fxaa", render_settings.use_fxaa),
            cereal::make_nvp("tonemap", render_settings.tonemap),
            cereal::make_nvp("ambient_occlusion", render_settings.ambient_occlusion),
            cereal::make_nvp("max_ao_distance", render_settings.max_ao_distance),
            cereal::make_nvp("bloom", render_settings.bloom), cereal::make_nvp("gamma", render_settings.gamma),
            cereal::make_nvp("exposure", render_settings.exposure),
            cereal::make_nvp("depth_of_field", render_settings.depth_of_field),
            cereal::make_nvp("use_dof_focus_overlay", render_settings.use_dof_focus_overlay));
}

template<class Archive>
void serialize(Archive &archive, vierkant::medium_params_t &medium)
{
    archive(cereal::make_nvp("attenuation_color", medium.attenuation_color),
            cereal::make_nvp("attenuation_distance", medium.attenuation_distance),
            cereal::make_nvp("scatter_factor", medium.scatter_factor),
            cereal::make_nvp("scatter_color", medium.scatter_color),
            cereal::make_nvp("phase_asymmetry_g", medium.phase_asymmetry_g), cereal::make_nvp("ior", medium.ior));
}

template<class Archive>
void serialize(Archive &archive, vierkant::sunlight_params_t &sunlight)
{
    archive(cereal::make_nvp("color", sunlight.color), cereal::make_nvp("intensity", sunlight.intensity),
            cereal::make_nvp("spherical_coords", sunlight.spherical_coords),
            cereal::make_nvp("angular_size", sunlight.angular_size));
}

template<class Archive>
void serialize(Archive &archive, vierkant::PBRPathTracer::settings_t &render_settings)
{
    archive(cereal::make_nvp("resolution", render_settings.resolution),
            cereal::make_nvp("max num batches", render_settings.max_num_batches),
            cereal::make_nvp("num_samples", render_settings.num_samples),
            cereal::make_nvp("max_trace_depth", render_settings.max_trace_depth),
            cereal::make_nvp("max_path_beta", render_settings.max_path_beta),
            cereal::make_nvp("suspend_trace_when_done", render_settings.suspend_trace_when_done),
            cereal::make_nvp("disable_material", render_settings.disable_material),
            cereal::make_nvp("draw_skybox", render_settings.draw_skybox),
            cereal::make_nvp("compaction", render_settings.compaction),
            cereal::make_nvp("use_denoiser", render_settings.denoising),
            cereal::make_nvp("tonemap", render_settings.tonemap), cereal::make_nvp("bloom", render_settings.bloom),
            cereal::make_nvp("gamma", render_settings.gamma), cereal::make_nvp("exposure", render_settings.exposure),
            cereal::make_nvp("depth_of_field", render_settings.depth_of_field),
            cereal::make_optional_nvp("suppress_reset", render_settings.suppress_reset),
            cereal::make_optional_nvp("camera_medium", render_settings.camera_medium),
            cereal::make_optional_nvp("sunlight_params", render_settings.sunlight_params),
            cereal::make_optional_nvp("mis_mode", render_settings.mis_mode),
            cereal::make_optional_nvp("suppress_refractive_caustics", render_settings.suppress_refractive_caustics),
            cereal::make_optional_nvp("max_accumulation_drift", render_settings.max_accumulation_drift));
}

template<class Archive>
void serialize(Archive &archive, vierkant::ortho_camera_params_t &cam)
{
    archive(cereal::make_nvp("left", cam.left), cereal::make_nvp("right", cam.right),
            cereal::make_nvp("bottom", cam.bottom), cereal::make_nvp("top", cam.top),
            cereal::make_nvp("near", cam.near_), cereal::make_nvp("far", cam.far_));
}

template<class Archive>
void serialize(Archive &archive, vierkant::physical_camera_params_t &params)
{
    archive(cereal::make_nvp("focal_length", params.focal_length),
            cereal::make_nvp("sensor_width", params.sensor_width),
            cereal::make_nvp("clipping_distances", params.clipping_distances),
            cereal::make_nvp("focal_distance", params.focal_distance), cereal::make_nvp("fstop", params.fstop));
}

template<class Archive>
void serialize(Archive &archive, vierkant::camera_component_t &c)
{
    archive(cereal::make_nvp("projection", c.projection), cereal::make_nvp("physical", c.physical),
            cereal::make_optional_nvp("ortho", c.ortho));
}

template<class Archive>
void serialize(Archive &archive, vierkant::lightsource_t &light)
{
    archive(cereal::make_nvp("id", light.id), cereal::make_nvp("name", light.name),
            cereal::make_nvp("type", light.type), cereal::make_nvp("color", light.color),
            cereal::make_nvp("intensity", light.intensity), cereal::make_nvp("range", light.range),
            cereal::make_nvp("inner_cone_angle", light.inner_cone_angle),
            cereal::make_nvp("outer_cone_angle", light.outer_cone_angle),
            cereal::make_optional_nvp("size", light.size, glm::vec2(1.f)),
            cereal::make_optional_nvp("cookie", light.cookie));
}

template<class Archive>
void serialize(Archive &archive, vierkant::lightsource_component_t &light_cmp)
{
    archive(cereal::make_nvp("light_id", light_cmp.light_id));
}

template<class Archive>
void serialize(Archive &archive, vierkant::CameraControl &camera_control)
{
    archive(cereal::make_nvp("enabled", camera_control.enabled),
            cereal::make_nvp("mouse_sensitivity", camera_control.mouse_sensitivity),
            cereal::make_optional_nvp("mouse_wheel_sensitivity", camera_control.mouse_wheel_sensitivity,
                                      glm::vec2(1.f)),
            cereal::make_optional_nvp("joystick_sensitivity", camera_control.joystick_sensitivity, glm::vec2(1.f)));
}

template<class Archive>
void serialize(Archive &archive, vierkant::FlyCamera &fly_camera)
{
    archive(cereal::base_class<vierkant::CameraControl>(&fly_camera), cereal::make_nvp("position", fly_camera.position),
            cereal::make_nvp("spherical_coords", fly_camera.spherical_coords),
            cereal::make_nvp("move_speed", fly_camera.move_speed));
}

template<class Archive>
void serialize(Archive &archive, vierkant::OrbitCamera &orbit_camera)
{
    archive(cereal::base_class<vierkant::CameraControl>(&orbit_camera),
            cereal::make_nvp("spherical_coords", orbit_camera.spherical_coords),
            cereal::make_nvp("distance", orbit_camera.distance), cereal::make_nvp("look_at", orbit_camera.look_at));
}
}// namespace vierkant
