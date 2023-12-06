//
// Created by crocdialer on 2/28/19.
//

#pragma once

#include <optional>
#include <set>
#include "vierkant/Device.hpp"
#include "vierkant/Buffer.hpp"
#include "vierkant/Image.hpp"
#include "vierkant/Geometry.hpp"
#include "vierkant/Material.hpp"
#include <vierkant/vertex_attrib.hpp>
#include <vierkant/transform.hpp>
#include <vierkant/intersection.hpp>
#include <vierkant/object_component.hpp>

namespace vierkant
{

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief       for a given datatype, retrieve the corresponding VkIndexType.
 *              currently defined:
 *              index_type<uint16_t>() -> VK_INDEX_TYPE_UINT16
 *              index_type<uint32_t>() -> VK_INDEX_TYPE_UINT32
 *
 * @tparam  T   template parameter providing the datatype
 * @return      the VkIndexType for T
 */
template<typename T>
VkIndexType index_type();

/**
 * @brief       for a given datatype, retrieve the corresponding VkFormat.
 *              e.g.:
 *              format<float>() -> VK_FORMAT_R32_SFLOAT
 *              format<glm::vec4>() -> VK_FORMAT_R32G32B32A32_SFLOAT
 *
 * @tparam T    template parameter providing the datatype
 * @return      the VkFormat for T
 */
template<typename T>
VkFormat format();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct mesh_buffer_params_t
{
    //! flag indicating if the index-buffer should be re-mapped, avoiding duplicate vertices
    bool remap_indices = false;

    //! flag indicating if the vertex/index-order should be optimized for vertex-cache utilization
    bool optimize_vertex_cache = false;

    //! flag indicating if a cascade of simplified meshes (LODs) shall be generated.
    bool generate_lods = false;

    //! maximum number of lods to be generated
    uint32_t max_num_lods = 7;

    //! flag indicating if meshlet/cluster information shall be generated.
    bool generate_meshlets = false;

    //! flag indicating if vertex-colors shall be used, if existing.
    bool use_vertex_colors = false;

    //! flag indicating if a packed vertex-layout should be used
    bool pack_vertices = false;

    //! maximum number of vertices per meshlet
    size_t meshlet_max_vertices = 64;

    //! maximum number of triangles per meshlet
    size_t meshlet_max_triangles = 64;

    //! cone-weight used during meshlet-generation. useful for cluster-culling
    float meshlet_cone_weight = 0.5f;

    bool operator==(const mesh_buffer_params_t &other) const;
    inline bool operator!=(const mesh_buffer_params_t &other) const { return !(*this == other); };
};

//! mesh_buffer_bundle_t is a helper-struct to group buffer-data and other information.
struct mesh_buffer_bundle_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_CLASS_PTR(Mesh)

/**
 * @brief   Mesh groups all sorts of resources,
 *          required to feed vertex-data into a (graphics-)pipeline.
 */
class Mesh
{
public:

    enum AttribLocation : uint32_t
    {
        ATTRIB_POSITION = 0,
        ATTRIB_COLOR = 1,
        ATTRIB_TEX_COORD = 2,
        ATTRIB_NORMAL = 3,
        ATTRIB_TANGENT = 4,
        ATTRIB_BONE_INDICES = 5,
        ATTRIB_BONE_WEIGHTS = 6,
        ATTRIB_MAX [[maybe_unused]]
    };

    struct create_info_t
    {
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        vierkant::BufferPtr staging_buffer = nullptr;
        VkBufferUsageFlags buffer_usage_flags = 0;
        mesh_buffer_params_t mesh_buffer_params = {};
    };

    struct entry_create_info_t
    {
        std::string name;
        GeometryPtr geometry = nullptr;
        vierkant::transform_t transform = {};
        uint32_t node_index = 0;
        uint32_t material_index = 0;
        std::vector<GeometryPtr> morph_targets;
        std::vector<double> morph_weights;
    };

