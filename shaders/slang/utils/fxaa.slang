module fxaa;

namespace utils
{

public struct fxaa_settings_t
{
    public float luma_thresh;
    public float mul_reduce;
    public float min_reduce;
    public float max_span;
    public bool show_edges;
};

public static const fxaa_settings_t fxaa_default_settings = { 0.5, 1.0 / 256.0, 1.0 / 512.0, 16.0, false };

static const float3 to_luma = float3(0.299, 0.587, 0.114);

//! compress a scene-linear color to luma [0, 1), so detection and blending work on HDR input
float3 compress(float3 c) { return c / (1.0 + dot(c, to_luma)); }

//! inverse of compress()
float3 expand(float3 c) { return c / (1.0 - dot(c, to_luma)); }

float luma(float3 c) { return dot(c, to_luma); }

public float4 fxaa(Sampler2D tex, float2 coord, fxaa_settings_t settings)
{
    float4 color = tex.Sample(coord);
    float3 rgbM = compress(color.rgb);

    float3 rgbNW = compress(tex.Sample(coord, int2(-1,  1)).rgb);
    float3 rgbNE = compress(tex.Sample(coord, int2( 1,  1)).rgb);
    float3 rgbSW = compress(tex.Sample(coord, int2(-1, -1)).rgb);
    float3 rgbSE = compress(tex.Sample(coord, int2( 1, -1)).rgb);

    uint tw, th;
    tex.GetDimensions(tw, th);
    float2 texel_step = 1.0 / float2(tw, th);

    float lumaNW = luma(rgbNW);
    float lumaNE = luma(rgbNE);
    float lumaSW = luma(rgbSW);
    float lumaSE = luma(rgbSE);
    float lumaM  = luma(rgbM);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    if(lumaMax - lumaMin < lumaMax * settings.luma_thresh) { return color; }

    float2 samplingDirection;
    samplingDirection.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    samplingDirection.y =   (lumaNW + lumaSW) - (lumaNE + lumaSE);

    float samplingDirectionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * settings.mul_reduce,
                                        settings.min_reduce);
    float minSamplingDirectionFactor = 1.0 / (min(abs(samplingDirection.x), abs(samplingDirection.y)) +
                                              samplingDirectionReduce);

    samplingDirection = clamp(samplingDirection * minSamplingDirectionFactor,
                              -float2(settings.max_span), float2(settings.max_span)) * texel_step;

    float3 rgbSampleNeg = compress(tex.Sample(coord + samplingDirection * (1.0 / 3.0 - 0.5)).rgb);
    float3 rgbSamplePos = compress(tex.Sample(coord + samplingDirection * (2.0 / 3.0 - 0.5)).rgb);
    float3 rgbTwoTab    = (rgbSamplePos + rgbSampleNeg) * 0.5;

    float3 rgbSampleNegOuter = compress(tex.Sample(coord + samplingDirection * (0.0 / 3.0 - 0.5)).rgb);
    float3 rgbSamplePosOuter = compress(tex.Sample(coord + samplingDirection * (3.0 / 3.0 - 0.5)).rgb);
    float3 rgbFourTab = (rgbSamplePosOuter + rgbSampleNegOuter) * 0.25 + rgbTwoTab * 0.5;

    float lumaFourTab = luma(rgbFourTab);

    float4 ret = color;
    ret.rgb = expand((lumaFourTab < lumaMin || lumaFourTab > lumaMax) ? rgbTwoTab : rgbFourTab);
    if(settings.show_edges) { ret.r = 1.0; }
    return ret;
}

}// namespace utils
