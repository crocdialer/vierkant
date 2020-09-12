struct fxaa_settings_t
{
    float luma_thresh;
    float mul_reduce;
    float min_reduce;
    float max_span;
    bool show_edges;
};

const fxaa_settings_t fxaa_default_settings = fxaa_settings_t(0.5, 1.0 / 256.0, 1.0 / 512.0, 16.0, false);

// fast approximate anti-aliasing, by the book
// @see http://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf
vec4 fxaa(sampler2D tex, vec2 coord, fxaa_settings_t settings)
{
    vec4 color = texture(tex, coord);
    vec3 rgbM = color.rgb;

    // sampling neighbour texels using offsets
    vec3 rgbNW = textureOffset(tex, coord, ivec2(-1, 1)).rgb;
    vec3 rgbNE = textureOffset(tex, coord, ivec2(1, 1)).rgb;
    vec3 rgbSW = textureOffset(tex, coord, ivec2(-1, -1)).rgb;
    vec3 rgbSE = textureOffset(tex, coord, ivec2(1, -1)).rgb;

    // determine texel-step size
    vec2 texel_step = 1.0 / textureSize(tex, 0);

    // NTSC luma formula
    const vec3 toLuma = vec3(0.299, 0.587, 0.114);

    // Convert from RGB to luma.
    float lumaNW = dot(rgbNW, toLuma);
    float lumaNE = dot(rgbNE, toLuma);
    float lumaSW = dot(rgbSW, toLuma);
    float lumaSE = dot(rgbSE, toLuma);
    float lumaM = dot(rgbM, toLuma);

    // Gather minimum and maximum luma.
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    // If contrast is lower than a maximum threshold ...
    if(lumaMax - lumaMin < lumaMax * settings.luma_thresh)
    {
        // ... do no AA and return.
        return color;
    }

    // Sampling is done along the gradient.
    vec2 samplingDirection;
    samplingDirection.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    samplingDirection.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    // Sampling step distance depends on the luma: The brighter the sampled texels, the smaller the final sampling step direction.
    // This results, that brighter areas are less blurred/more sharper than dark areas.
    float samplingDirectionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * settings.mul_reduce, settings.min_reduce);

    // Factor for norming the sampling direction plus adding the brightness influence.
    float minSamplingDirectionFactor = 1.0 / (min(abs(samplingDirection.x), abs(samplingDirection.y)) + samplingDirectionReduce);

    // Calculate final sampling direction vector by reducing, clamping to a range and finally adapting to the texture size.
    vec2 max_span = vec2(settings.max_span);
    samplingDirection = clamp(samplingDirection * minSamplingDirectionFactor, -max_span, max_span) * texel_step;

    // Inner samples on the tab.
    vec3 rgbSampleNeg = texture(tex, coord + samplingDirection * (1.0 / 3.0 - 0.5)).rgb;
    vec3 rgbSamplePos = texture(tex, coord + samplingDirection * (2.0 / 3.0 - 0.5)).rgb;

    vec3 rgbTwoTab = (rgbSamplePos + rgbSampleNeg) * 0.5;

    // Outer samples on the tab.
    vec3 rgbSampleNegOuter = texture(tex, coord + samplingDirection * (0.0 / 3.0 - 0.5)).rgb;
    vec3 rgbSamplePosOuter = texture(tex, coord + samplingDirection * (3.0 / 3.0 - 0.5)).rgb;

    vec3 rgbFourTab = (rgbSamplePosOuter + rgbSampleNegOuter) * 0.25 + rgbTwoTab * 0.5;

    // Calculate luma for checking against the minimum and maximum value.
    float lumaFourTab = dot(rgbFourTab, toLuma);

    // outer samples of the tab beyond the edge?
    vec4 ret = color;

    // use only two samples.
    if(lumaFourTab < lumaMin || lumaFourTab > lumaMax){ ret.rgb = rgbTwoTab; }
    // use four samples
    else{ ret.rgb = rgbFourTab; }

    // Show edges for debug purposes.
    if(settings.show_edges){ ret.r = 1.0; }
    return ret;
}