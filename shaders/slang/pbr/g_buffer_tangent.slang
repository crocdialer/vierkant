import "../renderer/types.slang";
import "../utils/transform.slang";
import "../utils/packed_vertex.slang";
import "../utils/camera.slang";

[[vk::push_constant]]
renderer::context_t context;

[[vk::binding(renderer::ResourceBinding::Vertices, 0)]]
StructuredBuffer<Ptr<utils::packed_vertex_t, Access.Read>> vertex_buffers;

[[vk::binding(renderer::ResourceBinding::MeshDraws, 0)]]
StructuredBuffer<renderer::mesh_draw_t> draws;

struct camera_ubo_t
{
    utils::camera_t camera;
    utils::camera_t last_camera;
};
[[vk::binding(renderer::ResourceBinding::JitterOffset, 0)]]
ConstantBuffer<camera_ubo_t> camera_ubo;

[[shader("vertex")]]
    renderer::g_buffer_vertex_data_t vertex_main(
    uint vertex_id: SV_VertexID,
    uint base_vertex: SV_StartVertexLocation,
    uint instance_id: SV_StartInstanceLocation,
    nointerpolation out renderer::index_bundle_t indices,
    out float4 sv_position: SV_Position)
{
    indices.mesh_draw_index = instance_id;
    indices.material_index = draws[instance_id].material_index;
    indices.meshlet_index = 0;
    indices.lod_index = draws[instance_id].lod_index;
    indices.lod_count = draws[instance_id].lod_count;

    renderer::matrix_struct_t m = draws[indices.mesh_draw_index].current_matrices;
    renderer::matrix_struct_t m_last = draws[indices.mesh_draw_index].last_matrices;

    utils::Vertex v = utils::unpack(vertex_buffers[draws[indices.mesh_draw_index].vertex_buffer_index][vertex_id + base_vertex]);

    renderer::g_buffer_vertex_data_t vertex_out;
    float3 world_pos = utils::apply_transform(m.transform, v.position);
    vertex_out.current_position = mul(camera_ubo.camera.projection, mul(camera_ubo.camera.view, float4(world_pos, 1.0)));
    float3 last_world_pos = utils::apply_transform(m_last.transform, v.position);
    vertex_out.last_position = mul(camera_ubo.last_camera.projection, mul(camera_ubo.last_camera.view, float4(last_world_pos, 1.0)));

    float4 jittered_position = vertex_out.current_position;
    jittered_position.xy += 2.0 * camera_ubo.camera.sample_offset * jittered_position.w;
    sv_position = jittered_position;

    vertex_out.tex_coord = mul(m.texture, float4(v.tex_coord, 0, 1)).xy;
    vertex_out.normal = utils::apply_rotation(m.transform, v.normal);
    vertex_out.tangent = utils::apply_rotation(m.transform, v.tangent);

    return vertex_out;
}
