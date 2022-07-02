//
// Created by crocdialer on 2/28/19.
//

#pragma once

#include "vierkant/Device.hpp"
#include "vierkant/Buffer.hpp"
#include "vierkant/Image.hpp"
#include "vierkant/Geometry.hpp"
#include "vierkant/Material.hpp"
#include <vierkant/vertex_attrib.hpp>
#include <vierkant/intersection.hpp>

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

///////////////////////////////////////////////////////////////////////////////////////////////////

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
        ATTRIB_MAX
    };

    struct entry_t
    {
        std::string name;
        glm::mat4 transform = glm::mat4(1);
        vierkant::AABB bounding_box;
        vierkant::Sphere bounding_sphere;
        uint32_t node_index = 0;

        int32_t vertex_offset = 0;
        uint32_t num_vertices = 0;
        uint32_t base_index = 0;
        uint32_t num_indices = 0;
        uint32_t meshlet_offset = 0;
        uint32_t num_meshlets = 0;
        uint32_t material_index = 0;
        VkPrimitiveTopology primitive_type = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        std::vector<float> morph_weights;

        bool enabled = true;
    };

    struct alignas(16) meshlet_t
    {
        //! offsets within meshlet_vertices and meshlet_triangles
        uint32_t vertex_offset = 0;
        uint32_t triangle_offset = 0;

        //! number of vertices and triangles used in the meshlet
        uint32_t vertex_count = 0;
        uint32_t triangle_count = 0;

        //! bounding sphere, useful for frustum and occlusion cluster-culling
        vierkant::Sphere bounding_sphere;
    };

    struct create_info_t
    {
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        vierkant::BufferPtr staging_buffer = nullptr;
        VkBufferUsageFlags buffer_usage_flags = 0;
        bool optimize_vertex_cache = false;
        bool generate_meshlets = false;
        bool use_vertex_colors = true;
    };

    struct entry_create_info_t
    {
        std::string name;
        GeometryPtr geometry = nullptr;
        std::vector<GeometryPtr> morph_targets;
        glm::mat4 transform = glm::mat4(1);
        uint32_t node_index = 0;
        uint32_t material_index = 0;
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

    Mesh(const Mesh &) = delete;

    Mesh(Mesh &&) = delete;

    Mesh &operator=(Mesh other) = delete;

    /**
     * @brief   bind vertex- and index-buffers for the provided vierkant::Mesh
     *
     * @param   command_buffer  handle to an VkCommandBuffer to record the bind-operation into
     */
    void bind_buffers(VkCommandBuffer command_buffer) const;

    /**
     * @brief   Create an array of VkVertexInputAttributeDescription for a given vierkant::Mesh
     *
     * @return  the newly created array of VkVertexInputAttributeDescriptions
     */
    [[nodiscard]] std::vector<VkVertexInputAttributeDescription> attribute_descriptions() const;

    /**
     * @brief   Create an array of VkVertexInputBindingDescription for a given vierkant::Mesh
     *
     * @return  the newly created array of VkVertexInputBindingDescriptions
     */
    [[nodiscard]] std::vector<VkVertexInputBindingDescription> binding_descriptions() const;

    /**
     * @brief   update_entry_transforms applies the current animation to all entry-transforms.
     *          no-op if no animations are defined.
     */
    void update_entry_transforms();

    // vertex attributes
    vertex_attrib_map_t vertex_attribs;

    // entries for sub-meshes
    std::vector<entry_t> entries;

    // materials for submeshes
    std::vector<vierkant::MaterialPtr> materials;

    // animations general
    uint32_t animation_index = 0;
    float animation_speed = 1.f;

    // node animations
    vierkant::nodes::NodePtr root_node, root_bone;
    std::vector<vierkant::nodes::node_animation_t> node_animations;

    // vertex buffer
    vierkant::BufferPtr vertex_buffer;

    // index buffer
    vierkant::BufferPtr index_buffer;
    VkDeviceSize index_buffer_offset = 0;
    VkIndexType index_type = VK_INDEX_TYPE_UINT32;

    //! morph-targets
    std::vector<vierkant::BufferPtr> morph_target_buffers;

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

    //! combined meshlet-buffer
    std::vector<Mesh::meshlet_t> meshlets;

    //! indices into vertex-buffer, referenced my meshlets
    std::vector<index_t> meshlet_vertices;

    //! micro-indices into meshlet_vertices, referenced my meshlets
    std::vector<uint8_t> meshlet_triangles;
};

/**
 * @brief   create_combined_buffers 'can' be used to create combined and interleaved vertex/index/meshlet-buffers
 *          for a list of geometries. helpful during GPU-mesh/buffer creation.
 *
 * @param   entry_create_infos      an array of entry_create_info_t structs.
 * @param   optimize_vertex_cache   flag indicating if the vertex/index-order should be optimized
 * @param   generate_meshlets       flag indicating if meshlet/cluster information shall be generated.
 * @param   use_vertex_colors       flag indicating if oldschoold vertex-colors shall be respected.
 * @return  a intermediate mesh_buffer_bundle_t.
 */
mesh_buffer_bundle_t create_combined_buffers(const std::vector<Mesh::entry_create_info_t> &entry_create_infos,
                                             bool optimize_vertex_cache,
                                             bool generate_meshlets,
                                             bool use_vertex_colors);

}//namespace vierkant
