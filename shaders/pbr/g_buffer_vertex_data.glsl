#ifndef PBR_G_BUFFER_VERTEX_DATA_GLSL
#define PBR_G_BUFFER_VERTEX_DATA_GLSL

struct g_buffer_vertex_data_t
{
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
    vec4 current_position;
    vec4 last_position;
};

#endif// PBR_G_BUFFER_VERTEX_DATA_GLSL