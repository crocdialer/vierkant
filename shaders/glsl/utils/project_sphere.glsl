#ifndef UTILS_PROJECT_SPHERE_GLSL
#define UTILS_PROJECT_SPHERE_GLSL

// 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere. Michael Mara, Morgan McGuire. 2013
bool project_sphere(vec3 C, float r, float znear, float P00, float P11, inout vec4 aabb)
{
    if (-C.z < r + znear){ return false; }

    vec2 cx = vec2(-C.x, C.z);
    vec2 vx = vec2(sqrt(dot(cx, cx) - r * r), r);
    vec2 minx = mat2(vx.x, vx.y, -vx.y, vx.x) * cx;
    vec2 maxx = mat2(vx.x, -vx.y, vx.y, vx.x) * cx;

    vec2 cy = C.yz;
    vec2 vy = vec2(sqrt(dot(cy, cy) - r * r), r);
    vec2 miny = mat2(vy.x, vy.y, -vy.y, vy.x) * cy;
    vec2 maxy = mat2(vy.x, -vy.y, vy.y, vy.x) * cy;

    aabb = vec4(minx.x / minx.y * P00, miny.x / miny.y * P11, maxx.x / maxx.y * P00, maxy.x / maxy.y * P11);
    aabb = aabb * vec4(0.5f, -0.5f, 0.5f, -0.5f) + vec4(0.5f); // clip space -> uv space

    return true;
}

// vec4 frustum: left/right/top/bottom
bool project_sphere_ortho(vec3 C, float r, float znear, vec4 frustum, inout vec4 aabb)
{
    if (-C.z < r + znear){ return false; }
    float w = frustum.y - frustum.x;
    float h = frustum.z - frustum.w;
    const float eps = 1.0e-6;
    if (w < eps || h < eps){ return false; }
    aabb = clamp(vec4((C.x - r - frustum.x) / w, (C.y - r - frustum.w) / h,
                 (C.x + r - frustum.x) / w, (C.y + r - frustum.w) / h), 0.0, 1.0);
    return true;
}

#endif //UTILS_PROJECT_SPHERE_GLSL