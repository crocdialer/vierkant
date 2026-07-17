#pragma once

#include <entt/entity/registry.hpp>

#include <crocore/ThreadPool.hpp>

#include <vierkant/AssetProvider.hpp>
#include <vierkant/Image.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/Object3D.hpp>
#include <vierkant/mesh_component.hpp>
#include <vierkant/model/model_loading.hpp>
#include <vierkant/punctual_light.hpp>

namespace vierkant
{

DEFINE_NAMED_UUID(SceneId)
DEFINE_CLASS_PTR(Scene)

//! define an object-component, used to identify sub-scenes
struct subscene_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();
    vierkant::SceneId scene_id = vierkant::SceneId::nil();
};

class Scene
{
public:
    virtual ~Scene() = default;

    static ScenePtr create(const std::shared_ptr<vierkant::ObjectStore> &object_store = {},
                           const vierkant::AssetProviderPtr &asset_provider = {});

    virtual void add_object(const Object3DPtr &object);

    virtual void remove_object(const Object3DPtr &object);

    virtual void clear();

    virtual void update(double time_delta);

    [[nodiscard]] uint64_t current_frame() const { return m_current_frame; }

    /**
     * @brief   object_by_id finds and returns an object based on its object/entity-id
     *
     * @param   object_id   a provided object-id
     * @return  an object or nullptr, if nothing was found
     */
    [[nodiscard]] Object3D *object_by_id(uint32_t object_id) const;

    [[nodiscard]] std::vector<Object3D *> objects_by_name(const std::string_view &name) const;

    [[nodiscard]] Object3D *any_object_by_name(const std::string_view &name) const;

    [[nodiscard]] Object3DPtr pick(const Ray &ray) const;

    [[nodiscard]] inline const Object3DPtr &root() const { return m_root; };

    [[nodiscard]] const vierkant::ImagePtr &environment() const { return m_skybox; }

    void set_environment(const vierkant::ImagePtr &img);

    [[nodiscard]] inline const std::shared_ptr<entt::registry> &registry() const { return m_object_store->registry(); }

    /**
    * @brief   'create_mesh_object' is a factory to create an Object3D containing a mesh.
    *
    * in addition the created object offers support for animations and dynamically updated aabbs for submeshes.
    *
    * @param    mesh_component  a provided mesh-component
    * @return   a newly created Object3D with attached components.
    */
    [[nodiscard]] vierkant::Object3DPtr create_mesh_object(const vierkant::mesh_component_t &mesh_component) const;

    /**
     * @brief   'create_primitive_object' is a factory to create an Object3D containing a built-in
     *          primitive-mesh, provided by the asset-provider.
     *
     * @param   type    a provided primitive-type
     * @return  a newly created Object3D or nullptr, if the asset-provider has no mesh-factory
     */
    [[nodiscard]] vierkant::Object3DPtr create_primitive_object(vierkant::primitive_type type) const;

    [[nodiscard]] vierkant::Object3DPtr create_object() const;

    [[nodiscard]] vierkant::Object3DPtr
    create_camera(const vierkant::camera_params_variant_t &params = vierkant::physical_camera_params_t{}) const;

    /**
     * @brief   'create_lightsource' registers a lightsource-asset with the asset-provider
     *          and creates an Object3D referencing it.
     *
     * @param   params  lightsource-parameters, registered as asset under params.id
     * @return  a newly created Object3D with attached lightsource-component
     */
    [[nodiscard]] vierkant::Object3DPtr create_lightsource(const vierkant::lightsource_t &params = {}) const;

    [[nodiscard]] const vierkant::AssetProviderPtr &asset_provider() const { return m_asset_provider; }

    /**
     * @brief   prune_assets walks the scene-graph, collects the live material/texture/sampler/mesh/light ids
     *          and hands them to the AssetProvider, which reaps everything else.
     *
     * @param   extra_live_materials    additional material-ids to keep alive regardless of scene-graph
     *                                  references (e.g. a user-authored material-library) - their
     *                                  textures/samplers are kept as well.
     * @param   extra_live_lights       additional light-ids to keep alive regardless of scene-graph references.
     */
    void prune_assets(const std::unordered_set<vierkant::MaterialId> &extra_live_materials = {},
                      const std::unordered_set<vierkant::LightId> &extra_live_lights = {});

protected:
    explicit Scene(const std::shared_ptr<vierkant::ObjectStore> &object_store,
                   const vierkant::AssetProviderPtr &asset_provider);

private:
    static constexpr char s_scene_root_name[] = "scene root";

    std::shared_ptr<vierkant::ObjectStore> m_object_store;

    vierkant::AssetProviderPtr m_asset_provider;

    vierkant::ImagePtr m_skybox = nullptr;

    Object3DPtr m_root;

    uint64_t m_current_frame = 0;

    std::chrono::steady_clock::time_point m_start_time = std::chrono::steady_clock::now();
};

//! helper struct to group an entity/id with a sub-entry-index
struct id_entry_t
{
    uint32_t id;
    uint32_t entry;

    bool operator==(const id_entry_t &other) const { return id == other.id && entry == other.entry; }
};

}// namespace vierkant

namespace std
{
template<>
struct hash<vierkant::id_entry_t>
{
    size_t operator()(vierkant::id_entry_t const &key) const noexcept;
};

}// namespace std
