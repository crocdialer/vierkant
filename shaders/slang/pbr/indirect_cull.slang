import "../renderer/types.slang";
import "../utils/camera.slang";
import "../utils/project_sphere.slang";
import "../utils/transform.slang";

[[vk::binding(0, 0)]] Sampler2D u_depth_pyramid;

struct draw_cull_result_t
{
    uint draw_count;
    uint num_visible;
    uint num_frustum_culled;
    uint num_occlusion_culled;
    uint num_contribution_culled;
    uint num_triangles;
    uint num_meshlets;
};

struct draw_cull_data_t
{
    float4x4 view;

    float P00, P11, znear, zfar;
    float4 frustum;

    renderer::indexed_draw_command_t *draws_in;
    renderer::indexed_draw_command_t *draws_in_post;
    renderer::mesh_draw_t            *mesh_draws_in;
    Ptr<renderer::mesh_entry_t, Access.Read> mesh_entries_in;
    renderer::indexed_draw_command_t *draws_out_pre;
    renderer::indexed_draw_command_t *draws_out_post;
    uint                             *draw_count_pre;
    uint                             *draw_count_post;
    Ptr<uint, Access.Read>            draw_command_indices_in_post;
    draw_cull_result_t               *draw_result;

    uint num_draws;
    float lod_base;
    float lod_step;
    uint max_num_lods;
    float2 pyramid_size;

    bool ortho;
    bool frustum_cull;
    bool occlusion_cull;
    bool contribution_cull;
    bool use_meshlets;
    bool lod_enabled;

    uint task_workgroup_size;
};

[[vk::binding(1, 0)]] ConstantBuffer<draw_cull_data_t> cull_data_ubo;

uint div_up(uint nom, uint denom) { return (nom + denom - 1) / denom; }

