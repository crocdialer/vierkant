#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"
#include "../utils/camera.glsl"

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, set = 0, binding = BINDING_MATRIX) readonly buffer MatrixBuffer
{
    matrix_struct_t u_matrices[];
};

layout(std140, set = 0, binding = BINDING_PREVIOUS_MATRIX) readonly buffer MatrixBufferPrevious
{
    matrix_struct_t u_previous_matrices[];
};

layout(std140, binding = BINDING_BONES) uniform UBOBones
{
    mat4 u_bones[MAX_NUM_BONES];
};

layout(std140, binding = BINDING_PREVIOUS_BONES) uniform UBOPreviousBones
{
    mat4 u_previous_bones[MAX_NUM_BONES];
};

layout(std140, binding = BINDING_JITTER_OFFSET) uniform UBOJitter
{
    camera_t camera;
    camera_t last_camera;
};

layout(location = ATTRIB_POSITION) in vec3 a_position;
layout(location = ATTRIB_COLOR) in vec4 a_color;
layout(location = ATTRIB_NORMAL) in vec3 a_normal;
layout(location = ATTRIB_BONE_INDICES) in uvec4 a_bone_ids;
layout(location = ATTRIB_BONE_WEIGHTS) in vec4 a_bone_weights;

layout(location = 0) flat out uint object_index;
layout(location = 1) out VertexData
{
    vec4 color;
    vec3 normal;

    vec4 current_position;
    vec4 last_position;
} vertex_out;

void main()
{
    object_index = gl_InstanceIndex;//gl_BaseInstance + gl_InstanceIndex
    matrix_struct_t m = u_matrices[object_index];
    matrix_struct_t m_last = u_previous_matrices[object_index];

    vec4 current_vertex = vec4(0);
    vec4 last_vertex = vec4(0);

    for (int i = 0; i < 4; i++)
    {
        current_vertex += u_bones[a_bone_ids[i]] * vec4(a_position, 1.0) * a_bone_weights[i];
        last_vertex += u_previous_bones[a_bone_ids[i]] * vec4(a_position, 1.0) * a_bone_weights[i];
    }
    vertex_out.color = a_color;
    vertex_out.normal = normalize(mat3(camera.view) * (m.normal * vec4(a_normal, 1.0)).xyz);

    vertex_out.current_position = camera.projection * camera.view * m.modelview * vec4(current_vertex.xyz, 1.0);
    vertex_out.last_position = last_camera.projection * last_camera.view * m_last.modelview * vec4(last_vertex.xyz, 1.0);

    vec4 jittered_position = vertex_out.current_position;
    jittered_position.xy += 2.0 * camera.sample_offset * jittered_position.w;
    gl_Position = jittered_position;
}