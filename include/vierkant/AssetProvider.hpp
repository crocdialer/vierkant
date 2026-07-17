#pragma once

#include <map>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include <vierkant/Device.hpp>
#include <vierkant/Image.hpp>
#include <vierkant/Material.hpp>
#include <vierkant/mesh_component.hpp>
#include <vierkant/model/model_loading.hpp>
#include <vierkant/punctual_light.hpp>

namespace vierkant
{

DEFINE_CLASS_PTR(AssetProvider)

//! live-id set gathered by a Scene graph-walk and handed to AssetProvider::prune
struct asset_live_set_t
{
    std::unordered_set<vierkant::MaterialId> materials;
    std::unordered_set<vierkant::texture_key_t> textures;
    std::unordered_set<vierkant::SamplerId> samplers;
    std::unordered_set<vierkant::MeshId> meshes;
    std::unordered_set<vierkant::LightId> lights;
};

//! enumeration of built-in primitives
enum class primitive_type
{
    PLANE,
    BOX,
    SPHERE,
    CYLINDER,
    CAPSULE
};

//! factory-callback used to create gpu-meshes from geometry, provided by an application
using mesh_factory_fn = std::function<vierkant::MeshPtr(const vierkant::GeometryConstPtr &)>;

/**
 * @brief   AssetProvider owns the runtime (GPU) assets consumed during rendering:
 *          materials, textures (keyed by {texture_id, sampler_id}), samplers and meshes.
 *
 * Mutation (populate/prune) happens on the render-thread at a defined sync-point; reads borrow whole
 * maps by pointer during culling. There is no internal locking - see project notes (W2 #6).
 */
class AssetProvider
{
public:
    static AssetProviderPtr create();

    // materials
    void add_material(material_t m);
    void remove_material(const MaterialId &id);
    [[nodiscard]] const material_t *material(const MaterialId &id) const;
    material_t *material(const MaterialId &id);

    // textures (GPU), keyed by {texture_id, sampler_id}
    void add_texture(const texture_key_t &key, ImagePtr img);
    [[nodiscard]] ImagePtr texture(const texture_key_t &key) const;

    // samplers (GPU) - owned + deduped by SamplerId; created by the model-loader, inserted via populate()
    [[nodiscard]] VkSamplerPtr sampler(const SamplerId &id) const;

    // lights
    void add_light(lightsource_t l);
    void remove_light(const LightId &id);
    [[nodiscard]] const lightsource_t *light(const LightId &id) const;
    lightsource_t *light(const LightId &id);

    // meshes
    [[nodiscard]] const mesh_asset_t *mesh_asset(const MeshId &id) const;
    void add_mesh(const MeshId &id, mesh_asset_t asset);

    // built-in primitive-meshes, created lazily via an application-provided mesh-factory
    void set_mesh_factory(mesh_factory_fn fn);
    [[nodiscard]] bool has_mesh_factory() const;

    /**
     * @brief   'primitive_mesh' returns a built-in primitive-mesh, lazily created via the mesh-factory
     *          and cached (thread-safe). created meshes carry a deterministic, name-based mesh-id
     *          (see 'primitive_names') and reference a default 'primitive_material'.
     *
     * @param   type    a provided primitive-type
     * @return  a mesh or nullptr, if no mesh-factory was provided
     */
    [[nodiscard]] MeshPtr primitive_mesh(primitive_type type);

    //! stable names for built-in primitives ("plane", "cube", ...), also used for serialization
    [[nodiscard]] static const std::map<primitive_type, std::string> &primitive_names();

    //! bulk-merge a finished load (render-thread; see W2 #6)
    void populate(const model::load_mesh_result_t &result);

    //! mark-sweep (render-thread); reaps materials + textures + samplers + meshes
    void prune(const asset_live_set_t &live);

    // borrowed whole-map access for cull/imgui (render-thread only)
    [[nodiscard]] const std::unordered_map<MaterialId, material_t> &materials() const { return m_materials; }
    [[nodiscard]] const std::unordered_map<texture_key_t, ImagePtr> &textures() const { return m_textures; }
    [[nodiscard]] const std::unordered_map<LightId, lightsource_t> &lights() const { return m_lights; }

    //! adapter satisfying physics' mesh_provider_fn
    [[nodiscard]] std::function<const mesh_asset_t *(MeshId)> mesh_provider() const;

private:
    AssetProvider() = default;

    std::unordered_map<MaterialId, material_t> m_materials;
    std::unordered_map<texture_key_t, ImagePtr> m_textures;
    std::unordered_map<SamplerId, VkSamplerPtr> m_samplers;
    std::unordered_map<LightId, lightsource_t> m_lights;
    mesh_map_t m_meshes;

    // primitives are kept separate from m_meshes: lazy creation may happen off the render-thread,
    // while m_meshes follows the render-thread mutation contract above
    mesh_factory_fn m_mesh_factory;
    std::map<primitive_type, MeshPtr> m_primitive_meshes;
    mutable std::mutex m_primitive_mutex;
};

}// namespace vierkant
