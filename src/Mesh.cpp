//
// Created by crocdialer on 2/28/19.
//

#include <map>
#include <set>

#include <crocore/utils.hpp>
#include <meshoptimizer.h>
#include <vierkant/Mesh.hpp>
#include <vierkant/vertex_splicer.hpp>

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
VkFormat format<glm::vec<2, uint16_t>>
        ()
{
    return
            VK_FORMAT_R16G16_UINT;
}

template<>
VkFormat format<glm::vec<3, uint16_t>>
        ()
{
    return
            VK_FORMAT_R16G16B16_UINT;
}

template<>
VkFormat format<glm::vec<4, uint16_t>>
        ()
{
    return
            VK_FORMAT_R16G16B16A16_UINT;
}

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

    mesh_buffer_bundle_t buffers = create_combined_buffers(entry_create_infos,
                                                           create_info.optimize_vertex_cache,
                                                           create_info.generate_meshlets,
                                                           create_info.use_vertex_colors);

    constexpr auto num_array_bytes = [](const auto &array) -> size_t
    {
        using elem_t = typename std::decay<decltype(array)>::type::value_type;
        return array.size() * sizeof(elem_t);
    };
    size_t num_staging_bytes = 0;
    size_t staging_offset = 0;
    num_staging_bytes += num_array_bytes(buffers.vertex_buffer);
    num_staging_bytes += num_array_bytes(buffers.index_buffer);
    num_staging_bytes += num_array_bytes(buffers.meshlets);
    num_staging_bytes += num_array_bytes(buffers.meshlet_vertices);
    num_staging_bytes += num_array_bytes(buffers.meshlet_triangles);

    auto staging_buffer = create_info.staging_buffer;

    // combine buffers into staging buffer
    if(!staging_buffer)
    {
        staging_buffer = vierkant::Buffer::create(device, nullptr, num_staging_bytes,
                                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                  VMA_MEMORY_USAGE_CPU_ONLY);
    }
    else{ staging_buffer->set_data(nullptr, num_staging_bytes); }

    auto staging_copy = [num_array_bytes,
            staging_buffer,
            &staging_offset,
            command_buffer = create_info.command_buffer,
            device](const auto &array, vierkant::BufferPtr &outbuffer, VkBufferUsageFlags flags)
    {
        flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        size_t num_buffer_bytes = num_array_bytes(array);

        // copy array into staging-buffer
        auto staging_data = static_cast<uint8_t *>(staging_buffer->map()) + staging_offset;
        memcpy(staging_data, array.data(), num_buffer_bytes);

        if(!outbuffer)
        {
            outbuffer = vierkant::Buffer::create(device, nullptr, num_buffer_bytes, flags, VMA_MEMORY_USAGE_GPU_ONLY);
        }
        else{ outbuffer->set_data(nullptr, num_buffer_bytes); }

        // issue copy from stagin-buffer to GPU-buffer
        staging_buffer->copy_to(outbuffer, command_buffer, staging_offset, 0, num_buffer_bytes);
        staging_offset += num_buffer_bytes;
    };

    // create vertexbuffer
    vierkant::BufferPtr vertex_buffer;
    staging_copy(buffers.vertex_buffer, vertex_buffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                       create_info.buffer_usage_flags);

    auto mesh = vierkant::Mesh::create();
    mesh->vertex_buffer = vertex_buffer;
    mesh->vertex_attribs = std::move(buffers.vertex_attribs);
    mesh->entries = std::move(buffers.entries);
    for(auto &[location, vertex_attrib]: mesh->vertex_attribs){ vertex_attrib.buffer = vertex_buffer; }

    if(!buffers.meshlets.empty())
    {
        auto buffer_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | create_info.buffer_usage_flags;
        staging_copy(buffers.meshlets, mesh->meshlets, buffer_flags);
        staging_copy(buffers.meshlet_vertices, mesh->meshlet_vertices, buffer_flags);
        staging_copy(buffers.meshlet_triangles, mesh->meshlet_triangles, buffer_flags);
    }

    // use indices
    if(!buffers.index_buffer.empty())
    {
        staging_copy(buffers.index_buffer, mesh->index_buffer,
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT | create_info.buffer_usage_flags);
    }

    mesh->materials.resize(buffers.num_materials);
    for(auto &m: mesh->materials){ m = vierkant::Material::create(); }

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

    for(auto &pair: vertex_attribs)
    {
        auto &att = pair.second;
        buf_tuples.insert(std::make_tuple(att.buffer, att.buffer_offset, att.stride, att.input_rate));
    }

    std::vector<VkBuffer> buf_handles;
    std::vector<VkDeviceSize> offsets;

    for(const auto &[buffer, buffer_offset, stride, input_rate]: buf_tuples)
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
        const auto &animation = node_animations[animation_index];

        // entry animation transforms
        std::vector<glm::mat4> node_matrices;
        vierkant::nodes::build_node_matrices_bfs(root_node, animation, node_matrices);

        // morph-target weights
        std::vector<std::vector<float>> node_morph_weights;
        vierkant::nodes::build_morph_weights_bfs(root_node, animation, node_morph_weights);

        for(auto &entry: entries)
        {
            assert(entry.node_index < node_morph_weights.size());
            assert(entry.node_index < node_matrices.size());

            entry.transform = node_matrices[entry.node_index];
            entry.morph_weights = node_morph_weights[entry.node_index];
        }
    }
}

