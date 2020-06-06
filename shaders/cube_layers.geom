#version 460
#extension GL_ARB_separate_shader_objects : enable

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

layout(std140, binding = 0) uniform ubo_matrices
{
    mat4 u_view_matrix[6];
    mat4 u_model_matrix;
    mat4 u_projection_matrix;
};

layout(location = 0) out VertexData
{
  vec3 eye_vec;
} vertex_out;

void main()
{
    for(int j = 0; j < 6; ++j)
    {
        for(int i = 0; i < 3; ++i)
        {
            vec4 tmp = u_view_matrix[j] * u_model_matrix * gl_in[i].gl_Position;
//            vec4 tmp = gl_in[i].gl_Position;
            vertex_out.eye_vec = tmp.xyz;
            gl_Position = u_projection_matrix * tmp;
            gl_Layer = j;
            EmitVertex();
        }
        EndPrimitive();
    }
}
