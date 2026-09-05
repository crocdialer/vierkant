import "../renderer/types.slang";
import "../utils/transform.slang";
import "../utils/packed_vertex.slang";
import "../utils/camera.slang";

[[vk::binding(renderer::ResourceBinding::RenderData, 0)]]
ConstantBuffer<renderer::render_data_t> render_data;

[[shader("vertex")]]
renderer::g_buffer_vertex_data_t vertex_main(uint vertex_id: SV_VertexID, uint base_vertex: SV_StartVertexLocation,
                                             uint instance_id: SV_StartInstanceLocation,
                                             nointerpolation out renderer::index_bundle_t indices,
                                             out float4 sv_position: SV_Position)
{
    indices.mesh_draw_index = instance_id;
    indices.material_index = render_data.mesh_draws[instance_id].material_index;
    indices.meshlet_index = 0;
    indices.lod_index = render_data.mesh_draws[instance_id].lod_index;
    indices.lod_count = render_data.mesh_draws[instance_id].lod_count;

    renderer::matrix_struct_t m = render_data.mesh_draws[indices.mesh_draw_index].current_matrices;
    renderer::matrix_struct_t m_last = render_data.mesh_draws[indices.mesh_draw_index].last_matrices;

    utils::Vertex v =
            utils::unpack(render_data.mesh_buffers[render_data.mesh_draws[indices.mesh_draw_index].mesh_buffer_index]
                                  .vertices[vertex_id + base_vertex]);

    renderer::g_buffer_vertex_data_t vertex_out;
    float3 world_pos = utils::apply_transform(m.transform, v.position);
    vertex_out.current_position =
            mul(render_data.cameras->camera.projection, mul(render_data.cameras->camera.view, float4(world_pos, 1.0)));
    float3 last_world_pos = utils::apply_transform(m_last.transform, v.position);
    vertex_out.last_position =
            mul(render_data.cameras->last_camera.projection, mul(render_data.cameras->last_camera.view, float4(last_world_pos, 1.0)));

    float4 jittered_position = vertex_out.current_position;
    jittered_position.xy += 2.0 * render_data.cameras->camera.sample_offset * jittered_position.w;
    sv_position = jittered_position;

    vertex_out.tex_coord = mul(m.texture, float4(v.tex_coord, 0, 1)).xy;
    vertex_out.normal = utils::apply_rotation(m.transform, v.normal);
    vertex_out.tangent = float4(utils::apply_rotation(m.transform, v.tangent.xyz), v.tangent.w);

    return vertex_out;
}