[shader("compute")]
[numthreads(32, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    draw_cull_data_t cull_data = cull_data_ubo;

    uint gid = dispatchThreadID.x;
    if(gid >= cull_data.num_draws) { return; }

    renderer::indexed_draw_command_t draw = cull_data.draws_in[gid];
    InterlockedAdd(cull_data.draw_result[0].draw_count, 1u);

    renderer::mesh_draw_t mesh_draw = cull_data.mesh_draws_in[draw.object_index];
    Ptr<renderer::mesh_entry_t, Access.Read> mesh_entry = cull_data.mesh_entries_in + mesh_draw.mesh_index;

    float3 center = mul(cull_data.view, float4(utils::apply_transform(mesh_draw.current_matrices.transform, mesh_entry->center), 1.0)).xyz;
    float radius = mesh_entry->radius * max(max(abs(mesh_draw.current_matrices.transform.scale_x),
                                               abs(mesh_draw.current_matrices.transform.scale_y)),
                                           abs(mesh_draw.current_matrices.transform.scale_z));

    bool visible = renderer::has_flag(uint(draw.flags), uint(renderer::DrawCommandFlags::Enabled));
    bool meshlet_draw = cull_data.use_meshlets && renderer::has_flag(uint(draw.flags), uint(renderer::DrawCommandFlags::Meshlets));

    float4 aabb = float4(0, 0, 1, 1);
    bool sphere_visible = utils::project_sphere(center, radius, cull_data.znear, cull_data.P00, cull_data.P11, aabb);

    if(cull_data.ortho)
    {
        sphere_visible = utils::project_sphere_ortho(center, radius, cull_data.znear, cull_data.frustum, aabb);
    }

    float2 screen_area = aabb.zw - aabb.xy;

    if(visible && cull_data.frustum_cull)
    {
        bool inside_frustum = cull_data.ortho ? !utils::frustum_cull_ortho(center, radius, cull_data.frustum) :
                                                !utils::frustum_cull(center, radius, cull_data.frustum);
        visible = visible && inside_frustum;

        if(!visible) { InterlockedAdd(cull_data.draw_result[0].num_frustum_culled, 1u); }
    }

    if(visible && cull_data.contribution_cull && sphere_visible)
    {
        const float size_thresh = 1.0 / cull_data.pyramid_size.x;
        visible = visible && max(screen_area.x, screen_area.y) >= size_thresh;

        if(!visible) { InterlockedAdd(cull_data.draw_result[0].num_contribution_culled, 1u); }
    }

    if(visible && cull_data.occlusion_cull && sphere_visible)
    {
        float width = screen_area.x * cull_data.pyramid_size.x;
        float height = screen_area.y * cull_data.pyramid_size.y;

        float level = floor(log2(max(width, height)));

        float depth = u_depth_pyramid.SampleLevel((aabb.xy + aabb.zw) * 0.5, level).x;
        depth = cull_data.ortho ? lerp(cull_data.zfar, cull_data.znear, depth) :
                                  clamp(cull_data.znear / depth, cull_data.znear, cull_data.zfar);
        float depth_sphere = -center.z - radius;

        visible = depth_sphere <= depth;
        if(!visible) { InterlockedAdd(cull_data.draw_result[0].num_occlusion_culled, 1u); }
    }

    if(visible && cull_data.lod_enabled)
    {
        float aabb_area = (aabb.z - aabb.x) * (aabb.w - aabb.y);
        float lod_index_f = log2(cull_data.lod_base / aabb_area) / log2(cull_data.lod_step);

        uint max_lod_index = min(mesh_entry->lod_count, renderer::MAX_NUM_MESH_LODS) - 1;
        max_lod_index = cull_data.max_num_lods > 0 ? min(max_lod_index, cull_data.max_num_lods - 1) : max_lod_index;
        uint lod_index = sphere_visible ? clamp(uint(lod_index_f + 1), 0u, max_lod_index) : 0u;

        cull_data.mesh_draws_in[draw.object_index].lod_index = uint16_t(lod_index);
        cull_data.mesh_draws_in[draw.object_index].lod_count = uint16_t(mesh_entry->lod_count);

        renderer::lod_t lod = mesh_entry->lods[lod_index];

        draw.vertex_offset = int(mesh_entry->vertex_offset);
        draw.index_count   = lod.num_indices;
        draw.first_index   = lod.base_index;
        draw.group_count_x = div_up(lod.num_meshlets, cull_data.task_workgroup_size);
        draw.group_count_y = 1;
        draw.group_count_z = 1;
        draw.base_meshlet  = lod.base_meshlet;
        draw.num_meshlets  = lod.num_meshlets;
    }

    if(visible)
    {
        InterlockedAdd(cull_data.draw_result->num_visible, 1u);
        InterlockedAdd(cull_data.draw_result->num_triangles, draw.index_count / 3u);
        InterlockedAdd(cull_data.draw_result->num_meshlets, draw.num_meshlets);

        bool needs_post_draw = !renderer::has_flag(uint(draw.flags), uint(renderer::DrawCommandFlags::Visible));

        uint draw_cmd_offset; InterlockedAdd(cull_data.draw_count_pre[draw.count_buffer_offset], 1u, draw_cmd_offset);
        cull_data.draws_out_pre[draw.first_draw_index + draw_cmd_offset] = draw;

        if(needs_post_draw || meshlet_draw)
        {
            uint draw_flags = uint(draw.flags);
            renderer::set_flag(draw_flags, uint(renderer::DrawCommandFlags::LateVisible),
                               needs_post_draw || (meshlet_draw && (draw.base_meshlet != cull_data.draws_in[gid].base_meshlet)));
            draw.flags = renderer::DrawCommandFlags(draw_flags);

            uint draw_cmd_index_post = cull_data.draw_command_indices_in_post[draw.object_index];

            let post_cmd = cull_data.draws_in_post + draw_cmd_index_post;
            draw.count_buffer_offset = post_cmd->count_buffer_offset;
            draw.first_draw_index    = post_cmd->first_draw_index;

            uint draw_cmd_offset_post; InterlockedAdd(cull_data.draw_count_post[draw.count_buffer_offset], 1u, draw_cmd_offset_post);
            cull_data.draws_out_post[draw.first_draw_index + draw_cmd_offset_post] = draw;
        }
    }

    uint vis_flags = uint(cull_data.draws_in[gid].flags);
    renderer::set_flag(vis_flags, uint(renderer::DrawCommandFlags::Visible), visible);
    cull_data.draws_in[gid].flags = renderer::DrawCommandFlags(vis_flags);
    cull_data.draws_in[gid].base_meshlet = draw.base_meshlet;
}
