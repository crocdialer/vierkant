#pragma once

#include <unordered_map>
#include <unordered_set>

#include <vierkant/Device.hpp>
#include <vierkant/Image.hpp>
#include <vierkant/Material.hpp>
#include <vierkant/mesh_component.hpp>
#include <vierkant/model/model_loading.hpp>

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
};

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
    static AssetProviderPtr create(vierkant::DevicePtr device);

    // materials
    void add_material(material_t m);
    [[nodiscard]] const material_t *material(const MaterialId &id) const;
    material_t *material(const MaterialId &id);

    // textures (GPU), keyed by {texture_id, sampler_id}
    void add_texture(const texture_key_t &key, ImagePtr img);
    [[nodiscard]] ImagePtr texture(const texture_key_t &key) const;

    // samplers (GPU) - owner + dedup by SamplerId
    [[nodiscard]] VkSamplerPtr sampler(const SamplerId &id) const;
    VkSamplerPtr get_or_create_sampler(const SamplerId &id, const texture_sampler_t &ts, uint32_t mip_levels);

    // meshes
    [[nodiscard]] const mesh_asset_t *mesh_asset(const MeshId &id) const;
    void add_mesh(const MeshId &id, mesh_asset_t asset);

    //! bulk-merge a finished load (render-thread; see W2 #6)
    void populate(const model::load_mesh_result_t &result);

    //! mark-sweep (render-thread); reaps materials + textures + samplers + meshes
    void prune(const asset_live_set_t &live);

    // borrowed whole-map access for cull/imgui (render-thread only)
    [[nodiscard]] const std::unordered_map<MaterialId, material_t> &materials() const { return m_materials; }
    [[nodiscard]] const std::unordered_map<texture_key_t, ImagePtr> &textures() const { return m_textures; }

    //! adapter satisfying physics' mesh_provider_fn
    [[nodiscard]] std::function<const mesh_asset_t *(MeshId)> mesh_provider() const;

private:
    explicit AssetProvider(vierkant::DevicePtr device) : m_device(std::move(device)) {}

    vierkant::DevicePtr m_device;
    std::unordered_map<MaterialId, material_t> m_materials;
    std::unordered_map<texture_key_t, ImagePtr> m_textures;
    std::unordered_map<SamplerId, VkSamplerPtr> m_samplers;
    mesh_map_t m_meshes;
};

}// namespace vierkant
