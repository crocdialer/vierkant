module pixel_data;

import "../utils/octahedral_map.slang";
import "../utils/rgb_log_luv.slang";

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

    uint pad[1];
};

public pixel_data_t pack(float3 radiance, float alpha, float3 normal, float depth, uint object_id)
{
    pixel_data_t ret;
    ret.radiance = radiance;
    ret.alpha = alpha;
    ret.normal = utils::pack_snorm_2x16(utils::normalized_vector_to_octahedral_mapping(normal));
    ret.depth = depth;
    ret.object_id = object_id;
    return ret;
}

}// namespace ray
