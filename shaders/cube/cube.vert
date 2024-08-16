#version 460 core
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shader_viewport_layer_array : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"

layout(std140, binding = 0) uniform ubo_matrices
{
    mat4 u_view_matrix[6];
    mat4 u_model_matrix;
    mat4 u_projection_matrix;
};

layout(location = ATTRIB_POSITION) in vec3 a_position;

layout(location = 0) out VertexData
{
    vec3 eye_vec;
} vertex_out;

void main()
{
    // amplify geometry for all cube-faces via instancing
    gl_Layer = gl_InstanceIndex;

    vec4 tmp = u_model_matrix * vec4(a_position, 1.0);
    vertex_out.eye_vec = vec3(tmp.x, -tmp.y, tmp.z);
    gl_Position = u_projection_matrix * u_view_matrix[gl_Layer] * tmp;
}
