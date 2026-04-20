import "../renderer/types.slang";
import "../utils/transform.slang";
import "../utils/packed_vertex.slang";
import "../utils/camera.slang";
import "../utils/project_sphere.slang";

// Specialization-constant IDs mirror the GLSL constant_id assignments
[[vk::constant_id(1)]] const uint mesh_workgroup_x = renderer::MAX_MESH_WORKGROUP_SIZE;

// ---- shared bindings (set 0) -----------------------------------------------

[[vk::push_constant]]
renderer::context_t context;

[[vk::binding(renderer::ResourceBinding::DrawCommands, 0)]]
StructuredBuffer<renderer::indexed_draw_command_t> draw_commands;

[[vk::binding(renderer::ResourceBinding::Meshlets, 0)]]
StructuredBuffer<renderer::meshlet_t> meshlets;

[[vk::binding(renderer::ResourceBinding::MeshDraws, 0)]]
StructuredBuffer<renderer::mesh_draw_t> draws;

[[vk::binding(renderer::ResourceBinding::MeshletVisibility, 0)]]
RWStructuredBuffer<uint> meshlet_visibilities;

struct camera_ubo_t
{
    utils::camera_t camera;
    utils::camera_t last_camera;
};
[[vk::binding(renderer::ResourceBinding::JitterOffset, 0)]]
ConstantBuffer<camera_ubo_t> camera_ubo;

// mesh-shader-only bindings
[[vk::binding(renderer::ResourceBinding::Vertices, 0)]]
StructuredBuffer<Ptr<utils::packed_vertex_t, Access.Read>> vertex_buffers;

[[vk::binding(renderer::ResourceBinding::MeshletVertices, 0)]]
StructuredBuffer<uint> meshlet_vertices;

[[vk::binding(renderer::ResourceBinding::MeshletTriangles, 0)]]
StructuredBuffer<uint8_t> meshlet_triangles;

// TODO: not really worth it? not sure ...
#define CULLING 1

#if CULLING
groupshared float3 vertex_clip[renderer::MESHLET_MAX_VERTICES];
#endif

struct MeshVertex
{
    float4 position : SV_Position;
    [[vk::location(0)]] nointerpolation renderer::index_bundle_t  indices;
    [[vk::location(5)]] renderer::g_buffer_vertex_data_t          vertex_data;
};

[outputtopology("triangle")]
[numthreads(mesh_workgroup_x, 1, 1)]
[[shader("mesh")]]
void mesh_main(
    uint ti    : SV_GroupThreadID,
    uint wg_id : SV_GroupID,
    in payload renderer::mesh_task_payload_t task_payload,
    OutputVertices<MeshVertex, renderer::MESHLET_MAX_VERTICES> out_vertices,
    OutputIndices<uint3, renderer::MESHLET_MAX_TRIANGLES>      out_indices)
{
    uint mi = task_payload.meshlet_base_index + uint(task_payload.meshlet_delta_indices[wg_id]);

    uint vcount = uint(meshlets[mi].vertex_count);
    uint tcount = uint(meshlets[mi].triangle_count);
    SetMeshOutputCounts(vcount, tcount);

    renderer::matrix_struct_t m      = draws[task_payload.object_index].current_matrices;
    renderer::matrix_struct_t m_last = draws[task_payload.object_index].last_matrices;

    for (uint i = ti; i < vcount; i += mesh_workgroup_x)
    {
        uint vi = meshlet_vertices[meshlets[mi].vertex_offset + i];
        utils::Vertex v = utils::unpack(vertex_buffers[draws[task_payload.object_index].vertex_buffer_index][vi]);

        float3 world_pos = utils::apply_transform(m.transform, v.position);
        float4 cur_pos   = mul(camera_ubo.camera.projection,
                               mul(camera_ubo.camera.view, float4(world_pos, 1.0)));
        float4 last_pos  = mul(camera_ubo.last_camera.projection,
                               mul(camera_ubo.last_camera.view,
                                   float4(utils::apply_transform(m_last.transform, v.position), 1.0)));

        float4 jittered_position = cur_pos;
        jittered_position.xy += 2.0 * camera_ubo.camera.sample_offset * jittered_position.w;
        out_vertices[i].position = jittered_position;

        out_vertices[i].indices.mesh_draw_index = task_payload.object_index;
        out_vertices[i].indices.material_index  = draws[task_payload.object_index].material_index;
        out_vertices[i].indices.meshlet_index   = mi;
        out_vertices[i].indices.lod_index       = uint(draws[task_payload.object_index].lod_index);
        out_vertices[i].indices.lod_count       = uint(draws[task_payload.object_index].lod_count);

        out_vertices[i].vertex_data.current_position = cur_pos;
        out_vertices[i].vertex_data.last_position    = last_pos;
        out_vertices[i].vertex_data.tex_coord        = mul(m.texture, float4(v.tex_coord, 0, 1)).xy;
        out_vertices[i].vertex_data.normal           = utils::apply_rotation(m.transform, v.normal);
        out_vertices[i].vertex_data.tangent          = utils::apply_rotation(m.transform, v.tangent);

#if CULLING
        vertex_clip[i] = float3((jittered_position.xy / jittered_position.w * 0.5 + float2(0.5)) * context.size,
                              jittered_position.w);
#endif
    }

#if CULLING
    GroupMemoryBarrierWithGroupSync();
#endif

    for (uint i = ti; i < tcount; i += mesh_workgroup_x)
    {
        uint base_index = meshlets[mi].triangle_offset + 3 * i;
        uint a = uint(meshlet_triangles[base_index]), b = uint(meshlet_triangles[base_index + 1]),
                c = uint(meshlet_triangles[base_index + 2]);
        out_indices[i] = uint3(a, b, c);

        #if CULLING
        bool culled = false;

        float2 pa = vertex_clip[a].xy, pb = vertex_clip[b].xy, pc = vertex_clip[c].xy;

        // backface culling + zero-area culling
        float2 eb = pb - pa;
        float2 ec = pc - pa;

        culled = culled || (eb.x * ec.y >= eb.y * ec.x);

        // small primitive culling
        float2 bmin = min(pa, min(pb, pc));
        float2 bmax = max(pa, max(pb, pc));
        float sbprec = 1.0 / 256.0; // note: this can be set to 1/2^subpixelPrecisionBits

        // note: this is slightly imprecise (doesn't fully match hw behavior and is both too loose and too strict)
        culled = culled || (round(bmin.x - sbprec) == round(bmax.x) || round(bmin.y) == round(bmax.y + sbprec));

        // the computations above are only valid if all vertices are in front of perspective plane
        culled = culled && (vertex_clip[a].z > 0 && vertex_clip[b].z > 0 && vertex_clip[c].z > 0);

        // TODO: find slang-equivalent
        //gl_MeshPrimitivesEXT[i].gl_CullPrimitiveEXT = culled;
        #endif
    }
}
