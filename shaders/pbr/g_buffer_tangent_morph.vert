#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2: require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#include "g_buffer_vertex_data.glsl"
#include "../renderer/types.glsl"
#include "../utils/packed_vertex.glsl"
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

layout(buffer_reference, scalar) readonly buffer VertexBufferPtr { packed_vertex_t v[]; };
layout(binding = BINDING_VERTICES, set = 0) readonly buffer Vertices { VertexBufferPtr vertex_buffers[]; };

layout(set = 0, binding = BINDING_DRAW_COMMANDS) readonly buffer DrawBuffer
{
    indexed_indirect_command_t draw_commands[];
};

layout(std140, binding = BINDING_MORPH_TARGETS, scalar) readonly buffer MorphVertices
{
    Vertex morph_vertices[];
};

layout(std430, set = 0, binding = BINDING_MESH_DRAWS) readonly buffer MeshDrawBuffer
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
    g_buffer_vertex_data_t vertex_out;
};

void main()
{
    const indexed_indirect_command_t draw_command = draw_commands[context.base_draw_index + gl_DrawID];
    indices.mesh_draw_index = draw_command.object_index;
    indices.material_index = draws[draw_command.object_index].material_index;
    matrix_struct_t m = draws[indices.mesh_draw_index].current_matrices;
    matrix_struct_t m_last = draws[indices.mesh_draw_index].last_matrices;

    // retrieve vertex-buffer, unpack vertex
    Vertex v = unpack(vertex_buffers[draws[indices.mesh_draw_index].vertex_buffer_index].v[gl_VertexIndex]);
    vec3 last_position = v.position;

    vec3 new_normal = vec3(0);
    vec3 new_tangent = vec3(0);

    // apply morph-targets
    for(uint i = 0; i < morph_params.morph_count; ++i)
    {
        uint morph_index = morph_params.base_vertex + i * morph_params.vertex_count + gl_VertexIndex - draw_command.vertexOffset;

        v.position += morph_vertices[morph_index].position * morph_params.weights[i];
        new_normal += slerp(v.normal, v.normal + morph_vertices[morph_index].normal, morph_params.weights[i]);
        new_tangent += slerp(v.tangent, v.tangent + morph_vertices[morph_index].tangent, morph_params.weights[i]);

        last_position += morph_vertices[morph_index].position * prev_morph_params.weights[i];
    }

    vertex_out.current_position = camera.projection * camera.view * vec4(apply_transform(m.transform, v.position), 1.0);
    vertex_out.last_position = last_camera.projection * last_camera.view * vec4(apply_transform(m_last.transform, last_position), 1.0);

    vec4 jittered_position = vertex_out.current_position;
    jittered_position.xy += 2.0 * camera.sample_offset * jittered_position.w;
    gl_Position = jittered_position;

    vertex_out.tex_coord = (m.texture * vec4(v.tex_coord, 0, 1)).xy;
    vertex_out.normal = normalize(apply_rotation(m.transform, new_normal));
    vertex_out.tangent = normalize(apply_rotation(m.transform, new_tangent));
}