module camera;

namespace utils
{

public struct camera_t
{
    public float4x4 view;
    public float4x4 projection;
    public float2 sample_offset;
    public float near;
    public float far;

    // left/right/top/bottom frustum planes
    public float4 frustum;
    public bool ortho;
    int pad[3];
};

// left/right/bottom/top frustum planes
public bool frustum_cull_ortho(float3 center, float radius, float4 frustum)
{
    return frustum[0] > center.x + radius || frustum[1] < center.x - radius || frustum[2] > center.y + radius ||
           frustum[3] < center.y - radius;
}

// perspective symmetric case:
// float4x4 projectionT = transpose(cull_result.camera->projection_matrix());
// vec4 frustumX = projectionT[3] + projectionT[0];// x + w < 0
// frustumX /= length(frustumX.xyz);
// vec4 frustumY = projectionT[3] + projectionT[1];// y + w < 0
// frustumY /= length(frustumY.xyz);
// vec4 frustum = vec4(frustumX.x, frustumX.z, frustumY.y, frustumY.z);
public bool frustum_cull(float3 center, float radius, float4 frustum)
{
    return !(center.z * frustum[1] - abs(center.x) * frustum[0] > -radius &&
             -center.z * frustum[3] - abs(center.y) * frustum[2] < radius);
}
}
