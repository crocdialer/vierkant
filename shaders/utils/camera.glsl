struct camera_t
{
    mat4 view;
    mat4 projection;
    vec2 sample_offset;
    float near;
    float far;

    // left/right/top/bottom frustum planes
    vec4 frustum;
};

bool frustum_cull(vec3 center, float radius, vec4 frustum)
{
    return  !(center.z * frustum[1] - abs(center.x) * frustum[0] > -radius &&
            center.z * frustum[3] - abs(center.y) * frustum[2] > -radius);
}