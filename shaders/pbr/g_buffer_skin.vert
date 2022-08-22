#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"
#include "../utils/camera.glsl"

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, set = 0, binding = BINDING_MESH_DRAWS) readonly buffer MeshDrawBuffer
{
    mesh_draw_t draws[];
};

layout(std140, binding = BINDING_BONES) readonly buffer UBOBones
{
    mat4 u_bones[];
};

layout(std140, binding = BINDING_PREVIOUS_BONES) readonly buffer UBOPreviousBones
{
    mat4 u_previous_bones[];
};

layout(std140, binding = BINDING_JITTER_OFFSET) uniform UBOJitter
{
    camera_t camera;
    camera_t last_camera;
};

layout(location = ATTRIB_POSITION) in vec3 a_position;
layout(location = ATTRIB_NORMAL) in vec3 a_normal;
layout(location = ATTRIB_BONE_INDICES) in uvec4 a_bone_ids;
layout(location = ATTRIB_BONE_WEIGHTS) in vec4 a_bone_weights;

layout(location = LOCATION_INDEX_BUNDLE) flat out index_bundle_t indices;
layout(location = LOCATION_VERTEX_BUNDLE) out VertexData
{
    vec3 normal;
    vec4 current_position;
    vec4 last_position;
} vertex_out;

void main()
{
    indices.mesh_draw_index = gl_BaseInstance;//gl_BaseInstance + gl_InstanceIndex
    matrix_struct_t m = draws[indices.mesh_draw_index].current_matrices;
    matrix_struct_t m_last = draws[indices.mesh_draw_index].last_matrices;

    vec4 current_vertex = vec4(0);
    vec4 last_vertex = vec4(0);

    for (int i = 0; i < 4; i++)
    {
        current_vertex += u_bones[a_bone_ids[i]] * vec4(a_position, 1.0) * a_bone_weights[i];
        last_vertex += u_previous_bones[a_bone_ids[i]] * vec4(a_position, 1.0) * a_bone_weights[i];
    }
    vertex_out.normal = normalize((m.normal * vec4(a_normal, 0)).xyz);

    vertex_out.current_position = camera.projection * camera.view * m.modelview * vec4(current_vertex.xyz, 1.0);
    vertex_out.last_position = last_camera.projection * last_camera.view * m_last.modelview * vec4(last_vertex.xyz, 1.0);

    vec4 jittered_position = vertex_out.current_position;
    jittered_position.xy += 2.0 * camera.sample_offset * jittered_position.w;
    gl_Position = jittered_position;
}