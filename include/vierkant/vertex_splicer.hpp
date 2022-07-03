//
// Created by crocdialer on 7/2/22.
//

#pragma once

#include <vierkant/Geometry.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/vertex_attrib.hpp>

namespace vierkant
{

/**
 * @brief   vertex_splicer can be used to create an interleaved vertex-buffer from multiple geometries.
 */
class vertex_splicer
{
public:

    //! store base vertex/index
    struct geometry_offset_t
    {
        size_t base_vertex = 0;
        size_t base_index = 0;
        size_t base_meshlet = 0;
        size_t num_meshlets = 0;
    };

    /**
     * @brief   insert a geometry to be spliced.
     *
     * @param   geometry    a geometry
     * @return  true, if the geometry was inserted.
     */
    bool insert(const vierkant::GeometryConstPtr &geometry);

    /**
     * @brief   create_vertex_buffer can be used to create an interleaved vertex-buffer from all inserted geometries.
     *
     * @return  an array containing the spliced vertex-data for all geometries.
     */
    [[nodiscard]] std::vector<uint8_t> create_vertex_buffer() const;

    /**
     * @brief   create_vertex_attribs can be used to retrieve a description of all vertex-attributes.
     *
     * @return  a vertex_attrib_map_t
     */
    [[nodiscard]] vertex_attrib_map_t create_vertex_attribs() const;

    std::map<vierkant::GeometryConstPtr, geometry_offset_t> offsets;
    size_t vertex_stride = 0;

    //! combined array of indices
    std::vector<index_t> index_buffer;

    bool use_vertex_colors = false;

private:

    struct vertex_data_t
    {
        uint32_t attrib_location;
        const uint8_t *data;
        size_t offset;
        size_t elem_size;
        size_t num_elems;
        VkFormat format;
    };

    template<typename A>
    inline void add_attrib(uint32_t location,
                           const A &array,
                           std::vector<vertex_data_t> &vertex_data,
                           size_t &offset,
                           size_t &stride,
                           size_t &num_bytes,
                           size_t &num_attribs)
    {
        if(!array.empty())
        {
            using elem_t = typename std::decay<decltype(array)>::type::value_type;
            size_t elem_size = sizeof(elem_t);
            vertex_data.push_back({location,
                                   (uint8_t *) array.data(),
                                   offset,
                                   static_cast<uint32_t>(elem_size),
                                   array.size(),
                                   vierkant::format<elem_t>()});
            offset += elem_size;
            stride += elem_size;
            num_bytes += array.size() * elem_size;
            num_attribs++;
        }
    }

    bool check_and_insert(const GeometryConstPtr &g);

    std::vector<vertex_data_t> m_vertex_data;
    std::vector<size_t> m_vertex_offsets;
    size_t m_num_bytes = 0;
    size_t m_num_attribs = 0;
    size_t m_current_base_vertex = 0, m_current_base_index = 0;
};

}
