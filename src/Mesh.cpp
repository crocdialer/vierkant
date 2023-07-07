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
using buffer_binding_set_t = std::set<std::tuple<vierkant::BufferPtr, VkDeviceSize, uint32_t, VkVertexInputRate>>;

///////////////////////////////////////////////////////////////////////////////////////////////////

template<>
VkIndexType index_type<uint8_t>()
{
    return VK_INDEX_TYPE_UINT8_EXT;
}

template<>
VkIndexType index_type<uint16_t>()
{
    return VK_INDEX_TYPE_UINT16;
}

template<>
VkIndexType index_type<uint32_t>()
{
    return VK_INDEX_TYPE_UINT32;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

template<>
VkFormat format<uint8_t>()
{
    return VK_FORMAT_R8_UNORM;
}

template<>
VkFormat format<float>()
{
    return VK_FORMAT_R32_SFLOAT;
}

template<>
VkFormat format<glm::vec2>()
{
    return VK_FORMAT_R32G32_SFLOAT;
}

template<>
VkFormat format<glm::vec3>()
{
    return VK_FORMAT_R32G32B32_SFLOAT;
}

template<>
VkFormat format<glm::vec4>()
{
    return VK_FORMAT_R32G32B32A32_SFLOAT;
}

template<>
VkFormat format<int32_t>()
{
    return VK_FORMAT_R32_SINT;
}

template<>
VkFormat format<glm::ivec2>()
{
    return VK_FORMAT_R32G32_SINT;
}

template<>
VkFormat format<glm::ivec3>()
{
    return VK_FORMAT_R32G32B32_SINT;
}

template<>
VkFormat format<glm::ivec4>()
{
    return VK_FORMAT_R32G32B32A32_SINT;
}

template<>
VkFormat format<uint32_t>()
{
    return VK_FORMAT_R32_UINT;
}

template<>
VkFormat format<glm::uvec2>()
{
    return VK_FORMAT_R32G32_UINT;
}

template<>
VkFormat format<glm::uvec3>()
{
    return VK_FORMAT_R32G32B32_UINT;
}

template<>
VkFormat format<glm::uvec4>()
{
    return VK_FORMAT_R32G32B32A32_UINT;
}

template<>
VkFormat format<glm::vec<2, uint16_t>>()
{
    return VK_FORMAT_R16G16_UINT;
}

template<>
VkFormat format<glm::vec<3, uint16_t>>()
{
    return VK_FORMAT_R16G16B16_UINT;
}

template<>
VkFormat format<glm::vec<4, uint16_t>>()
{
    return VK_FORMAT_R16G16B16A16_UINT;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::MeshPtr Mesh::create_from_geometry(const vierkant::DevicePtr &device, const GeometryPtr &geometry,
                                             const create_info_t &create_info)
{
    entry_create_info_t entry_create_info = {};
    entry_create_info.geometry = geometry;
    return create_with_entries(device, {entry_create_info}, create_info);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::MeshPtr Mesh::create_with_entries(const vierkant::DevicePtr &device,
                                            const std::vector<entry_create_info_t> &entry_create_infos,
                                            const create_info_t &create_info)
{
    if(entry_create_infos.empty()) { return nullptr; }

    create_mesh_buffers_params_t params = {};
    params.optimize_vertex_cache = create_info.optimize_vertex_cache;
    params.generate_lods = create_info.generate_lods;
    params.generate_meshlets = create_info.generate_meshlets;
    params.use_vertex_colors = create_info.use_vertex_colors;
    params.pack_vertices = create_info.pack_vertices;

    mesh_buffer_bundle_t buffers = create_mesh_buffers(entry_create_infos, params);
    return create_from_bundle(device, buffers, create_info);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::MeshPtr Mesh::create_from_bundle(const vierkant::DevicePtr &device,
                                           const vierkant::mesh_buffer_bundle_t &mesh_buffer_bundle,
                                           const create_info_t &create_info)
{
    constexpr auto num_array_bytes = [](const auto &array) -> size_t {
        using elem_t = typename std::decay<decltype(array)>::type::value_type;
        return array.size() * sizeof(elem_t);
    };
    size_t num_staging_bytes = 0;
    size_t staging_offset = 0;
    num_staging_bytes += num_array_bytes(mesh_buffer_bundle.vertex_buffer);
    num_staging_bytes += num_array_bytes(mesh_buffer_bundle.bone_vertex_buffer);
    num_staging_bytes += num_array_bytes(mesh_buffer_bundle.index_buffer);
    num_staging_bytes += num_array_bytes(mesh_buffer_bundle.morph_buffer);
    num_staging_bytes += num_array_bytes(mesh_buffer_bundle.meshlets);
    num_staging_bytes += num_array_bytes(mesh_buffer_bundle.meshlet_vertices);
    num_staging_bytes += num_array_bytes(mesh_buffer_bundle.meshlet_triangles);

    auto staging_buffer = create_info.staging_buffer;

    // combine buffers into staging buffer
    if(!staging_buffer)
    {
        staging_buffer = vierkant::Buffer::create(device, nullptr, num_staging_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                  VMA_MEMORY_USAGE_CPU_ONLY);
    }
    else { staging_buffer->set_data(nullptr, num_staging_bytes); }

    auto staging_copy = [num_array_bytes, staging_buffer, &staging_offset, command_buffer = create_info.command_buffer,
                         device](const auto &array, vierkant::BufferPtr &outbuffer, VkBufferUsageFlags flags) {
        flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        size_t num_bytes = num_array_bytes(array);

        assert(staging_buffer->num_bytes() - num_bytes >= staging_offset);

        // copy array into staging-buffer
        auto staging_data = static_cast<uint8_t *>(staging_buffer->map()) + staging_offset;
        memcpy(staging_data, array.data(), num_bytes);

        if(!outbuffer)
        {
            outbuffer = vierkant::Buffer::create(device, nullptr, num_bytes, flags, VMA_MEMORY_USAGE_GPU_ONLY);
        }
        else { outbuffer->set_data(nullptr, num_bytes); }

        // issue copy from staging-buffer to GPU-buffer
        staging_buffer->copy_to(outbuffer, command_buffer, staging_offset, 0, num_bytes);
        staging_offset += num_bytes;
    };

    // create vertexbuffer
    vierkant::BufferPtr vertex_buffer;
    staging_copy(mesh_buffer_bundle.vertex_buffer, vertex_buffer,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                         create_info.buffer_usage_flags);

    auto mesh = vierkant::Mesh::create();
    mesh->vertex_buffer = vertex_buffer;
    mesh->vertex_attribs = mesh_buffer_bundle.vertex_attribs;
    mesh->entries = mesh_buffer_bundle.entries;
    for(auto &[location, vertex_attrib]: mesh->vertex_attribs) { vertex_attrib.buffer = vertex_buffer; }

    auto buffer_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | create_info.buffer_usage_flags;

    if(!mesh_buffer_bundle.morph_buffer.empty())
    {
        staging_copy(mesh_buffer_bundle.morph_buffer, mesh->morph_buffer, buffer_flags);
    }

    if(!mesh_buffer_bundle.meshlets.empty())
    {
        staging_copy(mesh_buffer_bundle.meshlets, mesh->meshlets, buffer_flags);
        staging_copy(mesh_buffer_bundle.meshlet_vertices, mesh->meshlet_vertices, buffer_flags);
        staging_copy(mesh_buffer_bundle.meshlet_triangles, mesh->meshlet_triangles, buffer_flags);
    }

    if(!mesh_buffer_bundle.bone_vertex_buffer.empty())
    {
        staging_copy(mesh_buffer_bundle.bone_vertex_buffer, mesh->bone_vertex_buffer, buffer_flags);
    }

    // use indices
    if(!mesh_buffer_bundle.index_buffer.empty())
    {
        staging_copy(mesh_buffer_bundle.index_buffer, mesh->index_buffer,
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT | create_info.buffer_usage_flags);
    }

    mesh->materials.resize(mesh_buffer_bundle.num_materials);
    for(auto &m: mesh->materials) { m = vierkant::Material::create(); }

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

    for(auto &[binding, attrib]: vertex_attribs)
    {
        buf_tuples.insert(std::make_tuple(attrib.buffer, attrib.buffer_offset, attrib.stride, attrib.input_rate));
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
    if(index_buffer) { vkCmdBindIndexBuffer(command_buffer, index_buffer->handle(), index_buffer_offset, index_type); }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mesh_buffer_bundle_t create_mesh_buffers(const std::vector<Mesh::entry_create_info_t> &entry_create_infos,
                                         const create_mesh_buffers_params_t &params)
{
    mesh_buffer_bundle_t ret = {};

    vertex_splicer splicer;
    splicer.use_vertex_colors = params.use_vertex_colors;

    vertex_splicer morph_splice;
    uint32_t num_morph_targets = 0;
    uint32_t num_morph_vertices = 0;

    struct extra_offset_t
    {
        size_t morph_base_vertex = 0;
        size_t num_morph_targets = 0;

        std::vector<vierkant::Mesh::lod_t> lods;
    };
    std::map<vierkant::GeometryConstPtr, extra_offset_t> extra_offset_map;

    bool pack_vertices = params.pack_vertices;

    for(auto &ci: entry_create_infos)
    {
        if(!splicer.insert(ci.geometry))
        {
            spdlog::warn("create_mesh_buffers: array sizes do not match");
            return {};
        }

        if(num_morph_targets && num_morph_targets != ci.morph_targets.size())
        {
            spdlog::warn("create_mesh_buffers: morph-target counts do not match");
        }

        auto &extra_offsets = extra_offset_map[ci.geometry];

        Mesh::lod_t lod_0 = {};
        lod_0.base_index = splicer.offsets[ci.geometry].base_index;
        lod_0.num_indices = ci.geometry->indices.size();
        extra_offsets.lods.push_back(lod_0);

        extra_offsets.morph_base_vertex = num_morph_vertices;
        extra_offsets.num_morph_targets = ci.morph_targets.size();
        num_morph_targets += ci.morph_targets.size();

        for(auto &morph_geom: ci.morph_targets)
        {
            assert(ci.geometry->positions.size() == morph_geom->positions.size());
            morph_splice.insert(morph_geom);
            num_morph_vertices += morph_geom->positions.size();
        }
    }

    if(pack_vertices) { spdlog::debug("using quantized/packed vertex-layout"); }
    auto vertex_layout = pack_vertices ? VertexLayout::PACKED : VertexLayout::ADHOC;

    ret.vertex_buffer = splicer.create_vertex_buffer(vertex_layout);
    ret.index_buffer = splicer.index_buffer;
    ret.vertex_stride = params.pack_vertices ? sizeof(packed_vertex_t) : splicer.vertex_stride;
    ret.vertex_attribs = splicer.create_vertex_attribs(vertex_layout);
    ret.bone_vertex_buffer = splicer.create_bone_vertex_buffer();
    ret.num_morph_targets = num_morph_targets;
    ret.morph_buffer = morph_splice.create_vertex_buffer();

    // optional vertex/cache/fetch optimization here
    if(params.optimize_vertex_cache)
    {
        spdlog::stopwatch sw;

        for(const auto &[geom, offsets]: splicer.offsets)
        {
            auto index_data = ret.index_buffer.data() + offsets.base_index;
            size_t index_count = geom->indices.size();

            auto vertices = ret.vertex_buffer.data() + offsets.base_vertex * ret.vertex_stride;
            size_t vertex_count = geom->positions.size();

            meshopt_optimizeVertexCache(index_data, index_data, index_count, vertex_count);

            std::vector<uint32_t> vertex_remap(vertex_count);
            meshopt_optimizeVertexFetchRemap(vertex_remap.data(), index_data, index_count, vertex_count);
            meshopt_remapVertexBuffer(vertices, vertices, vertex_count, ret.vertex_stride, vertex_remap.data());
            meshopt_remapIndexBuffer(index_data, index_data, index_count, vertex_remap.data());

            // remap all morph-target-vertices
            auto &extra_offsets = extra_offset_map[geom];

            if(!ret.bone_vertex_buffer.empty())
            {
                size_t bone_vertex_stride = sizeof(bone_vertex_data_t);
                auto bone_vertex_data = ret.bone_vertex_buffer.data() + offsets.base_vertex * bone_vertex_stride;
                meshopt_remapVertexBuffer(bone_vertex_data, bone_vertex_data, vertex_count, bone_vertex_stride,
                                          vertex_remap.data());
            }

            for(uint32_t i = 0; i < extra_offsets.num_morph_targets; ++i)
            {
                auto morph_vertices = ret.morph_buffer.data() +
                                      (extra_offsets.morph_base_vertex + i * vertex_count) * morph_splice.vertex_stride;
                meshopt_remapVertexBuffer(morph_vertices, morph_vertices, vertex_count, morph_splice.vertex_stride,
                                          vertex_remap.data());
            }
        }

        spdlog::debug("optimize_vertex_cache: {} ({} mesh(es) - {} triangles)",
                      std::chrono::duration_cast<std::chrono::milliseconds>(sw.elapsed()), splicer.offsets.size(),
                      ret.index_buffer.size() / 3);
    }

    // generate LOD meshes here
    if(params.generate_lods)
    {
        // corresponds to mesh.entries
        for(auto &[geom, offsets]: splicer.offsets)
        {
            spdlog::stopwatch single_timer;

            auto index_data = ret.index_buffer.data() + offsets.base_index;
            auto vertices = ret.vertex_buffer.data() + offsets.base_vertex * ret.vertex_stride;

            auto &extra_offsets = extra_offset_map[geom];

            std::vector<index_t> lod_indices = {index_data, index_data + geom->indices.size()};
            size_t num_indices = geom->indices.size();

            size_t min_num = num_indices, max_num = num_indices;

            for(uint32_t i = 0; i < params.max_num_lods; ++i)
            {
                // shrink num_indices to 60%
                constexpr float shrink_factor = .5f;
                constexpr float max_mismatch = .1f;
                constexpr float target_error = 0.05f;
                float result_error = 0.f;
                float result_factor = 1.f;

                auto target_index_count = static_cast<size_t>(static_cast<float>(num_indices) * shrink_factor);

                constexpr bool sloppy = true;

                if(sloppy)
                {
                    num_indices =
                            meshopt_simplifySloppy(lod_indices.data(), lod_indices.data(), lod_indices.size(),
                                                   reinterpret_cast<const float *>(vertices), geom->positions.size(),
                                                   ret.vertex_stride, target_index_count, target_error, &result_error);
                }
                else
                {
                    constexpr uint32_t options = 0;
                    num_indices = meshopt_simplify(lod_indices.data(), lod_indices.data(), lod_indices.size(),
                                                   reinterpret_cast<const float *>(vertices), geom->positions.size(),
                                                   ret.vertex_stride, target_index_count, target_error, options,
                                                   &result_error);
                }

                result_factor = static_cast<float>(num_indices) / static_cast<float>(lod_indices.size());

                spdlog::trace("level-of-detail #{}: {} triangles - target/actual shrink_factor: {} / {} - "
                              "target/actual error: {} / {}",
                              i + 1, num_indices / 3, shrink_factor, result_factor, target_error, result_error);

                // not getting any simpler
                if(result_factor - shrink_factor > max_mismatch) { break; }

                min_num = num_indices;
                lod_indices.resize(num_indices);

                // store lod_indices
                Mesh::lod_t lod = {};
                lod.base_index = ret.index_buffer.size();
                lod.num_indices = num_indices;
                extra_offsets.lods.push_back(lod);

                // insert at end of index-buffer
                ret.index_buffer.insert(ret.index_buffer.end(), lod_indices.begin(), lod_indices.end());
            }

            spdlog::trace("generated: {} levels-of-detail ({} -> {} triangles) - {}", extra_offsets.lods.size(),
                          max_num / 3, min_num / 3,
                          std::chrono::duration_cast<std::chrono::milliseconds>(single_timer.elapsed()));
        }
    }

    // optional meshlet generation
    if(params.generate_meshlets)
    {
        spdlog::stopwatch sw;

        // generate meshlets for all LODs of all submeshes
        for(auto &[geom, offsets]: splicer.offsets)
        {
            auto &extra_offsets = extra_offset_map[geom];

            // all LODs
            for(uint32_t lod_idx = 0; lod_idx < extra_offsets.lods.size(); ++lod_idx)
            {
                spdlog::stopwatch single_timer;

                auto &lod = extra_offsets.lods[lod_idx];

                auto index_data = ret.index_buffer.data() + lod.base_index;
                auto vertices = ret.vertex_buffer.data() + offsets.base_vertex * ret.vertex_stride;

                // determine size
                size_t max_meshlets = meshopt_buildMeshletsBound(lod.num_indices, params.meshlet_max_vertices,
                                                                 params.meshlet_max_triangles);
                if(!max_meshlets) { break; }

                std::vector<meshopt_Meshlet> meshlets(max_meshlets);
                std::vector<uint32_t> meshlet_vertices(max_meshlets * params.meshlet_max_vertices);
                std::vector<uint8_t> meshlet_triangles(max_meshlets * params.meshlet_max_triangles * 3);

                // generate meshlets (optimize for locality)
                size_t meshlet_count = meshopt_buildMeshlets(
                        meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(), index_data, lod.num_indices,
                        reinterpret_cast<const float *>(vertices), geom->positions.size(), ret.vertex_stride,
                        params.meshlet_max_vertices, params.meshlet_max_triangles, params.meshlet_cone_weight);

                spdlog::trace("generate_meshlets (lod-lvl: {}): {} ({} triangles -> {} meshlets)", lod_idx,
                              std::chrono::duration_cast<std::chrono::milliseconds>(single_timer.elapsed()),
                              lod.num_indices / 3, meshlet_count);

                lod.base_meshlet = ret.meshlets.size();
                lod.num_meshlets = meshlet_count;

                size_t meshlet_vertex_offset = ret.meshlet_vertices.size();
                size_t meshlet_triangle_offset = ret.meshlet_triangles.size();

                // insert entry-meshlet data
                const meshopt_Meshlet &last = meshlets[meshlet_count - 1];
                size_t vertex_count = last.vertex_offset + last.vertex_count;
                size_t triangle_offset = last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3);

                ret.meshlets.reserve(ret.meshlets.size() + meshlet_count);

                // generate bounds, combine data in our API output-meshlets
                for(uint32_t mi = 0; mi < meshlet_count; ++mi)
                {
                    const auto &m = meshlets[mi];
                    auto bounds = meshopt_computeMeshletBounds(
                            &meshlet_vertices[m.vertex_offset], &meshlet_triangles[m.triangle_offset], m.triangle_count,
                            reinterpret_cast<const float *>(vertices), geom->positions.size(), ret.vertex_stride);
                    vierkant::Mesh::meshlet_t out_meshlet = {};
                    out_meshlet.vertex_offset = meshlet_vertex_offset + m.vertex_offset;
                    out_meshlet.vertex_count = m.vertex_count;
                    out_meshlet.triangle_offset = meshlet_triangle_offset + m.triangle_offset;
                    out_meshlet.triangle_count = m.triangle_count;

                    out_meshlet.bounding_sphere = {*reinterpret_cast<glm::vec3 *>(bounds.center), bounds.radius};
                    out_meshlet.normal_cone = {*reinterpret_cast<glm::vec3 *>(bounds.cone_axis), bounds.cone_cutoff};

                    ret.meshlets.push_back(out_meshlet);
                }

                // add entry vertex-offset
                for(uint32_t vi = 0; vi < vertex_count; ++vi) { meshlet_vertices[vi] += offsets.base_vertex; }

                ret.meshlet_vertices.insert(ret.meshlet_vertices.end(), meshlet_vertices.begin(),
                                            meshlet_vertices.begin() + int(vertex_count));
                ret.meshlet_triangles.insert(ret.meshlet_triangles.end(), meshlet_triangles.begin(),
                                             meshlet_triangles.begin() + int(triangle_offset));
            }
        }

        if(!ret.meshlets.empty())
        {
            spdlog::debug("generate_meshlets: {} (max_vertices: {} - max_triangles: {} - {} mesh(es) - {} triangles - "
                          "{} meshlets)",
                          std::chrono::duration_cast<std::chrono::milliseconds>(sw.elapsed()),
                          params.meshlet_max_vertices, params.meshlet_max_triangles, splicer.offsets.size(),
                          ret.index_buffer.size() / 3, ret.meshlets.size());
        }
    }

    // keep track of used material-indices
    std::set<uint32_t> material_index_set;

    for(const auto &entry_info: entry_create_infos)
    {
        const auto &geom = entry_info.geometry;
        const auto &offsets = splicer.offsets[geom];
        const auto &extra_offsets = extra_offset_map[geom];

        vierkant::Mesh::entry_t entry = {};
        entry.name = entry_info.name;
        entry.primitive_type = geom->topology;
        entry.vertex_offset = static_cast<int32_t>(offsets.base_vertex);
        entry.num_vertices = geom->positions.size();

        // all LOD base/meshlet indices
        entry.lods = extra_offsets.lods;

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

        // morph weights
        entry.morph_weights = entry_info.morph_weights;
        entry.morph_vertex_offset = extra_offsets.morph_base_vertex;

        // insert new entry
        ret.entries.push_back(entry);
    }

    // keep track of total num materials
    ret.num_materials = material_index_set.size();
    return ret;
}

bool create_mesh_buffers_params_t::operator==(const create_mesh_buffers_params_t &other) const
{
    if(optimize_vertex_cache != other.optimize_vertex_cache) { return false; }
    if(generate_lods != other.generate_lods) { return false; }
    if(max_num_lods != other.max_num_lods) { return false; }
    if(generate_meshlets != other.generate_meshlets) { return false; }
    if(use_vertex_colors != other.use_vertex_colors) { return false; }
    if(pack_vertices != other.pack_vertices) { return false; }
    if(meshlet_max_vertices != other.meshlet_max_vertices) { return false; }
    if(meshlet_max_triangles != other.meshlet_max_triangles) { return false; }
    if(meshlet_cone_weight != other.meshlet_cone_weight) { return false; }
    return true;
}

}//namespace vierkant

size_t std::hash<vierkant::create_mesh_buffers_params_t>::operator()(
        vierkant::create_mesh_buffers_params_t const &params) const
{
    size_t hash_val = 0;
    vierkant::hash_combine(hash_val, params.optimize_vertex_cache);
    vierkant::hash_combine(hash_val, params.generate_lods);
    vierkant::hash_combine(hash_val, params.max_num_lods);
    vierkant::hash_combine(hash_val, params.generate_meshlets);
    vierkant::hash_combine(hash_val, params.use_vertex_colors);
    vierkant::hash_combine(hash_val, params.pack_vertices);
    vierkant::hash_combine(hash_val, params.meshlet_max_vertices);
    vierkant::hash_combine(hash_val, params.meshlet_max_triangles);
    vierkant::hash_combine(hash_val, params.meshlet_cone_weight);
    return hash_val;
}

size_t std::hash<vierkant::animated_mesh_t>::operator()(vierkant::animated_mesh_t const &key) const
{
    size_t h = 0;
    vierkant::hash_combine(h, key.mesh);
    vierkant::hash_combine(h, key.animation_state);
    return h;
}