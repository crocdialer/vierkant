#version 460 core
#extension GL_ARB_separate_shader_objects : enable

//out gl_PerVertex
//{
//    vec4 gl_Position;
//};

layout(location = 0) in vec3 a_position;

void main()
{
    gl_Position = vec4(a_position, 1.0);
}
