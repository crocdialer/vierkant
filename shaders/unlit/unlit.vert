#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, set = 0, binding = BINDING_MESH_DRAWS) readonly buffer MeshDrawBuffer
{
    mesh_draw_t draws[];
};

layout(location = ATTRIB_POSITION) in vec3 a_position;

layout(location = 0) flat out uint object_index;

void main()
{
    object_index = gl_InstanceIndex;//gl_BaseInstance + gl_InstanceIndex
    matrix_struct_t m = draws[object_index].current_matrices;
    gl_Position = m.projection * m.modelview * vec4(a_position, 1.0);
}