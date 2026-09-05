import "../renderer/types.slang";
import "../utils/transform.slang";
import "../utils/packed_vertex.slang";
import "../utils/camera.slang";
import "../utils/project_sphere.slang";

// Specialization-constant IDs mirror the GLSL constant_id assignments
[[vk::constant_id(1)]]
const uint mesh_workgroup_x = renderer::MAX_MESH_WORKGROUP_SIZE;

// ---- shared bindings (set 0) -----------------------------------------------

[[vk::push_constant]]
renderer::push_constants_t push_constants;

[[vk::binding(renderer::ResourceBinding::RenderData, 0)]]
ConstantBuffer<renderer::render_data_t> render_data;

struct camera_ubo_t
{
    utils::camera_t camera;
    utils::camera_t last_camera;
};
[[vk::binding(renderer::ResourceBinding::JitterOffset, 0)]]
ConstantBuffer<camera_ubo_t> camera_ubo;

// mesh-shader-only bindings
// TODO: not really worth it? not sure ...
#define CULLING 0

#if CULLING
groupshared float3 vertex_clip[renderer::MESHLET_MAX_VERTICES];
#endif

struct vertex_data_t
{
    float4 position : SV_Position;

    [[vk::location(0)]]
    nointerpolation renderer::index_bundle_t indices;

    [[vk::location(5)]]
    renderer::g_buffer_vertex_data_t g_buffer_data;
};

#if CULLING
// per-primitive attributes (connectivity goes into OutputIndices<uint3, N>)
struct primitive_data_t
{
    bool cull_primitive : SV_CullPrimitive;

    // NOTE: add when we actually need it
    //uint primitive_id : SV_PrimitiveID;
};
#endif

[outputtopology("triangle")]
[numthreads(mesh_workgroup_x, 1, 1)]
[[shader("mesh")]]
void mesh_main(uint ti: SV_GroupThreadID, uint wg_id: SV_GroupID, in payload renderer::mesh_task_payload_t task_payload,
               OutputVertices<vertex_data_t, renderer::MESHLET_MAX_VERTICES> out_vertices,
               OutputIndices<uint3, renderer::MESHLET_MAX_TRIANGLES> out_indices
#if CULLING
               ,
               OutputPrimitives<primitive_data_t, renderer::MESHLET_MAX_TRIANGLES> out_primitives
#endif
)
{
    uint mi = task_payload.meshlet_base_index + uint(task_payload.meshlet_delta_indices[wg_id]);

    renderer::mesh_draw_t draw = render_data.mesh_draws[task_payload.object_index];
    renderer::mesh_buffers_t mesh_buffers = render_data.mesh_buffers[draw.mesh_buffer_index];

    uint vertex_count = uint(mesh_buffers.meshlets[mi].vertex_count);
    uint primitive_count = uint(mesh_buffers.meshlets[mi].triangle_count);
    SetMeshOutputCounts(vertex_count, primitive_count);

    renderer::matrix_struct_t m = render_data.mesh_draws[task_payload.object_index].current_matrices;
    renderer::matrix_struct_t m_last = render_data.mesh_draws[task_payload.object_index].last_matrices;

    for(uint i = ti; i < vertex_count; i += mesh_workgroup_x)
    {
        uint vi = mesh_buffers.meshlet_vertices[mesh_buffers.meshlets[mi].vertex_offset + i];
        utils::Vertex v = utils::unpack(mesh_buffers.vertices[vi]);

        float3 world_pos = utils::apply_transform(m.transform, v.position);
        float4 cur_pos = mul(camera_ubo.camera.projection, mul(camera_ubo.camera.view, float4(world_pos, 1.0)));
        float4 last_pos = mul(
                camera_ubo.last_camera.projection,
                mul(camera_ubo.last_camera.view, float4(utils::apply_transform(m_last.transform, v.position), 1.0)));

        float4 jittered_position = cur_pos;
        jittered_position.xy += 2.0 * camera_ubo.camera.sample_offset * jittered_position.w;
        out_vertices[i].position = jittered_position;

        out_vertices[i].indices.mesh_draw_index = task_payload.object_index;
        out_vertices[i].indices.material_index = render_data.mesh_draws[task_payload.object_index].material_index;
        out_vertices[i].indices.meshlet_index = mi;
        out_vertices[i].indices.lod_index = uint(render_data.mesh_draws[task_payload.object_index].lod_index);
        out_vertices[i].indices.lod_count = uint(render_data.mesh_draws[task_payload.object_index].lod_count);

        out_vertices[i].g_buffer_data.current_position = cur_pos;
        out_vertices[i].g_buffer_data.last_position = last_pos;
        out_vertices[i].g_buffer_data.tex_coord = mul(m.texture, float4(v.tex_coord, 0, 1)).xy;
        out_vertices[i].g_buffer_data.normal = utils::apply_rotation(m.transform, v.normal);
        out_vertices[i].g_buffer_data.tangent =
                float4(utils::apply_rotation(m.transform, v.tangent.xyz), v.tangent.w);

#if CULLING
        vertex_clip[i] = float3((jittered_position.xy / jittered_position.w * 0.5 + float2(0.5)) * push_constants.size,
                                jittered_position.w);
#endif
    }

#if CULLING
    GroupMemoryBarrierWithGroupSync();
#endif

    for(uint i = ti; i < primitive_count; i += mesh_workgroup_x)
    {
        uint base_index = mesh_buffers.meshlets[mi].triangle_offset + 3 * i;
        uint a = uint(mesh_buffers.meshlet_triangles[base_index]),
             b = uint(mesh_buffers.meshlet_triangles[base_index + 1]),
             c = uint(mesh_buffers.meshlet_triangles[base_index + 2]);
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
        float sbprec = 1.0 / 256.0;// note: this can be set to 1/2^subpixelPrecisionBits

        // note: this is slightly imprecise (doesn't fully match hw behavior and is both too loose and too strict)
        culled = culled || (round(bmin.x - sbprec) == round(bmax.x) || round(bmin.y) == round(bmax.y + sbprec));

        // the computations above are only valid if all vertices are in front of perspective plane
        culled = culled && (vertex_clip[a].z > 0 && vertex_clip[b].z > 0 && vertex_clip[c].z > 0);

        out_primitives[i].cull_primitive = culled;
#endif
    }
}
