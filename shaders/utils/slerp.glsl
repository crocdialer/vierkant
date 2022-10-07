//! spherical linear interpolation
vec3 slerp(vec3 x, vec3 y, float a)
{
    // get cosine of angle between vectors (-1 -> 1)
    float cos_alpha = dot(x, y);

    if (cos_alpha > 0.9999 || cos_alpha < -0.9999){ return a <= 0.5 ? x : y; }

    // get angle (0 -> pi)
    float alpha = acos(cos_alpha);

    // get sine of angle between vectors (0 -> 1)
    float sin_alpha = sin(alpha);

    // this breaks down when sin_alpha = 0, i.e. alpha = 0 or pi
    float t1 = sin((1.0 - a) * alpha) / sin_alpha;
    float t2 = sin(a * alpha) / sin_alpha;

    // interpolate src vectors
    return x * t1 + y * t2;
}