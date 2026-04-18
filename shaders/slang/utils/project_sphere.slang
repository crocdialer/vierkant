module project_sphere;

namespace utils
{

public bool project_sphere(float3 C, float r, float znear, float P00, float P11, inout float4 aabb)
{
    if(-C.z < r + znear) { return false; }

    float2 cx = float2(-C.x, C.z);
    float2 vx = float2(sqrt(dot(cx, cx) - r * r), r);
    float2 minx = mul(float2x2(float2(vx.x, -vx.y), float2(vx.y,  vx.x)), cx);
    float2 maxx = mul(float2x2(float2(vx.x,  vx.y), float2(-vx.y, vx.x)), cx);

    float2 cy = C.yz;
    float2 vy = float2(sqrt(dot(cy, cy) - r * r), r);
    float2 miny = mul(float2x2(float2(vy.x, -vy.y), float2(vy.y,  vy.x)), cy);
    float2 maxy = mul(float2x2(float2(vy.x,  vy.y), float2(-vy.y, vy.x)), cy);

    aabb = float4(minx.x / minx.y * P00, miny.x / miny.y * P11, maxx.x / maxx.y * P00, maxy.x / maxy.y * P11);
    aabb = aabb * float4(0.5f, -0.5f, 0.5f, -0.5f) + float4(0.5f);

    return true;
}

public bool project_sphere_ortho(float3 C, float r, float znear, float4 frustum, inout float4 aabb)
{
    if(-C.z < r + znear) { return false; }
    float w = abs(frustum.y - frustum.x);
    float h = abs(frustum.w - frustum.z);
    const float eps = 1.0e-6;
    if(w < eps || h < eps) { return false; }
    aabb = clamp(float4((C.x - r - frustum.x) / w, 1.0 - (C.y + r - frustum.z) / h,
                        (C.x + r - frustum.x) / w, 1.0 - (C.y - r - frustum.z) / h), 0.0, 1.0);
    return true;
}

}// namespace utils
