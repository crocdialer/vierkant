//
// Created by crocdialer on 7/2/22.
//

#include <vierkant/vertex_splicer.hpp>

namespace vierkant
{

bool vertex_splicer::insert(const vierkant::GeometryConstPtr &geometry)
{
    auto geom_it = offsets.find(geometry);

    if(geom_it == offsets.end())
    {
        size_t current_offset = m_num_bytes;

        if(!check_and_insert(geometry)){ return false; }
        m_vertex_offsets.push_back(current_offset);
        index_buffer.insert(index_buffer.end(), geometry->indices.begin(), geometry->indices.end());

        offsets[geometry] = {m_current_base_vertex, m_current_base_index};
        m_current_base_vertex += geometry->positions.size();
        m_current_base_index += geometry->indices.size();
    }
    return true;
}

[[nodiscard]] std::vector<uint8_t> vertex_splicer::create_vertex_buffer() const
{
    std::vector<uint8_t> ret(m_num_bytes);

    for(uint32_t o = 0; o < m_vertex_offsets.size(); ++o)
    {
        auto buf_data = ret.data() + m_vertex_offsets[o];

        for(uint32_t i = o * m_num_attribs; i < (o + 1) * m_num_attribs; ++i)
        {
            const auto &v = m_vertex_data[i];

            for(uint32_t j = 0; j < v.num_elems; ++j)
            {
                memcpy(buf_data + v.offset + j * vertex_stride, v.data + j * v.elem_size, v.elem_size);
            }
        }
    }
    return ret;
}

[[nodiscard]] vertex_attrib_map_t vertex_splicer::create_vertex_attribs() const
{
    vertex_attrib_map_t ret;

    for(uint32_t i = 0; i < m_num_attribs; ++i)
    {
        const auto &v = m_vertex_data[i];

        vierkant::vertex_attrib_t attrib;
        attrib.offset = v.offset;
        attrib.stride = static_cast<uint32_t>(vertex_stride);
        attrib.buffer = nullptr;
        attrib.buffer_offset = 0;
        attrib.format = v.format;
        ret[v.attrib_location] = attrib;
    }
    return ret;
}

bool vertex_splicer::check_and_insert(const GeometryConstPtr &g)
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
        add_attrib(Mesh::ATTRIB_COLOR, g->colors, m_vertex_data, offset, geom_vertex_stride, num_geom_bytes,
                   num_geom_attribs);
    }

    add_attrib(Mesh::ATTRIB_POSITION, g->positions, m_vertex_data, offset, geom_vertex_stride, num_geom_bytes,
               num_geom_attribs);
    add_attrib(Mesh::ATTRIB_TEX_COORD, g->tex_coords, m_vertex_data, offset, geom_vertex_stride, num_geom_bytes,
               num_geom_attribs);
    add_attrib(Mesh::ATTRIB_NORMAL, g->normals, m_vertex_data, offset, geom_vertex_stride, num_geom_bytes,
               num_geom_attribs);
    add_attrib(Mesh::ATTRIB_TANGENT, g->tangents, m_vertex_data, offset, geom_vertex_stride, num_geom_bytes,
               num_geom_attribs);
    add_attrib(Mesh::ATTRIB_BONE_INDICES, g->bone_indices, m_vertex_data, offset, geom_vertex_stride,
               num_geom_bytes,
               num_geom_attribs);
    add_attrib(Mesh::ATTRIB_BONE_WEIGHTS, g->bone_weights, m_vertex_data, offset, geom_vertex_stride,
               num_geom_bytes,
               num_geom_attribs);

    if(m_num_attribs && num_geom_attribs != m_num_attribs){ return false; }
    m_num_attribs = num_geom_attribs;
    vertex_stride = geom_vertex_stride;
    m_num_bytes += num_geom_bytes;

    return true;
}

}