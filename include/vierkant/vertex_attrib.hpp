//
// Created by crocdialer on 1/31/22.
//

#pragma once

#include <vierkant/Buffer.hpp>

namespace vierkant
{

/**
* @brief   Mesh::VertexAttrib defines a vertex-attribute available in the vertex-shader-stage.
*/
struct vertex_attrib_t
{
    vierkant::BufferPtr buffer;
    VkDeviceSize buffer_offset = 0;
    uint32_t offset = 0;
    uint32_t stride = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkVertexInputRate input_rate = VK_VERTEX_INPUT_RATE_VERTEX;
};

using vertex_attrib_map_t = std::map<uint32_t, vertex_attrib_t>;

/**
 * @brief   create an array of VkVertexInputAttributeDescription
 *
 * @param   attribs     a provided map containing bindings of vierkant::vertex_attrib_t.
 *
 * @return  an array of VkVertexInputAttributeDescription
 */
std::vector<VkVertexInputAttributeDescription> create_attribute_descriptions(const vertex_attrib_map_t &attribs);

/**
 * @brief   create an array of VkVertexInputBindingDescription
 *
 * @param   attribs     a provided map containing bindings of vierkant::vertex_attrib_t.
 *
 * @return  an array of VkVertexInputBindingDescription
 */

std::vector<VkVertexInputBindingDescription> create_binding_descriptions(const vertex_attrib_map_t &attribs);

}// namespace vierkant
