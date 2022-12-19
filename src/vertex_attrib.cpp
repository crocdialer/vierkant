//
// Created by crocdialer on 1/31/22.
//

#include <set>
#include <vierkant/vertex_attrib.hpp>

namespace vierkant
{

//! internal helper type
using buffer_binding_set_t = std::set<std::tuple<vierkant::BufferPtr, VkDeviceSize, uint32_t, VkVertexInputRate>>;

std::vector<VkVertexInputAttributeDescription> create_attribute_descriptions(const vertex_attrib_map_t &attribs)
{
    buffer_binding_set_t buf_tuples;

    for(auto &[binding, attrib] : attribs)
    {
        buf_tuples.insert(std::make_tuple(attrib.buffer, attrib.buffer_offset, attrib.stride, attrib.input_rate));
    }

    auto binding_index = [](const vertex_attrib_t &a, const buffer_binding_set_t &bufs) -> int32_t
    {
        int32_t i = 0;
        for(const auto &t : bufs)
        {
            if(t == std::make_tuple(a.buffer, a.buffer_offset, a.stride, a.input_rate)){ return i; }
            i++;
        }
        return -1;
    };

    std::vector<VkVertexInputAttributeDescription> ret;

    for(const auto &[location, attrib] : attribs)
    {
        auto binding = binding_index(attrib, buf_tuples);

        if(binding >= 0)
        {
            VkVertexInputAttributeDescription att;
            att.offset = attrib.offset;
            att.binding = static_cast<uint32_t>(binding);
            att.location = location;
            att.format = attrib.format;
            ret.push_back(att);
        }
    }
    return ret;
}

std::vector<VkVertexInputBindingDescription> create_binding_descriptions(const vertex_attrib_map_t &attribs)
{
    buffer_binding_set_t buf_tuples;

    for(auto &[binding, attrib] : attribs)
    {
        buf_tuples.insert(std::make_tuple(attrib.buffer, attrib.buffer_offset, attrib.stride, attrib.input_rate));
    }
    std::vector<VkVertexInputBindingDescription> ret;
    uint32_t i = 0;

    for(const auto &[buffer, buffer_offset, stride, input_rate] : buf_tuples)
    {
        VkVertexInputBindingDescription desc;
        desc.binding = i++;
        desc.stride = stride;
        desc.inputRate = input_rate;
        ret.push_back(desc);
    }
    return ret;
}
}// namespace vierkant