#version 460
#extension GL_ARB_separate_shader_objects : enable

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

layout(std140, binding = 1) uniform ubo_materials
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
            vertex_out.eye_vec = (gl_in[i].gl_Position).xyz;
            gl_Position = u_projection_matrix * u_view_matrix[j] * gl_in[i].gl_Position;
            gl_Layer = j;
            EmitVertex();
        }
        EndPrimitive();
    }
}
