#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "../renderer/types.glsl"
#include "../utils/camera.glsl"

//! Vertex defines the layout for a vertex-struct
struct Vertex
{
    vec3 position;
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
};

layout(set = 0, binding = BINDING_VERTICES, scalar) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(std140, set = 0, binding = BINDING_MESH_DRAWS) readonly buffer MeshDrawBuffer
{
    mesh_draw_t draws[];
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
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
    vec4 current_position;
    vec4 last_position;
} vertex_out;

void main()
{
    const Vertex v = vertices[gl_VertexIndex];

    object_index = gl_BaseInstance;
    matrix_struct_t m = draws[object_index].current_matrices;
    matrix_struct_t m_last = draws[object_index].last_matrices;

    vertex_out.current_position = camera.projection * camera.view * m.modelview * vec4(v.position, 1.0);
    vertex_out.last_position = last_camera.projection * last_camera.view * m_last.modelview * vec4(v.position, 1.0);

    vec4 jittered_position = vertex_out.current_position;
    jittered_position.xy += 2.0 * camera.sample_offset * jittered_position.w;
    gl_Position = jittered_position;

    vertex_out.tex_coord = (m.texture * vec4(v.tex_coord, 0, 1)).xy;
    vertex_out.normal = normalize(mat3(m.normal) * v.normal);
    vertex_out.tangent = normalize(mat3(m.normal) * v.tangent);
}