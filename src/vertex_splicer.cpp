//
// Created by crocdialer on 7/2/22.
//

#include <meshoptimizer.h>
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

[[nodiscard]] std::vector<uint8_t> vertex_splicer::create_vertex_buffer(VertexLayout layout) const
{
    std::vector<uint8_t> ret;

    if(layout == VertexLayout::ADHOC)
    {
        ret.resize(m_num_bytes);

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
    }
    else if(layout == VertexLayout::PACKED)
    {
        constexpr int num_mantissa_bits = 18;//1..23

        size_t num_bytes = 0;

        for(const auto &[geom, offset_bundle]: offsets)
        {
            num_bytes += geom->positions.size() * sizeof(packed_vertex_t);

            if(geom->positions.empty() || geom->normals.empty() || geom->tangents.empty() || geom->tex_coords.empty())
            {
                return {};
            }
        }
        ret.resize(num_bytes);

        // pack/fill
        for(const auto &[geom, offset_bundle]: offsets)
        {
            for(uint32_t i = 0; i < geom->positions.size(); ++i)
            {
                const glm::vec3 &pos = geom->positions[i];
                const glm::vec3 &normal = geom->normals[i];
                const glm::vec3 &tangent = geom->tangents[i];
                const glm::vec2 &texcoord = geom->tex_coords[i];

                packed_vertex_t *v = (packed_vertex_t *) ret.data() + offset_bundle.base_vertex + i;
                v->pos_x = meshopt_quantizeFloat(pos.x, num_mantissa_bits);
                v->pos_y = meshopt_quantizeFloat(pos.y, num_mantissa_bits);
                v->pos_z = meshopt_quantizeFloat(pos.z, num_mantissa_bits);

                v->normal_x = uint8_t(normal.x * 127.f + 127.5f);
                v->normal_y = uint8_t(normal.y * 127.f + 127.5f);
                v->normal_z = uint8_t(normal.z * 127.f + 127.5f);
                v->normal_w = 0;

                v->tangent_x = uint8_t(tangent.x * 127.f + 127.5f);
                v->tangent_y = uint8_t(tangent.y * 127.f + 127.5f);
                v->tangent_z = uint8_t(tangent.z * 127.f + 127.5f);
                v->tangent_w = 0;

                v->texcoord_x = meshopt_quantizeHalf(texcoord.x);
                v->texcoord_y = meshopt_quantizeHalf(texcoord.y);
            }
        }
    }
    return ret;
}

[[nodiscard]] vertex_attrib_map_t vertex_splicer::create_vertex_attribs(VertexLayout layout) const
{
    vertex_attrib_map_t ret;

    if(layout == VertexLayout::ADHOC)
    {
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
    }
    else if(layout == VertexLayout::PACKED)
    {
        auto &pos_attrib = ret[Mesh::AttribLocation::ATTRIB_POSITION];
        pos_attrib.format = VK_FORMAT_R32G32B32_SFLOAT;
        pos_attrib.offset = offsetof(packed_vertex_t, pos_x);
        pos_attrib.stride = sizeof(packed_vertex_t);

        auto &normal_attrib = ret[Mesh::AttribLocation::ATTRIB_NORMAL];
        normal_attrib.format = VK_FORMAT_R8G8B8A8_UINT;
        normal_attrib.offset = offsetof(packed_vertex_t, normal_x);
        normal_attrib.stride = sizeof(packed_vertex_t);

        auto &texcoord_attrib = ret[Mesh::AttribLocation::ATTRIB_TEX_COORD];
        texcoord_attrib.format = VK_FORMAT_R16G16_SFLOAT;
        texcoord_attrib.offset = offsetof(packed_vertex_t, texcoord_x);
        texcoord_attrib.stride = sizeof(packed_vertex_t);

        auto &tangent_attrib = ret[Mesh::AttribLocation::ATTRIB_TANGENT];
        tangent_attrib.format = VK_FORMAT_R8G8B8A8_UINT;
        tangent_attrib.offset = offsetof(packed_vertex_t, tangent_x);
        tangent_attrib.stride = sizeof(packed_vertex_t);
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

std::vector<uint8_t> vertex_splicer::create_bone_vertex_buffer() const
{
    std::vector<uint8_t> ret;
    size_t num_bytes = 0;

    for(const auto &[geom, offset_bundle]: offsets)
    {
        num_bytes += geom->positions.size() * sizeof(bone_vertex_data_t);
        if(geom->bone_weights.empty() || geom->bone_indices.empty()){ return {}; }
    }
    ret.resize(num_bytes);

    // pack/fill
    for(const auto &[geom, offset_bundle]: offsets)
    {
        for(uint32_t i = 0; i < geom->positions.size(); ++i)
        {
            const auto &indices = geom->bone_indices[i];
            const auto &weights = geom->bone_weights[i];

            bone_vertex_data_t *v = (bone_vertex_data_t *) ret.data() + offset_bundle.base_vertex + i;
            v->index_x = indices.x;
            v->index_y = indices.y;
            v->index_z = indices.z;
            v->index_w = indices.w;

            v->weight_x = meshopt_quantizeHalf(weights.x);
            v->weight_y = meshopt_quantizeHalf(weights.y);
            v->weight_z = meshopt_quantizeHalf(weights.z);
            v->weight_w = meshopt_quantizeHalf(weights.w);
        }
    }

    return ret;
}

}