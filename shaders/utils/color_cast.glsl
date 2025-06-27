#ifndef COLOR_CAST_GLSL
#define COLOR_CAST_GLSL

uint color_cast(vec4 color)
{
    uint ret = 0;
    ret |= uint(clamp(color.x, 0.0, 1.0) * 255);
    ret |= uint(clamp(color.y, 0.0, 1.0) * 255) << 8;
    ret |= uint(clamp(color.z, 0.0, 1.0) * 255) << 16;
    ret |= uint(clamp(color.w, 0.0, 1.0) * 255) << 24;
    return ret;
}

vec4 color_cast(uint color)
{
    vec4 ret;
    ret.x = (color & 0xFF) / 255.0;
    ret.y = ((color >> 8) & 0xFF) / 255.0;
    ret.z = ((color >> 16) & 0xFF) / 255.0;
    ret.w = ((color >> 24) & 0xFF) / 255.0;
    return ret;
}

#endif // COLOR_CAST_GLSL