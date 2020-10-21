//! support up to 13-tap
const uint max_array_size = 4;

//! specialization constant for actual size
layout (constant_id = 0) const uint gaussian_array_size = 3;

struct gaussian_ubo_t
{
    vec4 offsets[max_array_size];
    vec4 weights[max_array_size];
};

//! 1 dimensional gaussian-blur subpass
vec4 gaussian_blur(in sampler2D the_texture, in vec2 tex_coord, in gaussian_ubo_t settings)
{
    vec4 color = texture(the_texture, tex_coord) * settings.weights[0].x;
    vec2 texel = 1.0 / textureSize(the_texture, 0);

    for(int i = 1; i < gaussian_array_size; i++)
    {
        vec2 offset = texel * settings.offsets[i].xy;

        color += texture(the_texture, tex_coord + offset) * settings.weights[i].x;
        color += texture(the_texture, tex_coord - offset) * settings.weights[i].x;
    }
    return color;
}