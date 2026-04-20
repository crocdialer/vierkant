import "../renderer/types.slang";
import "../utils/transform.slang";
import "../utils/packed_vertex.slang";
import "../utils/camera.slang";
import "../utils/project_sphere.slang";

[[vk::constant_id(0)]] const uint task_workgroup_x = 128;
[[vk::constant_id(2)]] const bool use_culling = false;
[[vk::constant_id(3)]] const bool post_pass = false;

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

// task-shader-only bindings
[[vk::binding(renderer::ResourceBinding::Material, 0)]]
StructuredBuffer<renderer::material_t> materials;

[[vk::binding(renderer::ResourceBinding::DepthPyramid, 0)]]
Sampler2D u_depth_pyramid;

// ---- task-shader helpers ---------------------------------------------------

bool cone_cull(float3 center, float radius, float3 cone_axis, float cone_cutoff, float3 camera_position)
{
    return dot(center - camera_position, cone_axis) >= cone_cutoff * length(center - camera_position) + radius;
}

bool is_visible(uint visibility_base_index, uint meshlet_index)
{
    return (meshlet_visibilities[visibility_base_index + (meshlet_index >> 5u)] & (1u << (meshlet_index & 31u))) != 0;
}

void set_visible(uint base, uint meshlet_index, bool visible)
{
    uint bit  = 1u << (meshlet_index & 31u);
    uint slot = base + (meshlet_index >> 5u);

    if(visible)
    {
        InterlockedOr(meshlet_visibilities[slot], bit);
    }
    else
    {
        InterlockedAnd(meshlet_visibilities[slot], ~bit);
    }
}

// ---- task (amplification) shader -------------------------------------------

groupshared uint    shared_count;
groupshared renderer::mesh_task_payload_t task_payload;

[numthreads(task_workgroup_x, 1, 1)]
[[shader("amplification")]]
void task_main(
    uint3 dispatchThreadID: SV_DispatchThreadID,
    uint ti      : SV_GroupThreadID,
    uint wg_id   : SV_GroupID,
    uint draw_id : SV_DrawIndex)
{
    uint gid = dispatchThreadID.x;//wg_id * task_workgroup_x + ti;

    // init shared memory
    if (gid == 0) { shared_count = 0; }
    GroupMemoryBarrierWithGroupSync();

    renderer::indexed_draw_command_t draw_command = draw_commands[context.base_draw_index + draw_id];
    bool valid = gid < draw_command.num_meshlets;

    uint meshlet_base_index = wg_id * WorkgroupSize().x + draw_command.base_meshlet;
    uint mi                 = meshlet_base_index + ti;

    //mesh_task_payload_t task_payload;
    task_payload.object_index       = draw_command.object_index;
    task_payload.meshlet_base_index = meshlet_base_index;

    uint dispatch_count;

    if (use_culling)
    {
        bool visible          = valid;
        bool visible_last_frame = is_visible(draw_command.meshlet_visibility_index, gid);
        bool late_visible     = renderer::has_flag((uint)draw_command.flags,
                                                   (uint)renderer::DrawCommandFlags::LateVisible);

        if (post_pass && valid)
        {
            renderer::mesh_draw_t draw = draws[draw_command.object_index];
            float4x4 m = mul(camera_ubo.camera.view, utils::mat4_cast(draw.current_matrices.transform));

            float3 cone_axis_raw = float3(meshlets[mi].cone_axis[0],
                                          meshlets[mi].cone_axis[1],
                                          meshlets[mi].cone_axis[2]) / 127.0;
            cone_axis_raw        = normalize(mul((float3x3)m, cone_axis_raw));
            float  cone_cutoff   = float(meshlets[mi].cone_cutoff) / 127.0;

            float3 center = mul(m, float4(meshlets[mi].sphere_center, 1.0)).xyz;
            float3 s      = draw.current_matrices.transform.scale();
            float  radius = meshlets[mi].sphere_radius * max(max(abs(s.x), abs(s.y)), abs(s.z));

            visible = materials[draw.material_index].two_sided ||
                      !cone_cull(center, radius, cone_axis_raw, cone_cutoff, float3(0));

            bool inside_frustum = camera_ubo.camera.ortho
                                ? !utils::frustum_cull_ortho(center, radius, camera_ubo.camera.frustum)
                                : !utils::frustum_cull(center, radius, camera_ubo.camera.frustum);
            visible = visible && inside_frustum;

            float4 aabb;
            bool sphere_visible = !camera_ubo.camera.ortho &&
                                   utils::project_sphere(center, radius, camera_ubo.camera.near,
                                                         camera_ubo.camera.projection[0][0],
                                                         camera_ubo.camera.projection[1][1], aabb);
            if (camera_ubo.camera.ortho)
                sphere_visible = utils::project_sphere_ortho(center, radius, camera_ubo.camera.near,
                                                              camera_ubo.camera.frustum, aabb);

            uint pw, ph;
            u_depth_pyramid.GetDimensions(pw, ph);
            float2 pyramid_size = float2(pw, ph);
            float2 screen_area  = aabb.zw - aabb.xy;

            if (visible && sphere_visible)
            {
                float size_thresh = 1.0 / pyramid_size.x;
                visible = max(screen_area.x, screen_area.y) >= size_thresh;
            }

            if (visible && sphere_visible)
            {
                float width  = screen_area.x * pyramid_size.x;
                float height = screen_area.y * pyramid_size.y;
                float level  = floor(log2(max(width, height)));

                float depth = u_depth_pyramid.SampleLevel((aabb.xy + aabb.zw) * 0.5, level).x;
                depth = camera_ubo.camera.ortho
                      ? lerp(camera_ubo.camera.far, camera_ubo.camera.near, depth)
                      : clamp(camera_ubo.camera.near / depth, camera_ubo.camera.near, camera_ubo.camera.far);
                visible = (-center.z - radius) <= depth;
            }

            bool needs_post_draw = visible && (late_visible || !visible_last_frame);
            set_visible(draw_command.meshlet_visibility_index, gid, visible);
            visible = needs_post_draw;
        }
        else
        {
            visible = visible && (visible_last_frame && !late_visible);
        }

        // write payload delta-index
        if (visible)
        {
            uint index;
            InterlockedAdd(shared_count, 1u, index);
            task_payload.meshlet_delta_indices[index] = uint8_t(ti);
        }
        GroupMemoryBarrierWithGroupSync();
        dispatch_count = shared_count;
    }
    else
    {
        task_payload.meshlet_delta_indices[ti] = uint8_t(ti);
        dispatch_count = min(draw_command.num_meshlets - wg_id * task_workgroup_x, task_workgroup_x);
    }

    DispatchMesh(dispatch_count, 1, 1, task_payload);
}
