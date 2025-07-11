#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std430, set = 0, binding = BINDING_MESH_DRAWS) readonly buffer MeshDrawBuffer
{
    mesh_draw_t draws[];
};

layout(location = ATTRIB_POSITION) in vec3 a_position;

layout(location = LOCATION_INDEX_BUNDLE) flat out index_bundle_t indices;
layout(location = LOCATION_VERTEX_BUNDLE) out VertexData
{
    vec3 eye_vec;
} vertex_out;

void main()
{
    indices.mesh_draw_index = gl_InstanceIndex;
    matrix_struct_t m = draws[indices.mesh_draw_index].current_matrices;
    vertex_out.eye_vec = a_position.xyz;
    gl_Position = m.projection * vec4(apply_transform(m.transform, a_position), 1.0);
}