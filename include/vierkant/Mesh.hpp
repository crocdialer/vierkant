//
// Created by crocdialer on 2/28/19.
//

#pragma once

#include "vierkant/Device.hpp"
#include "vierkant/Buffer.hpp"
#include "vierkant/Image.hpp"
#include "vierkant/intersection.hpp"

namespace vierkant {

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

DEFINE_CLASS_PTR(Mesh);

using DescriptorPoolPtr = std::shared_ptr<VkDescriptorPool_T>;

using DescriptorSetLayoutPtr = std::shared_ptr<VkDescriptorSetLayout_T>;

using DescriptorSetPtr = std::shared_ptr<VkDescriptorSet_T>;

using descriptor_count_t = std::vector<std::pair<VkDescriptorType, uint32_t>>;

using buffer_binding_set_t = std::set<std::tuple<vierkant::BufferPtr, uint32_t, uint32_t, VkVertexInputRate>>;

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief   Extract the types of descriptors and their counts for a given vierkant::Mesh
 * @param   mesh    a vierkant::Mesh to extract the descriptor counts from
 * @param   counts  a reference to a descriptor_count_map_t to hold the results
 */
void add_descriptor_counts(const MeshConstPtr &mesh, descriptor_count_t &counts);

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
 * @brief   Create a shared VkDescriptorSetLayout (DescriptorSetLayoutPtr) for a given vierkant::Mesh
 * @param   device  handle for the vierkant::Device to create the DescriptorSetLayout
 * @param   mesh    a vierkant::Mesh to create the layout for
 * @return  the newly created DescriptorSetLayoutPtr
 */
DescriptorSetLayoutPtr create_descriptor_set_layout(const vierkant::DevicePtr &device, const MeshConstPtr &mesh);

/**
 * @brief   Create a shared VkDescriptorSet (DescriptorSetPtr) for a given vierkant::Mesh
 * @param   device  handle for the vierkant::Device to create the DescriptorSets
 * @param   pool    handle for a shared VkDescriptorPool to allocate the DescriptorSets from
 * @param   mesh    a vierkant::Mesh to create the array of DescriptorSets for
 * @return  the newly created DescriptorSetPtr
 */
DescriptorSetPtr create_descriptor_set(const vierkant::DevicePtr &device,
                                       const DescriptorPoolPtr &pool,
                                       const MeshConstPtr &mesh);

/**
 * @brief   bind vertex- and index-buffers for the provided vierkant::Mesh
 * @param   command_buffer  handle to an VkCommandBuffer to record the bind-operation into
 * @param   mesh            the vierkant::Mesh from which to bind the buffers
 */
void bind_buffers(VkCommandBuffer command_buffer, const MeshConstPtr &mesh);

/**
 * @brief   Create an array of VkVertexInputAttributeDescription for a given vierkant::Mesh
 * @param   mesh    the vierkant::Mesh from which to extract the VkVertexInputAttributeDescriptions
 * @return  the newly created array of VkVertexInputAttributeDescriptions
 */
std::vector<VkVertexInputAttributeDescription> attribute_descriptions(const MeshConstPtr &mesh);

/**
 * @brief   Create an array of VkVertexInputBindingDescription for a given vierkant::Mesh
 * @param   mesh    the vierkant::Mesh from which to extract the VkVertexInputBindingDescriptions
 * @return  the newly created array of VkVertexInputBindingDescriptions
 */
std::vector<VkVertexInputBindingDescription> binding_descriptions(const MeshConstPtr &mesh);

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief   Create a vierkant::MeshPtr from provided Geometry
 *          Will copy all available vertex-data into a single vertex buffer and create appropriate VertexAttribs for it.
 * @param   device  handle for the vierkant::Device to create subresources with
 * @param   geom    a Geometry struct to extract the vertex information from
 * @return  the newly created vierkant::MeshPtr
 */
vierkant::MeshPtr
create_mesh_from_geometry(const vierkant::DevicePtr &device, const Geometry &geom, bool interleave_data = true);

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief   Mesh groups all sorts of resources,
 *          required to feed vertex-data into a (graphics-)pipeline.
 *          Also used to add resource-descriptors for e.g. uniform-buffers or image-samplers.
 */
class Mesh
{
public:

    /**
     * @brief   Mesh::VertexAttrib defines a vertex-attribute available in the vertex-shader-stage.
     */
    struct VertexAttrib
    {
        int32_t location = -1;
        vierkant::BufferPtr buffer;
        VkDeviceSize buffer_offset = 0;
        uint32_t offset = 0;
        uint32_t stride = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkVertexInputRate input_rate = VK_VERTEX_INPUT_RATE_VERTEX;
    };

    /**
     * @brief   Mesh::Descriptor defines a resource available in a shader
     */
    struct Descriptor
    {
        VkDescriptorType type;
        VkShaderStageFlags stage_flags;
        uint32_t binding = 0;
        vierkant::BufferPtr buffer;
        VkDeviceSize buffer_offset = 0;
        std::vector<vierkant::ImagePtr> image_samplers;
    };

    static MeshPtr create();

    Mesh(const Mesh &) = delete;

    Mesh(Mesh &&) = delete;

    Mesh &operator=(Mesh other) = delete;

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    uint32_t num_elements = 0;

    // index buffer
    vierkant::BufferPtr index_buffer;
    VkDeviceSize index_buffer_offset = 0;
    VkIndexType index_type = VK_INDEX_TYPE_UINT32;

    // vertex attributes
    std::vector<VertexAttrib> vertex_attribs;

    // descriptors -> layout
    std::vector<Descriptor> descriptors;
    DescriptorSetLayoutPtr descriptor_set_layout;

private:

    Mesh() = default;
};

}//namespace vierkant
