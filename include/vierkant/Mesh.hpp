//
// Created by crocdialer on 2/28/19.
//

#pragma once

#include "vierkant/Device.hpp"
#include "vierkant/Buffer.hpp"
#include "vierkant/Image.hpp"
#include "vierkant/Geometry.hpp"
#include "vierkant/Material.hpp"
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

    /**
     * @brief   Mesh::VertexAttrib defines a vertex-attribute available in the vertex-shader-stage.
     */
    struct attrib_t
    {
        vierkant::BufferPtr buffer;
        VkDeviceSize buffer_offset = 0;
        uint32_t offset = 0;
        uint32_t stride = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkVertexInputRate input_rate = VK_VERTEX_INPUT_RATE_VERTEX;
    };

    struct entry_t
    {
        glm::mat4 transform = glm::mat4(1);
        vierkant::AABB boundingbox;
        uint32_t node_index = 0;

        uint32_t base_vertex = 0;
        uint32_t num_vertices = 0;
        uint32_t base_index = 0;
        uint32_t num_indices = 0;
        uint32_t material_index = 0;
        VkPrimitiveTopology primitive_type = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        bool enabled = true;
    };

    struct create_info_t
    {
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        vierkant::BufferPtr staging_buffer = nullptr;
        VkBufferUsageFlags buffer_usage_flags = 0;
    };

    struct entry_create_info_t
    {
        GeometryPtr geometry = nullptr;
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
    create_from_geometry(const vierkant::DevicePtr &device, const GeometryPtr &geometry,
                         const create_info_t& create_info);

    /**
     * @brief   Create a vierkant::MeshPtr with provided information about entries.
     *          Will copy all available vertex-data into a single vertex buffer and create appropriate VertexAttribs for it.
     *
     * @param   device          handle for the vierkant::Device to create subresources with
     * @param   entry_create_infos    an array of entry_create_info_t structs.
     * @return  the newly created vierkant::MeshPtr
     */
    static vierkant::MeshPtr
    create_with_entries(const vierkant::DevicePtr &device, const std::vector<entry_create_info_t> &entry_create_infos,
                        const create_info_t& create_info);

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
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions() const;

    /**
     * @brief   Create an array of VkVertexInputBindingDescription for a given vierkant::Mesh
     *
     * @return  the newly created array of VkVertexInputBindingDescriptions
     */
    std::vector<VkVertexInputBindingDescription> binding_descriptions() const;

    // vertex attributes
    std::map<uint32_t, attrib_t> vertex_attribs;

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

    // index buffer
    vierkant::BufferPtr index_buffer;
    VkDeviceSize index_buffer_offset = 0;
    VkIndexType index_type = VK_INDEX_TYPE_UINT32;

private:

    Mesh() = default;
};

}//namespace vierkant
