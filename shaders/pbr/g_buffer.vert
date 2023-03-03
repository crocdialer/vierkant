#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "../renderer/types.glsl"
#include "../utils/packed_vertex.glsl"
#include "../utils/camera.glsl"

layout(set = 0, binding = BINDING_VERTICES, scalar) readonly buffer VertexBuffer
{
    packed_vertex_t vertices[];
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
    Vertex v = unpack(vertices[gl_VertexIndex]);

    indices.mesh_draw_index = gl_BaseInstance;//gl_BaseInstance + gl_InstanceIndex
    indices.material_index = draws[gl_BaseInstance].material_index;
    indices.meshlet_index = 0;

    matrix_struct_t m = draws[indices.mesh_draw_index].current_matrices;
    matrix_struct_t m_last = draws[indices.mesh_draw_index].last_matrices;

    vertex_out.current_position = camera.projection * camera.view * vec4(apply_transform(m.transform, v.position), 1.0);
    vertex_out.last_position = last_camera.projection * last_camera.view * vec4(apply_transform(m_last.transform, v.position), 1.0);

    vec4 jittered_position = vertex_out.current_position;
    jittered_position.xy += 2.0 * camera.sample_offset * jittered_position.w;
    gl_Position = jittered_position;

    vertex_out.tex_coord = (m.texture * vec4(v.tex_coord, 0, 1)).xy;
    vertex_out.normal = apply_rotation(m.transform, v.normal);
}