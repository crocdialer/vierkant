#ifndef UTILS_PROCEDURAL_ENVIRONMENT_GLSL
#define UTILS_PROCEDURAL_ENVIRONMENT_GLSL

// low-life + neutral environment-light
vec3 environment_white(vec3 direction)
{
    vec3 col_sky = vec3(1.3), col_horizon = vec3(.6), col_ground = vec3(.1);
    return direction.y > 0 ? mix(col_horizon, col_sky, direction.y) :
    mix(col_horizon, col_ground, -direction.y);
}

#endif //UTILS_PROCEDURAL_ENVIRONMENT_GLSL