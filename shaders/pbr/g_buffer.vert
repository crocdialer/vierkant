#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable
//#extension GL_EXT_shader_8bit_storage: require
//#extension GL_EXT_shader_16bit_storage: require

#include "../renderer/types.glsl"
#include "../utils/camera.glsl"

//! Vertex defines the layout for a vertex-struct
struct Vertex
{
    vec3 position;
    vec2 tex_coord;
    vec3 normal;
//    vec3 tangent;
};

layout(set = 0, binding = BINDING_VERTICES, scalar) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(std140, set = 0, binding = BINDING_MATRIX) readonly buffer MatrixBuffer
{
    matrix_struct_t u_matrices[];
};

layout(std140, set = 0, binding = BINDING_PREVIOUS_MATRIX) readonly buffer MatrixBufferPrevious
{
    matrix_struct_t u_previous_matrices[];
};

layout(std140, binding = BINDING_JITTER_OFFSET) uniform UBOJitter
{
    camera_t camera;
    camera_t last_camera;
};

layout(push_constant) uniform PushConstants
{
    render_context_t context;
};

layout(location = 0) flat out uint object_index;
layout(location = 1) out VertexData
{
    vec3 normal;

    vec4 current_position;
    vec4 last_position;
} vertex_out;

void main()
{
    Vertex v = vertices[gl_VertexIndex];

    object_index = gl_InstanceIndex;//gl_BaseInstance + gl_InstanceIndex
    matrix_struct_t m = u_matrices[object_index];
    matrix_struct_t m_last = u_previous_matrices[object_index];

    vertex_out.current_position = camera.projection * camera.view * m.modelview * vec4(v.position, 1.0);
    vertex_out.last_position = last_camera.projection * last_camera.view * m_last.modelview * vec4(v.position, 1.0);

    vec4 jittered_position = vertex_out.current_position;
    jittered_position.xy += 2.0 * camera.sample_offset * jittered_position.w;
    gl_Position = jittered_position;

    vertex_out.normal = normalize(mat3(camera.view) * (m.normal * vec4(v.normal, 1.0)).xyz);
}