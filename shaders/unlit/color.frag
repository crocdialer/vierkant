#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, set = 0, binding = BINDING_MATERIAL) readonly buffer MaterialBuffer
{
    material_struct_t materials[];
};

layout(location = LOCATION_INDEX_BUNDLE) flat in index_bundle_t indices;
layout(location = LOCATION_VERTEX_BUNDLE) in VertexData
{
    vec4 color;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
//    uint material_index = indices.mesh_draw_index;
    out_color = vertex_in.color * materials[indices.material_index].color;
}