mesh_buffer_bundle_t create_combined_buffers(const std::vector<Mesh::entry_create_info_t> &entry_create_infos,
                                             bool optimize_vertex_cache,
                                             bool generate_meshlets,
                                             bool use_vertex_colors)
{
    mesh_buffer_bundle_t ret = {};

    vertex_splicer splicer;
    splicer.use_vertex_colors = use_vertex_colors;

    std::map<vierkant::GeometryConstPtr, std::vector<uint8_t>> morph_vertex_buffers;

    for(auto &ci: entry_create_infos)
    {
        if(!splicer.insert(ci.geometry))
        {
            spdlog::warn("create_mesh_from_geometry: array sizes do not match");
            return {};
        }

        vertex_splicer morph_splice;

        for(auto &morph_geom: ci.morph_targets)
        {
            morph_splice.insert(morph_geom);
        }

        morph_vertex_buffers[ci.geometry] = morph_splice.create_vertex_buffer();
    }

    ret.vertex_buffer = splicer.create_vertex_buffer();
    ret.index_buffer = splicer.index_buffer;
    ret.vertex_stride = splicer.vertex_stride;
    ret.vertex_attribs = splicer.create_vertex_attribs();

    // optional vertex/cache/fetch optimization here
    if(optimize_vertex_cache)
    {
        spdlog::stopwatch sw;

        for(const auto &[geom, offsets]: splicer.offsets)
        {
            auto index_data = ret.index_buffer.data() + offsets.index_offset;
            size_t index_count = geom->indices.size();

            auto vertices = ret.vertex_buffer.data() + offsets.vertex_offset * splicer.vertex_stride;
            size_t vertex_count = geom->positions.size();

            meshopt_optimizeVertexCache(index_data, index_data, index_count, vertex_count);

            std::vector<uint32_t> vertex_remap(vertex_count);
            meshopt_optimizeVertexFetchRemap(vertex_remap.data(), index_data, index_count, vertex_count);
            meshopt_remapVertexBuffer(vertices, vertices, vertex_count, splicer.vertex_stride, vertex_remap.data());
            meshopt_remapIndexBuffer(index_data, index_data, index_count, vertex_remap.data());
        }

        spdlog::debug("optimize_vertex_cache: {} ({} mesh(es) - {} triangles)",
                      std::chrono::duration_cast<std::chrono::milliseconds>(sw.elapsed()), splicer.offsets.size(),
                      ret.index_buffer.size() / 3);
    }

    // optional meshlet generation
    if(generate_meshlets)
    {
        // TODO: matches NV_mesh_shader, break out as parameters
        constexpr size_t max_vertices = 64;
        constexpr size_t max_triangles = 124;
        constexpr float cone_weight = 0.0f;

        spdlog::stopwatch sw;

        uint32_t meshlet_offset = 0;

        // corresponds to mesh.entries
        for(auto &[geom, offsets]: splicer.offsets)
        {
            spdlog::stopwatch single_timer;

            auto index_data = ret.index_buffer.data() + offsets.index_offset;
            auto vertices = ret.vertex_buffer.data() + offsets.vertex_offset * splicer.vertex_stride;

            // determine size
            size_t max_meshlets = meshopt_buildMeshletsBound(geom->indices.size(), max_vertices, max_triangles);
            std::vector<meshopt_Meshlet> meshlets(max_meshlets);
            std::vector<uint32_t> meshlet_vertices(max_meshlets * max_vertices);
            std::vector<uint8_t> meshlet_triangles(max_meshlets * max_triangles * 3);

            // generate meshlets (optimize for locality)
            size_t meshlet_count = meshopt_buildMeshlets(meshlets.data(), meshlet_vertices.data(),
                                                         meshlet_triangles.data(), index_data, geom->indices.size(),
                                                         reinterpret_cast<const float *>(vertices),
                                                         geom->positions.size(),
                                                         splicer.vertex_stride, max_vertices, max_triangles, cone_weight);

            spdlog::trace("generate_meshlet: {} ({} triangles -> {} meshlets)",
                          std::chrono::duration_cast<std::chrono::milliseconds>(single_timer.elapsed()),
                          geom->indices.size() / 3, meshlets.size());

//            size_t meshlet_count = meshopt_buildMeshletsScan(meshlets.data(), meshlet_vertices.data(),
//                                                             meshlet_triangles.data(), index_data, geom->indices.size(),
//                                                             geom->positions.size(),
//                                                             max_vertices, max_triangles);

            // pruning
            const meshopt_Meshlet &last = meshlets[meshlet_count - 1];

            size_t vertex_count = last.vertex_offset + last.vertex_count;
            size_t triangle_count = last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3);

            ret.meshlets.reserve(ret.meshlets.size() + meshlet_count);

            // generate bounds, combine data in our API output-meshlets
            for(uint32_t i = 0; i < meshlet_count; ++i)
            {
                const auto &m = meshlets[i];
                auto bounds = meshopt_computeMeshletBounds(&meshlet_vertices[m.vertex_offset],
                                                           &meshlet_triangles[m.triangle_offset],
                                                           m.triangle_count,
                                                           reinterpret_cast<const float *>(vertices),
                                                           geom->positions.size(), splicer.vertex_stride);
                vierkant::Mesh::meshlet_t out_meshlet = {};
                out_meshlet.vertex_offset = m.vertex_offset;
                out_meshlet.vertex_count = m.vertex_count;
                out_meshlet.triangle_offset = m.triangle_offset;
                out_meshlet.triangle_count = m.triangle_count;
                out_meshlet.bounding_sphere = {*reinterpret_cast<glm::vec3 *>(bounds.center), bounds.radius};

                ret.meshlets.push_back(out_meshlet);
            }

            offsets.meshlet_offset = meshlet_offset;
            offsets.num_meshlets = meshlet_count;
            meshlet_offset += meshlet_count;

            // insert entry-meshlet data
            ret.meshlet_vertices.insert(ret.meshlet_vertices.end(), meshlet_vertices.begin(),
                                        meshlet_vertices.begin() + int(vertex_count));
            ret.meshlet_triangles.insert(ret.meshlet_triangles.end(), meshlet_triangles.begin(),
                                         meshlet_triangles.begin() + int(triangle_count));
        }

        if(!ret.meshlets.empty())
        {
            spdlog::debug("generate_meshlets: {} ({} mesh(es) - {} triangles - {} meshlets)",
                          std::chrono::duration_cast<std::chrono::milliseconds>(sw.elapsed()), splicer.offsets.size(),
                          ret.index_buffer.size() / 3, ret.meshlets.size());
        }
    }

    // keep track of used material-indices
    std::set<uint32_t> material_index_set;

    for(const auto &entry_info: entry_create_infos)
    {
        const auto &geom = entry_info.geometry;

        auto[base_vertex, base_index, meshlet_offset, num_meshlets] = splicer.offsets[geom];

        vierkant::Mesh::entry_t entry = {};
        entry.name = entry_info.name;
        entry.primitive_type = geom->topology;
        entry.vertex_offset = static_cast<int32_t>(base_vertex);
        entry.num_vertices = geom->positions.size();
        entry.base_index = base_index;
        entry.num_indices = geom->indices.size();
        entry.meshlet_offset = static_cast<uint32_t>(meshlet_offset);
        entry.num_meshlets = num_meshlets;

        // use provided transforms for sub-meshes, if any
        entry.transform = entry_info.transform;

        // use provided node_index for sub-meshes, if any
        entry.node_index = entry_info.node_index;

        // use provided material_index for sub-meshes, if any
        entry.material_index = entry_info.material_index;
        material_index_set.insert(entry_info.material_index);

        // compute bounds
        entry.bounding_box = vierkant::compute_aabb(geom->positions);
        entry.bounding_sphere = vierkant::compute_bounding_sphere(geom->positions);

        // insert new entry
        ret.entries.push_back(entry);
    }

    // keep track of total num materials
    ret.num_materials = material_index_set.size();
    return ret;
}

}//namespace vierkant
