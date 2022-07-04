#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable
//#extension GL_EXT_shader_8bit_storage: require
//#extension GL_EXT_shader_16bit_storage: require

#include "../renderer/types.glsl"
#include "../utils/camera.glsl"

vec3 slerp(vec3 x, vec3 y, float a)
{

    // get cosine of angle between vectors (-1 -> 1)
    float cos_alpha = dot(x, y);

    if (cos_alpha > 0.9999 || cos_alpha < -0.9999){ return a <= 0.5 ? x : y; }

    // get angle (0 -> pi)
    float alpha = acos(cos_alpha);

    // get sine of angle between vectors (0 -> 1)
    float sin_alpha = sin(alpha);

    // this breaks down when SinAlpha = 0, i.e. Alpha = 0 or pi
    float t1 = sin((1.0 - a) * alpha) / sin_alpha;
    float t2 = sin(a * alpha) / sin_alpha;

    // interpolate src vectors
    return x * t1 + y * t2;
}

//! Vertex defines the layout for a vertex-struct
struct Vertex
{
    vec3 position;
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
};

//! morph_params_t contains information to access a morph-target buffer
struct morph_params_t
{
    uint morph_count;
    uint base_vertex;
    uint vertex_count;
    float weights[61];
};

layout(set = 0, binding = BINDING_VERTICES, scalar) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(std140, binding = BINDING_MORPH_TARGETS, scalar) readonly buffer MorphVertices
{
    Vertex morph_vertices[];
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

layout(set = 0, binding = BINDING_MORPH_PARAMS, scalar) readonly buffer MorphParams
{
    morph_params_t morph_params;
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
    Vertex v = vertices[gl_VertexIndex];

    // apply morph-targets
    for(uint i = 0; i < morph_params.morph_count; ++i)
    {
        uint morph_index = morph_params.base_vertex + i * morph_params.vertex_count + (gl_VertexIndex - gl_BaseVertex);
        v.position += morph_vertices[morph_index].position * morph_params.weights[i];
        v.normal += morph_vertices[morph_index].normal * morph_params.weights[i];
    }
    v.normal = normalize(v.normal);

    object_index = gl_BaseInstance;//gl_BaseInstance + gl_InstanceIndex
    matrix_struct_t m = u_matrices[object_index];
    matrix_struct_t m_last = u_previous_matrices[object_index];

    vertex_out.current_position = camera.projection * camera.view * m.modelview * vec4(v.position, 1.0);
    vertex_out.last_position = last_camera.projection * last_camera.view * m_last.modelview * vec4(v.position, 1.0);

    vec4 jittered_position = vertex_out.current_position;
    jittered_position.xy += 2.0 * camera.sample_offset * jittered_position.w;
    gl_Position = jittered_position;

    vertex_out.tex_coord = (m.texture * vec4(v.tex_coord, 0, 1)).xy;
    vertex_out.normal = normalize(mat3(m.normal) * v.normal);
    vertex_out.tangent = normalize(mat3(m.normal) * v.tangent);
}