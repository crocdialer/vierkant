//
// Created by crocdialer on 2/28/19.
//

#include <map>
#include <set>
#include <crocore/utils.hpp>
#include <meshoptimizer.h>
#include <vierkant/Mesh.hpp>

namespace vierkant
{

//! internal helper type
using buffer_binding_set_t = std::set<std::tuple<vierkant::BufferPtr, uint32_t, uint32_t, VkVertexInputRate>>;

///////////////////////////////////////////////////////////////////////////////////////////////////

template<>
VkIndexType index_type<uint8_t>(){ return VK_INDEX_TYPE_UINT8_EXT; }

template<>
VkIndexType index_type<uint16_t>(){ return VK_INDEX_TYPE_UINT16; }

template<>
VkIndexType index_type<uint32_t>(){ return VK_INDEX_TYPE_UINT32; }

///////////////////////////////////////////////////////////////////////////////////////////////////

template<>
VkFormat format<uint8_t>(){ return VK_FORMAT_R8_UNORM; }

template<>
VkFormat format<float>(){ return VK_FORMAT_R32_SFLOAT; }

template<>
VkFormat format<glm::vec2>(){ return VK_FORMAT_R32G32_SFLOAT; }

template<>
VkFormat format<glm::vec3>(){ return VK_FORMAT_R32G32B32_SFLOAT; }

template<>
VkFormat format<glm::vec4>(){ return VK_FORMAT_R32G32B32A32_SFLOAT; }

template<>
VkFormat format<int32_t>(){ return VK_FORMAT_R32_SINT; }

template<>
VkFormat format<glm::ivec2>(){ return VK_FORMAT_R32G32_SINT; }

template<>
VkFormat format<glm::ivec3>(){ return VK_FORMAT_R32G32B32_SINT; }

template<>
VkFormat format<glm::ivec4>(){ return VK_FORMAT_R32G32B32A32_SINT; }

template<>
VkFormat format<uint32_t>(){ return VK_FORMAT_R32_UINT; }

template<>
VkFormat format<glm::uvec2>(){ return VK_FORMAT_R32G32_UINT; }

template<>
VkFormat format<glm::uvec3>(){ return VK_FORMAT_R32G32B32_UINT; }

template<>
VkFormat format<glm::uvec4>(){ return VK_FORMAT_R32G32B32A32_UINT; }

template<>
VkFormat format<glm::vec<2, uint16_t>>(){ return VK_FORMAT_R16G16_UINT; }

template<>
VkFormat format<glm::vec<3, uint16_t>>(){ return VK_FORMAT_R16G16B16_UINT; }

template<>
VkFormat format<glm::vec<4, uint16_t>>(){ return VK_FORMAT_R16G16B16A16_UINT; }

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::MeshPtr
Mesh::create_from_geometry(const vierkant::DevicePtr &device, const GeometryPtr &geometry,
                           const create_info_t &create_info)
{
    entry_create_info_t entry_create_info = {};
    entry_create_info.geometry = geometry;
    return create_with_entries(device, {entry_create_info}, create_info);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::MeshPtr
Mesh::create_with_entries(const vierkant::DevicePtr &device,
                          const std::vector<entry_create_info_t> &entry_create_infos,
                          const create_info_t &create_info)
{
    if(entry_create_infos.empty()){ return nullptr; }

    struct vertex_data_t
    {
        uint32_t attrib_location;
        const uint8_t *data;
        size_t offset;
        size_t elem_size;
        size_t num_elems;
        VkFormat format;
    };
    std::vector<vertex_data_t> vertex_data;

    auto add_attrib = [](uint32_t location,
                         const auto &array, std::vector<vertex_data_t> &vertex_data,
                         size_t &offset,
                         size_t &stride,
                         size_t &num_bytes, size_t &num_attribs)
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
    };

    size_t vertex_stride = 0, num_bytes = 0, num_attribs = 0;

    // sanity check array sizes
    auto check_and_insert = [add_attrib, &vertex_stride, &num_bytes, &num_attribs](const GeometryConstPtr &g,
                                                                                   std::vector<vertex_data_t> &vertex_data) -> bool
    {
        vertex_stride = 0;
        size_t offset = 0;
        size_t num_geom_attribs = 0;
        auto num_vertices = g->positions.size();

        if(g->positions.empty()){ return false; }
        if(!g->colors.empty() && g->colors.size() != num_vertices){ return false; }
        if(!g->tex_coords.empty() && g->tex_coords.size() != num_vertices){ return false; }
        if(!g->normals.empty() && g->normals.size() != num_vertices){ return false; }
        if(!g->tangents.empty() && g->tangents.size() != num_vertices){ return false; }
        if(!g->bone_indices.empty() && g->bone_indices.size() != num_vertices){ return false; }
        if(!g->bone_weights.empty() && g->bone_weights.size() != num_vertices){ return false; }

        add_attrib(ATTRIB_POSITION, g->positions, vertex_data, offset, vertex_stride, num_bytes, num_geom_attribs);
        add_attrib(ATTRIB_COLOR, g->colors, vertex_data, offset, vertex_stride, num_bytes, num_geom_attribs);
        add_attrib(ATTRIB_TEX_COORD, g->tex_coords, vertex_data, offset, vertex_stride, num_bytes, num_geom_attribs);
        add_attrib(ATTRIB_NORMAL, g->normals, vertex_data, offset, vertex_stride, num_bytes, num_geom_attribs);
        add_attrib(ATTRIB_TANGENT, g->tangents, vertex_data, offset, vertex_stride, num_bytes, num_geom_attribs);
        add_attrib(ATTRIB_BONE_INDICES, g->bone_indices, vertex_data, offset, vertex_stride, num_bytes, num_geom_attribs);
        add_attrib(ATTRIB_BONE_WEIGHTS, g->bone_weights, vertex_data, offset, vertex_stride, num_bytes, num_geom_attribs);

        if(num_attribs && num_geom_attribs != num_attribs){ return false; }
        num_attribs = num_geom_attribs;

        return true;
    };

    std::vector<size_t> vertex_offsets;

    // store base vertex/index
    struct geometry_offset_t
    {
        size_t vertex_offset = 0;
        size_t index_offset = 0;
    };
    std::map<vierkant::GeometryConstPtr, geometry_offset_t> geom_offsets;
    size_t current_base_vertex = 0, current_base_index = 0;

    // concat indices
    std::vector<vierkant::index_t> indices;

    // insert all geometries
    for(auto &ci : entry_create_infos)
    {
        auto geom_it = geom_offsets.find(ci.geometry);

        if(geom_it == geom_offsets.end())
        {
            vertex_offsets.push_back(num_bytes);

            if(!check_and_insert(ci.geometry, vertex_data))
            {
                spdlog::warn("create_mesh_from_geometry: array sizes do not match");
                return nullptr;
            }

            indices.insert(indices.end(), ci.geometry->indices.begin(), ci.geometry->indices.end());

            geom_offsets[ci.geometry] = {current_base_vertex, current_base_index};
            current_base_vertex += ci.geometry->positions.size();
            current_base_index += ci.geometry->indices.size();
        }
    }

    auto mesh = vierkant::Mesh::create();

    auto staging_buffer = create_info.staging_buffer;

    // combine buffers into staging buffer
    if(!staging_buffer)
    {
        staging_buffer = vierkant::Buffer::create(device, nullptr, num_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                  VMA_MEMORY_USAGE_CPU_ONLY);
    }
    else{ staging_buffer->set_data(nullptr, num_bytes); }

    // create vertexbuffer
    auto vertex_buffer = vierkant::Buffer::create(device, nullptr, num_bytes,
                                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                  create_info.buffer_usage_flags,
                                                  VMA_MEMORY_USAGE_GPU_ONLY);
    auto staging_data = (uint8_t *) staging_buffer->map();


    for(uint32_t i = 0; i < num_attribs; ++i)
    {
        const auto &v = vertex_data[i];

        vierkant::vertex_attrib_t attrib;
        attrib.offset = v.offset;
        attrib.stride = static_cast<uint32_t>(vertex_stride);
        attrib.buffer = vertex_buffer;
        attrib.buffer_offset = 0;
        attrib.format = v.format;
        mesh->vertex_attribs[v.attrib_location] = attrib;
    }

    for(uint32_t o = 0; o < vertex_offsets.size(); ++o)
    {
        auto buf_data = staging_data + vertex_offsets[o];

        for(uint32_t i = o * num_attribs; i < (o + 1) * num_attribs; ++i)
        {
            const auto &v = vertex_data[i];

            for(uint32_t j = 0; j < v.num_elems; ++j)
            {
                memcpy(buf_data + v.offset + j * vertex_stride, v.data + j * v.elem_size, v.elem_size);
            }
        }
    }

    // done with things, optimize here
    if(create_info.optimize_vertex_cache)
    {
        for(const auto &[geom, offsets] : geom_offsets)
        {
            auto index_data = indices.data() + offsets.index_offset;
            auto vertices = staging_data + offsets.vertex_offset * vertex_stride;

            meshopt_optimizeVertexCache(index_data, index_data, geom->indices.size(), geom->positions.size());
            meshopt_optimizeVertexFetch(vertices, index_data, geom->indices.size(), vertices, geom->positions.size(),
                                        vertex_stride);
        }
    }

    // copy combined vertex data to device-buffer
    staging_buffer->copy_to(vertex_buffer, create_info.command_buffer);

    // keep track of used material-indices
    std::set<uint32_t> material_index_set;

    for(const auto &entry_info : entry_create_infos)
    {
        const auto &geom = entry_info.geometry;

        auto [base_vertex, base_index] = geom_offsets[geom];

        vierkant::Mesh::entry_t entry = {};
        entry.name = entry_info.name;
        entry.primitive_type = geom->topology;
        entry.vertex_offset = base_vertex;
        entry.num_vertices = geom->positions.size();
        entry.base_index = base_index;
        entry.num_indices = geom->indices.size();

        // use provided transforms for sub-meshes, if any
        entry.transform = entry_info.transform;

        // use provided node_index for sub-meshes, if any
        entry.node_index = entry_info.node_index;

        // use provided material_index for sub-meshes, if any
        entry.material_index = entry_info.material_index;
        material_index_set.insert(entry_info.material_index);

        // combine with aabb
        entry.boundingbox = vierkant::compute_aabb(geom->positions);

        // insert new entry
        mesh->entries.push_back(entry);
    }

    // use indices
    if(!indices.empty())
    {
        mesh->index_buffer = vierkant::Buffer::create(device, indices,
                                                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | create_info.buffer_usage_flags,
                                                      VMA_MEMORY_USAGE_GPU_ONLY);
    }

    mesh->materials.resize(material_index_set.size());
    for(auto &m : mesh->materials){ m = vierkant::Material::create(); }

    return mesh;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

MeshPtr Mesh::create()
{
    auto ret = MeshPtr(new Mesh());
//    ret->set_name("mesh_" + std::to_string(ret->id()));
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::bind_buffers(VkCommandBuffer command_buffer) const
{
    buffer_binding_set_t buf_tuples;

    for(auto &pair : vertex_attribs)
    {
        auto &att = pair.second;
        buf_tuples.insert(std::make_tuple(att.buffer, att.buffer_offset, att.stride, att.input_rate));
    }

    std::vector<VkBuffer> buf_handles;
    std::vector<VkDeviceSize> offsets;

    for(const auto &[buffer, buffer_offset, stride, input_rate] : buf_tuples)
    {
        buf_handles.push_back(buffer->handle());
        offsets.push_back(buffer_offset);
    }

    // bind vertex buffer
    vkCmdBindVertexBuffers(command_buffer, 0, static_cast<uint32_t>(buf_handles.size()), buf_handles.data(),
                           offsets.data());

    // bind index buffer
    if(index_buffer)
    {
        vkCmdBindIndexBuffer(command_buffer, index_buffer->handle(), index_buffer_offset, index_type);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<VkVertexInputAttributeDescription> Mesh::attribute_descriptions() const
{
    return create_attribute_descriptions(vertex_attribs);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<VkVertexInputBindingDescription> Mesh::binding_descriptions() const
{
    return create_binding_descriptions(vertex_attribs);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::update_entry_transforms()
{
    if(!root_bone && animation_index < node_animations.size())
    {
        // entry animation transforms
        std::vector<glm::mat4> node_matrices;
        vierkant::nodes::build_node_matrices_bfs(root_node,
                                                 node_animations[animation_index],
                                                 node_matrices);

        for(auto &entry : entries){ entry.transform = node_matrices[entry.node_index]; }
    }
}

}//namespace vierkant
