//! support up to 13-tap
const uint max_array_size = 4;

//! specialization constant for number of taps
layout (constant_id = 0) const uint num_taps = 9;

const uint gaussian_array_size = num_taps / 4 + 1;

const bool odd_num_samples = (num_taps / 2) % 2 == 0;

struct gaussian_ubo_t
{
    vec4 offsets[max_array_size];
    vec4 weights[max_array_size];
    vec2 size;
};

//! 1 dimensional gaussian-blur subpass
vec4 gaussian_blur(in sampler2D the_texture, in vec2 tex_coord, in gaussian_ubo_t settings)
{
    vec4 color = vec4(0);

    if(odd_num_samples)
    {
        color += texture(the_texture, tex_coord) * settings.weights[0].x;
    }

    vec2 texel = 1.0 / settings.size;

    for(uint i = odd_num_samples ? 1 : 0; i < gaussian_array_size; i++)
    {
        vec2 offset = texel * settings.offsets[i].xy;

        color += texture(the_texture, tex_coord + offset) * settings.weights[i].x;
        color += texture(the_texture, tex_coord - offset) * settings.weights[i].x;
    }
    return color;
}