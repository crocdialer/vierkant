#ifndef CAMERA_GLSL
#define CAMERA_GLSL

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

// left/right/bottom/top frustum planes
bool frustum_cull_ortho(vec3 center, float radius, vec4 frustum)
{
    return frustum[0] > center.x + radius || frustum[1] < center.x - radius &&
           frustum[2] > center.y + radius && frustum[3] < center.y - radius;
}

// perspective symmetric case:
// mat4 projectionT = transpose(cull_result.camera->projection_matrix());
// vec4 frustumX = projectionT[3] + projectionT[0];// x + w < 0
// frustumX /= length(frustumX.xyz);
// vec4 frustumY = projectionT[3] + projectionT[1];// y + w < 0
// frustumY /= length(frustumY.xyz);
// vec4 frustum = vec4(frustumX.x, frustumX.z, frustumY.y, frustumY.z);
bool frustum_cull(vec3 center, float radius, vec4 frustum)
{
    return  !(center.z * frustum[1] - abs(center.x) * frustum[0] > -radius &&
    -center.z * frustum[3] - abs(center.y) * frustum[2] < radius);
}

#endif // CAMERA_GLSL