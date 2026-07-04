#include <vierkant/AssetProvider.hpp>

namespace vierkant
{

AssetProviderPtr AssetProvider::create() { return AssetProviderPtr(new AssetProvider()); }

void AssetProvider::add_material(material_t m) { m_materials[m.id] = std::move(m); }

void AssetProvider::remove_material(const MaterialId &id) { m_materials.erase(id); }

const material_t *AssetProvider::material(const MaterialId &id) const
{
    auto it = m_materials.find(id);
    return it != m_materials.end() ? &it->second : nullptr;
}

material_t *AssetProvider::material(const MaterialId &id)
{
    auto it = m_materials.find(id);
    return it != m_materials.end() ? &it->second : nullptr;
}

void AssetProvider::add_light(lightsource_t l) { m_lights[l.id] = std::move(l); }

void AssetProvider::remove_light(const LightId &id) { m_lights.erase(id); }

const lightsource_t *AssetProvider::light(const LightId &id) const
{
    auto it = m_lights.find(id);
    return it != m_lights.end() ? &it->second : nullptr;
}

lightsource_t *AssetProvider::light(const LightId &id)
{
    auto it = m_lights.find(id);
    return it != m_lights.end() ? &it->second : nullptr;
}

void AssetProvider::add_texture(const texture_key_t &key, ImagePtr img) { m_textures[key] = std::move(img); }

ImagePtr AssetProvider::texture(const texture_key_t &key) const
{
    auto it = m_textures.find(key);
    return it != m_textures.end() ? it->second : nullptr;
}

VkSamplerPtr AssetProvider::sampler(const SamplerId &id) const
{
    auto it = m_samplers.find(id);
    return it != m_samplers.end() ? it->second : nullptr;
}

const mesh_asset_t *AssetProvider::mesh_asset(const MeshId &id) const
{
    auto it = m_meshes.find(id);
    return it != m_meshes.end() ? &it->second : nullptr;
}

void AssetProvider::add_mesh(const MeshId &id, mesh_asset_t asset) { m_meshes[id] = std::move(asset); }

void AssetProvider::populate(const model::load_mesh_result_t &result)
{
    for(const auto &[id, mat]: result.materials) { m_materials[id] = mat; }
    for(const auto &[key, img]: result.textures) { m_textures[key] = img; }
    for(const auto &[id, vk_sampler]: result.samplers) { m_samplers[id] = vk_sampler; }
    for(const auto &[id, l]: result.lights) { m_lights[id] = l; }

    // attach mesh without a bundle; callers needing the persist-able bundle (physics) add_mesh afterwards
    if(result.mesh) { m_meshes[result.mesh->id] = {.mesh = result.mesh}; }
}

void AssetProvider::prune(const asset_live_set_t &live)
{
    std::erase_if(m_materials, [&live](const auto &p) { return !live.materials.contains(p.first); });
    std::erase_if(m_textures, [&live](const auto &p) { return !live.textures.contains(p.first); });
    std::erase_if(m_samplers, [&live](const auto &p) { return !live.samplers.contains(p.first); });
    std::erase_if(m_meshes, [&live](const auto &p) { return !live.meshes.contains(p.first); });
    std::erase_if(m_lights, [&live](const auto &p) { return !live.lights.contains(p.first); });
}

std::function<const mesh_asset_t *(MeshId)> AssetProvider::mesh_provider() const
{
    return [this](MeshId id) -> const mesh_asset_t * { return mesh_asset(id); };
}

}// namespace vierkant
