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

const vec3 half_extents = vec3(0.5);

const vec3 base_vertices[8] =
{
    vec3(-half_extents.x, -half_extents.y, half_extents.z), // bottom left front 0
    vec3(half_extents.x, -half_extents.y, half_extents.z), // bottom right front 1
    vec3(half_extents.x, -half_extents.y, -half_extents.z), // bottom right back 2
    vec3(-half_extents.x, -half_extents.y, -half_extents.z), // bottom left back 3
    vec3(-half_extents.x, half_extents.y, half_extents.z), // top left front 4
    vec3(half_extents.x, half_extents.y, half_extents.z), // top right front 5
    vec3(half_extents.x, half_extents.y, -half_extents.z), // top right back 6
    vec3(-half_extents.x, half_extents.y, -half_extents.z), // top left back 7
};

uint indices[36] = {
    0, 1, 5, 5, 4, 0, // front
    1, 2, 6, 6, 5, 1, // right
    2, 3, 7, 7, 6, 2, // back
    3, 0, 4, 4, 7, 3, // left
    4, 5, 6, 6, 7, 4, // top
    3, 2, 1, 1, 0, 3, // bottom
};

layout(location = 0) out VertexData
{
    vec3 eye_vec;
} vertex_out;

void main()
{
    // amplify geometry for all cube-faces via instancing
    gl_Layer = gl_InstanceIndex;

    vec4 tmp = u_model_matrix * vec4(base_vertices[indices[gl_VertexIndex]], 1.0);
    vertex_out.eye_vec = vec3(tmp.x, -tmp.y, tmp.z);
    gl_Position = u_projection_matrix * u_view_matrix[gl_Layer] * tmp;
}
