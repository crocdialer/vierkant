#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "../renderer/types.glsl"
#include "../renderer/packed_vertex.glsl"
#include "../utils/camera.glsl"
#include "../utils/slerp.glsl"

//! morph_params_t contains information to access a morph-target buffer
struct morph_params_t
{
    uint morph_count;
    uint base_vertex;
    uint vertex_count;
    float weights[61];
};

layout(set = 0, binding = BINDING_DRAW_COMMANDS) readonly buffer DrawBuffer
{
    indexed_indirect_command_t draw_commands[];
};

layout(set = 0, binding = BINDING_VERTICES, scalar) readonly buffer VertexBuffer
{
    packed_vertex_t vertices[];
};

layout(std140, binding = BINDING_MORPH_TARGETS, scalar) readonly buffer MorphVertices
{
    Vertex morph_vertices[];
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

layout(set = 0, binding = BINDING_MORPH_PARAMS) readonly buffer MorphParams
{
    morph_params_t morph_params;
};

layout(set = 0, binding = BINDING_PREVIOUS_MORPH_PARAMS) readonly buffer PreviousMorphParams
{
    morph_params_t prev_morph_params;
};

layout(push_constant) uniform PushConstants
{
    render_context_t context;
};

layout(location = LOCATION_INDEX_BUNDLE) flat out index_bundle_t indices;
layout(location = LOCATION_VERTEX_BUNDLE) out VertexData
{
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
    vec4 current_position;
    vec4 last_position;
} vertex_out;

void main()
{
    const indexed_indirect_command_t draw_command = draw_commands[context.base_draw_index + gl_DrawID];
    Vertex v = unpack(vertices[gl_VertexIndex]);

    Vertex current_vertex;
    current_vertex.position = v.position;
    vec3 last_position = v.position;

    // apply morph-targets
    for(uint i = 0; i < morph_params.morph_count; ++i)
    {
        uint morph_index = morph_params.base_vertex + i * morph_params.vertex_count + gl_VertexIndex - draw_command.vertexOffset;

        current_vertex.position += morph_vertices[morph_index].position * morph_params.weights[i];
        current_vertex.normal = slerp(v.normal, v.normal + morph_vertices[morph_index].normal, morph_params.weights[i]);
        current_vertex.tangent = slerp(v.tangent, v.tangent + morph_vertices[morph_index].normal, morph_params.weights[i]);

        last_position += morph_vertices[morph_index].position * prev_morph_params.weights[i];
    }
    current_vertex.normal = normalize(current_vertex.normal);

    indices.mesh_draw_index = draw_command.object_index;
    indices.material_index = draw_command.object_index;
    matrix_struct_t m = draws[indices.mesh_draw_index].current_matrices;
    matrix_struct_t m_last = draws[indices.mesh_draw_index].last_matrices;

    vertex_out.current_position = camera.projection * camera.view * m.modelview * vec4(current_vertex.position, 1.0);
    vertex_out.last_position = last_camera.projection * last_camera.view * m_last.modelview * vec4(last_position, 1.0);

    vec4 jittered_position = vertex_out.current_position;
    jittered_position.xy += 2.0 * camera.sample_offset * jittered_position.w;
    gl_Position = jittered_position;

    vertex_out.tex_coord = (m.texture * vec4(v.tex_coord, 0, 1)).xy;
    vertex_out.normal = normalize(mat3(m.normal) * current_vertex.normal);
    vertex_out.tangent = normalize(mat3(m.normal) * current_vertex.tangent);
}