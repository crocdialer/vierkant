#pragma once

#include <vierkant/Camera.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/Scene.hpp>
#include <vierkant/animation.hpp>
#include <vierkant/model/model_loading.hpp>
#include <vierkant/physics_context.hpp>
#include <vierkant/punctual_light.hpp>

// TODO: this should simply be vierkant::mesh_component, once that uses IDs
struct mesh_state_t
{
    vierkant::MeshId mesh_id = vierkant::MeshId::nil();
    std::optional<std::unordered_set<uint32_t>> entry_indices = {};
    std::optional<std::vector<vierkant::MaterialId>> material_ids = {};
    bool mesh_library = false;
};

struct scene_node_t
{
    //! a descriptive name
    std::string name;

    //! indicating if node is enabled
    bool enabled = true;

    //! optional rigid transformation. absent means identity, and stays absent through a round-trip.
    std::optional<vierkant::transform_t> transform = {};

    //! bitmask of vierkant::transform_component_t::space_flags_t, marking world-space channels
    uint8_t transform_space = 0;

    //! list of child-nodes (indices into scene_data_t::nodes)
    std::vector<uint32_t> children = {};

    //! optional sub-scene-id.
    std::optional<vierkant::SceneId> scene_id;

    //! optional mesh-state
    std::optional<mesh_state_t> mesh_state;

    //! optional animation-state
    std::optional<vierkant::animation_component_t> animation_state = {};

    //! optional physics-state
    std::optional<vierkant::physics_component_t> physics_state = {};

    //! optional physics-constraints
    std::optional<vierkant::constraint_component_t> constraints = {};

    //! optional camera-state
    std::optional<vierkant::camera_component_t> camera_state = {};

    //! optional lightsource-state
    std::optional<vierkant::lightsource_component_t> light_state = {};
};

struct scene_data_t
{
    //! descriptive name for the scene
    std::string name;

    //! map of sub-scenes (.json)
    std::unordered_map<vierkant::SceneId, std::string> scene_paths;

    //! array of file-paths, containing model-files (.gltf, .glb, .obj)
    std::unordered_map<vierkant::MeshId, std::string> model_paths;

    //! optional filepath for a material-bundle (.4km)
    std::string material_bundle_path;

    //! lightsource-assets, referenced by scene_node_t::light_state
    std::unordered_map<vierkant::LightId, vierkant::lightsource_t> lights;

    //! material-assets, referenced by mesh material_ids
    std::unordered_map<vierkant::MaterialId, vierkant::material_t> materials;

    //! sampler-descriptions, referenced by material_t::texture_data
    std::unordered_map<vierkant::SamplerId, vierkant::texture_sampler_t> texture_samplers;

    //! optional filepath for an equirectangular HDR environment
    std::string environment_path;

    //! factor multiplied with environment-light
    float environment_factor = 1.f;

    std::vector<scene_node_t> nodes;

    //! indices into scene_data_t::nodes
    std::vector<uint32_t> scene_roots;

    //! optional index into nodes: the camera the scene is viewed through.
    //! absent means the application's own viewport-camera, which is not part of the scene.
    std::optional<uint32_t> active_camera;
};