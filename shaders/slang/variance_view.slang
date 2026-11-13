import "utils/color.slang";
import "ray/pixel_data.slang";

[[vk::binding(0)]]
StructuredBuffer<ray::pixel_data_t> in_pixel;

//! per-pixel luminance-variance
[[vk::binding(1)]]
StructuredBuffer<float> in_variance;

[[vk::binding(2)]]
[[vk::image_format("r8")]]
RWTexture2D<float> out_mask;

struct push_constants_t
{
    uint2 size;
};

[[vk::push_constant]]
push_constants_t constants;

//! relative variance below this mean-luminance is dominated by the floor
static const float VARIANCE_VIEW_EPS = 1e-4;

//! "hard to sample" mask: relative variance v / (m² + eps) on a log-scale, 1e-3 .. 1e2 -> [0, 1]
[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    if(any(dispatchThreadID.xy >= constants.size)) { return; }
    uint buffer_idx = dispatchThreadID.y * constants.size.x + dispatchThreadID.x;

    float m = dot(in_pixel[buffer_idx].radiance, utils::LuminanceNTSC);
    float rel_var = in_variance[buffer_idx] / (m * m + VARIANCE_VIEW_EPS);

    out_mask[dispatchThreadID.xy] = saturate((log10(max(rel_var, 1e-6)) + 3.0) / 5.0);
}