    struct lod_t
    {
        uint32_t base_index = 0;
        uint32_t num_indices = 0;
        uint32_t base_meshlet = 0;
        uint32_t num_meshlets = 0;
    };

    struct entry_t
    {
        std::string name;
        vierkant::transform_t transform = {};
        vierkant::AABB bounding_box;
        vierkant::Sphere bounding_sphere;
        uint32_t node_index = 0;

        int32_t vertex_offset = 0;
        uint32_t num_vertices = 0;

        std::vector<lod_t> lods;

        uint32_t material_index = 0;
        VkPrimitiveTopology primitive_type = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        uint32_t morph_vertex_offset = 0;
        std::vector<double> morph_weights;
    };

    struct alignas(16) meshlet_t
    {
        //! offsets within meshlet_vertices and meshlet_triangles
        uint32_t vertex_offset = 0;
        uint32_t triangle_offset = 0;

        //! number of vertices and triangles used in the meshlet
        uint32_t vertex_count = 0;
        uint32_t triangle_count = 0;

        //! bounding sphere, useful for frustum and occlusion culling
        vierkant::Sphere bounding_sphere;

        //! normal cone, useful for backface culling
        vierkant::Cone normal_cone;
    };

    static MeshPtr create();

    /**
     * @brief   Create a vierkant::MeshPtr from a provided Geometry.
     *          Will copy all available vertex-data into a single vertex buffer and create appropriate VertexAttribs for it.
     *
     * @param   device      handle for the vierkant::Device to create subresources with
     * @param   geometry    a Geometry struct to extract the vertex information from
     * @return  the newly created vierkant::MeshPtr
     */
    static vierkant::MeshPtr
    create_from_geometry(const vierkant::DevicePtr &device,
                         const GeometryPtr &geometry,
                         const create_info_t &create_info);

    /**
     * @brief   Create a vierkant::MeshPtr with provided information about entries.
     *          Will copy all available vertex-data into a single vertex buffer and create appropriate VertexAttribs for it.
     *
     * @param   device              handle for the vierkant::Device to create subresources with
     * @param   entry_create_infos  an array of entry_create_info_t structs.
     * @return  the newly created vierkant::MeshPtr
     */
    static vierkant::MeshPtr
    create_with_entries(const vierkant::DevicePtr &device,
                        const std::vector<entry_create_info_t> &entry_create_infos,
                        const create_info_t &create_info);

    /**
     * @brief   Create a vierkant::MeshPtr from a provided vierkant::mesh_buffer_bundle_t.
     *          Will copy all available vertex-data into a single vertex buffer and create appropriate VertexAttribs for it.
     *
     * @param   device      handle for the vierkant::Device to create subresources with
     * @param   geometry    a Geometry struct to extract the vertex information from
     * @return  the newly created vierkant::MeshPtr
     */
    static vierkant::MeshPtr create_from_bundle(const vierkant::DevicePtr &device,
                                                const vierkant::mesh_buffer_bundle_t &mesh_buffer_bundle,
                                                const create_info_t &create_info);

    Mesh(const Mesh &) = delete;

    Mesh(Mesh &&) = delete;

    Mesh &operator=(Mesh other) = delete;

    /**
     * @brief   bind vertex- and index-buffers for the provided vierkant::Mesh
     *
     * @param   command_buffer  handle to an VkCommandBuffer to record the bind-operation into
     */
    void bind_buffers(VkCommandBuffer command_buffer) const;

    //! vertex attributes
    vertex_attrib_map_t vertex_attribs;

    //! entries for sub-meshes
    std::vector<entry_t> entries;

    //! materials for submeshes
    std::vector<vierkant::MaterialPtr> materials;

    //! node animations
    vierkant::nodes::NodePtr root_node, root_bone;
    std::vector<vierkant::nodes::node_animation_t> node_animations;

    //! vertex buffer
    vierkant::BufferPtr vertex_buffer;

