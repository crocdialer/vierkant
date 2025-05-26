#ifndef COLOR_MAP_GLSL
#define COLOR_MAP_GLSL

// debug coloring
vec3 jet(float val)
{
    return vec3(min(4.0f * val - 1.5f, -4.0f * val + 4.5f),
                min(4.0f * val - 0.5f, -4.0f * val + 3.5f),
                min(4.0f * val + 0.5f, -4.0f * val + 2.5f));
}

#endif // COLOR_MAP_GLSL