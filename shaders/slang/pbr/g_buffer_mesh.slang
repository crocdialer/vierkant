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

// ---- mesh shader -----------------------------------------------------------
groupshared renderer::mesh_task_payload_t task_payload;

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
    OutputVertices<MeshVertex, renderer::MESHLET_MAX_VERTICES> verts,
    OutputIndices<uint3, renderer::MESHLET_MAX_TRIANGLES>      inds)
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

        float4 jittered = cur_pos;
        jittered.xy += 2.0 * camera_ubo.camera.sample_offset * jittered.w;
        verts[i].position = jittered;

        verts[i].indices.mesh_draw_index = task_payload.object_index;
        verts[i].indices.material_index  = draws[task_payload.object_index].material_index;
        verts[i].indices.meshlet_index   = mi;
        verts[i].indices.lod_index       = uint(draws[task_payload.object_index].lod_index);
        verts[i].indices.lod_count       = uint(draws[task_payload.object_index].lod_count);

        verts[i].vertex_data.current_position = cur_pos;
        verts[i].vertex_data.last_position    = last_pos;
        verts[i].vertex_data.tex_coord        = mul(m.texture, float4(v.tex_coord, 0, 1)).xy;
        verts[i].vertex_data.normal           = utils::apply_rotation(m.transform, v.normal);
        verts[i].vertex_data.tangent          = utils::apply_rotation(m.transform, v.tangent);
    }

    for (uint i = ti; i < tcount; i += mesh_workgroup_x)
    {
        uint base = meshlets[mi].triangle_offset + 3 * i;
        inds[i] = uint3(meshlet_triangles[base], meshlet_triangles[base + 1], meshlet_triangles[base + 2]);
    }
}