    //! bone-vertex buffer
    vierkant::BufferPtr bone_vertex_buffer;

    //! index buffer
    vierkant::BufferPtr index_buffer;
    VkDeviceSize index_buffer_offset = 0;
    VkIndexType index_type = VK_INDEX_TYPE_UINT32;

    //! morph-targets
    vierkant::BufferPtr morph_buffer;

    //! meshlet-buffer
    vierkant::BufferPtr meshlets;

    //! indices into vertex-buffer
    vierkant::BufferPtr meshlet_vertices;

    //! micro-indices into meshlet_vertices
    vierkant::BufferPtr meshlet_triangles;

private:

    Mesh() = default;
};

//! mesh_buffer_bundle_t is a helper-struct to group buffer-data and other information.
struct mesh_buffer_bundle_t
{
    //! vertex-stride in bytes
    uint32_t vertex_stride = 0;

    //! vertex attributes present in vertex-buffer
    vertex_attrib_map_t vertex_attribs;

    //! entries for sub-meshes/buffers
    std::vector<Mesh::entry_t> entries;

    //! total number of materials referenced by entries
    uint32_t num_materials = 0;

    //! combined array of vertices (vertex-footprint varies hence encoded as raw-bytes)
    std::vector<uint8_t> vertex_buffer;

    //! combined array of indices
    std::vector<index_t> index_buffer;

    //! combined array of bone vertex-data (bone_vertex_data_t)
    std::vector<uint8_t> bone_vertex_buffer;

    //! combined array of vertex-displacements (vertex-displacement-footprint varies hence encoded as raw-bytes)
    std::vector<uint8_t> morph_buffer;
    uint32_t num_morph_targets = 0;

    //! combined meshlet-buffer
    std::vector<Mesh::meshlet_t> meshlets;

    //! indices into vertex-buffer, referenced my meshlets
    std::vector<index_t> meshlet_vertices;

    //! micro-indices into meshlet_vertices, referenced my meshlets
    std::vector<uint8_t> meshlet_triangles;
};

/**
 * @brief   create_mesh_buffers 'can' be used to create combined and interleaved vertex/index/meshlet-buffers
 *          for a list of geometries. helpful during GPU-mesh/buffer creation.
 *
 * @param   entry_create_infos      an array of entry_create_info_t structs.
 * @param   params                  a struct grouping parameters.
 * @return  a intermediate mesh_buffer_bundle_t.
 */
mesh_buffer_bundle_t create_mesh_buffers(const std::vector<Mesh::entry_create_info_t> &entry_create_infos,
                                         const mesh_buffer_params_t &params);

struct animated_mesh_t
{
    vierkant::MeshConstPtr mesh = {};
    vierkant::animation_component_t animation_state = {};

    inline bool operator==(const animated_mesh_t &k) const
    {
        return mesh == k.mesh && animation_state == k.animation_state;
    }
};

struct mesh_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();

    //! handle to a mesh, containing buffers and a list of entries
    vierkant::MeshConstPtr mesh;

    //! optional set of used entry-indices.
    std::optional<std::set<uint32_t>> entry_indices = {};
};

//! helper struct to group an entity/id with a sub-entry-index
struct id_entry_t
{
    uint32_t id;
    uint32_t entry;

    inline bool operator==(const id_entry_t &other) const
    {
        return id == other.id && entry == other.entry;
    }
};

}//namespace vierkant

// template specializations for hashing
namespace std
{
template<>
struct hash<vierkant::mesh_buffer_params_t>
{
    size_t operator()(vierkant::mesh_buffer_params_t const &params) const;
};

template<>
struct hash<vierkant::animated_mesh_t>
{
    size_t operator()(vierkant::animated_mesh_t const &key) const;
};

template<>
struct hash<vierkant::id_entry_t>
{
    size_t operator()(vierkant::id_entry_t const &key) const;
};

}