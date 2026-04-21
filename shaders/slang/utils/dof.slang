module dof;

import color;
import random;
import sampling;

namespace utils
{

public struct dof_params_t
{
    public float focal_distance;
    public float focal_length;
    public float aperture;
    public float sensor_width;
    public float near;
    public float far;
    public bool debug;
};

float linearize(float depth, float near, float far)
{
    return clamp(near / depth, near, far);
}

float avg_depth(Sampler2D depth_map, float2 coord)
{
    uint tw, th;
    depth_map.GetDimensions(tw, th);
    float2 texel = 1.0 / float2(tw, th);

    float d = depth_map.Sample(coord).x;
    const float2 diagonal_cross[4] = { float2(-1,-1), float2(1,1), float2(-1,1), float2(1,-1) };

    for(uint o = 0; o < 4; ++o) { d += depth_map.Sample(coord + 1.5 * texel * diagonal_cross[o]).x; }
    return d / 5.0;
}

public float4 depth_of_field(Sampler2D color_map, Sampler2D depth_map, float2 coord,
                              float2 frag_coord, dof_params_t p)
{
    uint rng_state = uint(frag_coord.x + frag_coord.y * 65536.0);

    float depth = linearize(avg_depth(depth_map, coord), p.near, p.far);

    float circle_of_confusion_sz = abs((p.aperture * p.focal_length * (depth - p.focal_distance)) /
                                       (p.sensor_width * depth * (p.focal_length + p.focal_distance)));

    const uint num_taps = 13;
    float2 Xi = float2(rnd(rng_state), rnd(rng_state));

    float3 color = float3(0);
    for(uint i = 0; i < num_taps; ++i)
    {
        float2 offset = 0.5 * circle_of_confusion_sz * sample_unit_disc(frac(Xi + Hammersley(i, num_taps)));
        color += color_map.Sample(coord + offset).rgb;
    }
    color /= num_taps;

    if(p.debug)
    {
        float3 focus_overlay = jet(1.0 - clamp(circle_of_confusion_sz * p.focal_distance / p.focal_length, 0.0, 1.0));
        return float4(lerp(color, focus_overlay, 0.2), 1.0);
    }
    return float4(color, 1.0);
}

}// namespace utils
