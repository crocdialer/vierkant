#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"
#include "../utils/camera.glsl"

layout (triangles, fractional_odd_spacing, cw) in;

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

layout(std140, binding = BINDING_JITTER_OFFSET) uniform UBOJitter
{
    camera_t camera;
    camera_t last_camera;
};

struct VertexData
{
    vec4 color;
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
    vec4 current_position;
    vec4 last_position;
};
layout(location = 0) flat in uint in_object_index[];
layout(location = 1) in VertexData vertex_in[];

layout(location = 0) flat out uint out_object_index;
layout(location = 1) out VertexData vertex_out;

VertexData interpolate_vertex()
{
    VertexData ret;
    ret.color = gl_TessCoord.x * vertex_in[0].color + gl_TessCoord.y * vertex_in[1].color + gl_TessCoord.z * vertex_in[2].color;
    ret.tex_coord = gl_TessCoord.x * vertex_in[0].tex_coord + gl_TessCoord.y * vertex_in[1].tex_coord + gl_TessCoord.z * vertex_in[2].tex_coord;
    ret.normal = gl_TessCoord.x * vertex_in[0].normal + gl_TessCoord.y * vertex_in[1].normal + gl_TessCoord.z * vertex_in[2].normal;
    ret.tangent = gl_TessCoord.x * vertex_in[0].tangent + gl_TessCoord.y * vertex_in[1].tangent + gl_TessCoord.z * vertex_in[2].tangent;
    ret.current_position = gl_TessCoord.x * vertex_in[0].current_position + gl_TessCoord.y * vertex_in[1].current_position + gl_TessCoord.z * vertex_in[2].current_position;
    ret.last_position = gl_TessCoord.x * vertex_in[0].last_position + gl_TessCoord.y * vertex_in[1].last_position + gl_TessCoord.z * vertex_in[2].last_position;
    return ret;
}

void main(void)
{
    gl_Position = (gl_TessCoord.x * gl_in[0].gl_Position) +
                  (gl_TessCoord.y * gl_in[1].gl_Position) +
                  (gl_TessCoord.z * gl_in[2].gl_Position);

    vertex_out = interpolate_vertex();

    uint object_index = in_object_index[0];
    matrix_struct_t m = u_matrices[object_index];
    matrix_struct_t m_last = u_previous_matrices[object_index];

    vertex_out.current_position = camera.projection * camera.view * m.modelview * gl_Position;
    vertex_out.last_position = last_camera.projection * last_camera.view * m_last.modelview * gl_Position;

    vec4 jittered_position = vertex_out.current_position;
    jittered_position.xy += 2.0 * camera.sample_offset * jittered_position.w;
    gl_Position = jittered_position;

    vertex_out.tex_coord = (m.texture * vec4(vertex_out.tex_coord, 0, 1)).xy;
    vertex_out.normal = normalize(mat3(m.normal) * vertex_out.normal);
    vertex_out.tangent = normalize(mat3(m.normal) * vertex_out.tangent);
}