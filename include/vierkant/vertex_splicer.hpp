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
 * @brief   vertex_splicer can be used to create an interleaved vertex-buffer from an array of Geometries.
 */
class vertex_splicer
{
public:

    struct vertex_data_t
    {
        uint32_t attrib_location;
        const uint8_t *data;
        size_t offset;
        size_t elem_size;
        size_t num_elems;
        VkFormat format;
    };

    //! store base vertex/index
    struct geometry_offset_t
    {
        size_t vertex_offset = 0;
        size_t index_offset = 0;
        size_t meshlet_offset = 0;
        size_t num_meshlets = 0;
    };

    bool insert(const vierkant::GeometryConstPtr &geometry)
    {
        auto geom_it = geom_offsets.find(geometry);

        if(geom_it == geom_offsets.end())
        {
            size_t current_offset = num_bytes;

            if(!check_and_insert(geometry))
            {
                spdlog::warn("create_mesh_from_geometry: array sizes do not match");
                return false;
            }
            vertex_offsets.push_back(current_offset);
            index_buffer.insert(index_buffer.end(), geometry->indices.begin(), geometry->indices.end());

            geom_offsets[geometry] = {current_base_vertex, current_base_index};
            current_base_vertex += geometry->positions.size();
            current_base_index += geometry->indices.size();
        }
        return true;
    }

    [[nodiscard]] std::vector<uint8_t> create_vertex_buffer() const
    {
        std::vector<uint8_t> ret(num_bytes);

        for(uint32_t o = 0; o < vertex_offsets.size(); ++o)
        {
            auto buf_data = ret.data() + vertex_offsets[o];

            for(uint32_t i = o * num_attribs; i < (o + 1) * num_attribs; ++i)
            {
                const auto &v = vertex_data[i];

                for(uint32_t j = 0; j < v.num_elems; ++j)
                {
                    memcpy(buf_data + v.offset + j * vertex_stride, v.data + j * v.elem_size, v.elem_size);
                }
            }
        }
        return ret;
    }

    [[nodiscard]] vertex_attrib_map_t create_vertex_attribs() const
    {
        vertex_attrib_map_t ret;

        for(uint32_t i = 0; i < num_attribs; ++i)
        {
            const auto &v = vertex_data[i];

            vierkant::vertex_attrib_t attrib;
            attrib.offset = v.offset;
            attrib.stride = static_cast<uint32_t>(vertex_stride);
            attrib.buffer = nullptr;
            attrib.buffer_offset = 0;
            attrib.format = v.format;
            ret[v.attrib_location] = attrib;
        }
        return ret;
    };

    std::vector<vertex_data_t> vertex_data;
    std::map<vierkant::GeometryConstPtr, geometry_offset_t> geom_offsets;
    size_t vertex_stride = 0;
    size_t num_bytes = 0;
    size_t num_attribs = 0;

    //! combined array of indices
    std::vector<index_t> index_buffer;

    bool use_vertex_colors = false;

private:

    std::vector<size_t> vertex_offsets;
    size_t current_base_vertex = 0, current_base_index = 0;

    template<typename A>
    static void add_attrib(uint32_t location,
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

    bool check_and_insert(const GeometryConstPtr &g)
    {
        size_t num_geom_bytes = 0;
        size_t geom_vertex_stride = 0;
        size_t offset = 0;
        size_t num_geom_attribs = 0;
        auto num_vertices = g->positions.size();

        if(g->positions.empty()){ return false; }
        if(use_vertex_colors && !g->colors.empty() && g->colors.size() != num_vertices){ return false; }
        if(!g->tex_coords.empty() && g->tex_coords.size() != num_vertices){ return false; }
        if(!g->normals.empty() && g->normals.size() != num_vertices){ return false; }
        if(!g->tangents.empty() && g->tangents.size() != num_vertices){ return false; }
        if(!g->bone_indices.empty() && g->bone_indices.size() != num_vertices){ return false; }
        if(!g->bone_weights.empty() && g->bone_weights.size() != num_vertices){ return false; }

        if(use_vertex_colors)
        {
            add_attrib(Mesh::ATTRIB_COLOR, g->colors, vertex_data, offset, geom_vertex_stride, num_geom_bytes,
                       num_geom_attribs);
        }

        add_attrib(Mesh::ATTRIB_POSITION, g->positions, vertex_data, offset, geom_vertex_stride, num_geom_bytes,
                   num_geom_attribs);
        add_attrib(Mesh::ATTRIB_TEX_COORD, g->tex_coords, vertex_data, offset, geom_vertex_stride, num_geom_bytes,
                   num_geom_attribs);
        add_attrib(Mesh::ATTRIB_NORMAL, g->normals, vertex_data, offset, geom_vertex_stride, num_geom_bytes,
                   num_geom_attribs);
        add_attrib(Mesh::ATTRIB_TANGENT, g->tangents, vertex_data, offset, geom_vertex_stride, num_geom_bytes,
                   num_geom_attribs);
        add_attrib(Mesh::ATTRIB_BONE_INDICES, g->bone_indices, vertex_data, offset, geom_vertex_stride, num_geom_bytes,
                   num_geom_attribs);
        add_attrib(Mesh::ATTRIB_BONE_WEIGHTS, g->bone_weights, vertex_data, offset, geom_vertex_stride, num_geom_bytes,
                   num_geom_attribs);

        if(num_attribs && num_geom_attribs != num_attribs){ return false; }
        num_attribs = num_geom_attribs;
        vertex_stride = geom_vertex_stride;
        num_bytes += num_geom_bytes;

        return true;
    };
};

}
