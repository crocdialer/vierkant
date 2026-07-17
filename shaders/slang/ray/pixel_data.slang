module pixel_data;

import "../utils/rgb_log_luv.slang";
import "../utils/color.slang";

namespace ray
{

public struct pixel_data_t
{
    public float3 radiance;
    public float alpha;
    public float depth;

    //! normal in octahedral encoding
    public uint normal;

    public uint object_id;

    public uint albedo;
};

public pixel_data_t pack(float3 radiance, float alpha, uint normal_oct, float depth, uint object_id, uint albedo)
{
    pixel_data_t ret;
    ret.radiance = radiance;
    ret.alpha = alpha;
    ret.normal = normal_oct;
    ret.depth = depth;
    ret.object_id = object_id;
    ret.albedo = albedo;
    return ret;
}

public float3 unpack_albedo(uint packed)
{
    return utils::color_cast(packed).rgb;
}

}// namespace ray
