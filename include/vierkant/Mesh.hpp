//
// Created by crocdialer on 2/28/19.
//

#pragma once

#include "vierkant/Device.hpp"
#include "vierkant/Buffer.hpp"
#include "vierkant/Image.hpp"
#include "vierkant/Object3D.hpp"
#include "vierkant/Geometry.hpp"

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

using DescriptorPoolPtr = std::shared_ptr<VkDescriptorPool_T>;

using DescriptorSetLayoutPtr = std::shared_ptr<VkDescriptorSetLayout_T>;

using DescriptorSetPtr = std::shared_ptr<VkDescriptorSet_T>;

using descriptor_count_t = std::vector<std::pair<VkDescriptorType, uint32_t>>;

using buffer_binding_set_t = std::set<std::tuple<vierkant::BufferPtr, uint32_t, uint32_t, VkVertexInputRate>>;

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief   descriptor_t defines a resource-descriptor available in a shader program.
 *          it is default constructable, trivially copyable, movable and hashable.
 */
struct descriptor_t
{
    VkDescriptorType type;
    VkShaderStageFlags stage_flags;
    uint32_t binding = 0;
    vierkant::BufferPtr buffer;
    VkDeviceSize buffer_offset = 0;
    std::vector<vierkant::ImagePtr> image_samplers;

    bool operator==(const descriptor_t &other) const;

    bool operator!=(const descriptor_t &other) const{ return !(*this == other); };
};

/**
 * @brief   Extract the types of descriptors and their counts for a given vierkant::Mesh.
 *
 * @param   descriptors an array of descriptors to extract the descriptor counts from
 * @param   counts      a reference to a descriptor_count_map_t to hold the results
 */
void add_descriptor_counts(const std::vector<descriptor_t> &descriptors, descriptor_count_t &counts);

/**
 * @brief   Create a shared VkDescriptorPool (DescriptorPoolPtr)
 *
 * @param   device  handle for the vierkant::Device to create the DescriptorPool
 * @param   counts  a descriptor_count_map_t providing type and count of descriptors
 * @return  the newly constructed DescriptorPoolPtr
 */
DescriptorPoolPtr create_descriptor_pool(const vierkant::DevicePtr &device,
                                         const descriptor_count_t &counts,
                                         uint32_t max_sets);

/**
 * @brief   Create a shared VkDescriptorSetLayout (DescriptorSetLayoutPtr) for a given array of vierkant::descriptor_t
 *
 * @param   device      handle for the vierkant::Device to create the DescriptorSetLayout
 * @param   descriptors an array of descriptor_t to create a layout from
 * @return  the newly created DescriptorSetLayoutPtr
 */
DescriptorSetLayoutPtr
create_descriptor_set_layout(const vierkant::DevicePtr &device, const std::vector<descriptor_t> &descriptors);

/**
 * @brief   Create a shared VkDescriptorSet (DescriptorSetPtr) for a provided DescriptorLayout
 *
 * @param   device  handle for the vierkant::Device to create the DescriptorSet
 * @param   pool    handle for a shared VkDescriptorPool to allocate the DescriptorSet from
 * @param   layout  handle for a shared VkDescriptorSetLayout to use as blueprint
 * @return  the newly created DescriptorSetPtr
 */
DescriptorSetPtr create_descriptor_set(const vierkant::DevicePtr &device,
                                       const DescriptorPoolPtr &pool,
                                       const DescriptorSetLayoutPtr &layout);

/**
 * @brief   Update an existing shared VkDescriptorSet with a provided array of vierkant::descriptor_t.
 *
 * @param   device          handle for the vierkant::Device to update the DescriptorSet
 * @param   descriptor_set  handle for a shared VkDescriptorSet to update
 * @param   descriptors     an array of descriptor_t to use for updating the DescriptorSet
 */
void update_descriptor_set(const vierkant::DevicePtr &device, const DescriptorSetPtr &descriptor_set,
                           const std::vector<descriptor_t> &descriptors);

///////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_CLASS_PTR(Mesh);

/**
 * @brief   Mesh groups all sorts of resources,
 *          required to feed vertex-data into a (graphics-)pipeline.
 */
class Mesh : public Object3D
{
public:

    struct entry_t
    {
        index_t base_vertex = 0;
        uint32_t num_vertices = 0;
        index_t base_index = 0;
        uint32_t num_indices = 0;
        uint32_t material_index = 0;
        VkPrimitiveTopology primitive_type = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        bool enabled = true;
    };

    /**
     * @brief   Mesh::VertexAttrib defines a vertex-attribute available in the vertex-shader-stage.
     */
    struct attrib_t
    {
        int32_t location = -1;
        vierkant::BufferPtr buffer;
        VkDeviceSize buffer_offset = 0;
        uint32_t offset = 0;
        uint32_t stride = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkVertexInputRate input_rate = VK_VERTEX_INPUT_RATE_VERTEX;
    };

    static MeshPtr create();

    /**
     * @brief   Create a vierkant::MeshPtr from provided Geometry
     *          Will copy all available vertex-data into a single vertex buffer and create appropriate VertexAttribs for it.
     *
     * @param   device  handle for the vierkant::Device to create subresources with
     * @param   geom    a Geometry struct to extract the vertex information from
     * @return  the newly created vierkant::MeshPtr
     */
    static vierkant::MeshPtr
    create_from_geometries(const vierkant::DevicePtr &device,
                           const std::vector<GeometryPtr> &geometries,
                           bool interleave_data = true);

    Mesh(const Mesh &) = delete;

    Mesh(Mesh &&) = delete;

    Mesh &operator=(Mesh other) = delete;

    vierkant::AABB aabb() const override{ return boundingbox; }

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
    std::vector<attrib_t> vertex_attribs;

    // entries for sub-meshes
    std::vector<entry_t> entries;

    // index buffer
    vierkant::BufferPtr index_buffer;
    VkDeviceSize index_buffer_offset = 0;
    VkIndexType index_type = VK_INDEX_TYPE_UINT32;

    // boundingbox
    vierkant::AABB boundingbox;

private:

    Mesh() = default;
};

}//namespace vierkant

namespace std
{
template<>
struct hash<vierkant::descriptor_t>
{
    size_t operator()(const vierkant::descriptor_t &descriptor) const;
};
}
