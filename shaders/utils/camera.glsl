struct camera_t
{
    mat4 view;
    mat4 projection;
    vec2 sample_offset;
    float near;
    float far;
};