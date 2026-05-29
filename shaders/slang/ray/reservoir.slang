module reservoir;

import "../utils/octahedral_map.slang";
import "../utils/rgb_log_luv.slang";

namespace ray
{

//! ReservoirGI represents an indirect lighting reservoir that stores radiance and weight,
//! as well as the position the radiance came from.
public struct ReservoirGI
{
    //! position of 2nd bounce surface.
    public float3 position;

    //! normal vector of 2nd bounce surface.
    public float3 normal;

    //! incoming radiance from 2nd bounce surface.
    public float3 radiance;

    //! Overloaded: represents RIS weight sum during streaming,
    //! then reservoir weight (inverse PDF) after FinalizeResampling
    public float weight_sum;

    //! number of samples considered for this reservoir
    public uint M;

    //! number of frames the chosen sample has survived.
    public uint age;
};

public struct packed_reservoir_t
{
    //! position as-is
    public float3 position;

    //! age encoded unsigned/16
    public uint16_t age;

    //! num samples encoded unsigned/16
    public uint16_t M;

    //! rgb-radiance stored in 32-bit LogLUV format.
    public uint packed_radiance;

    //! weight as-is
    public float weight;

    //! stored as 2x 16-bit snorms in octahedral mapping
    public uint packed_normal;

    //! 16-byte padding
    public float unused;
};

public packed_reservoir_t pack(ReservoirGI reservoir)
{
    packed_reservoir_t ret;
    ret.position = reservoir.position;
    ret.age = uint16_t(reservoir.age);
    ret.M = uint16_t(reservoir.M);
    ret.packed_radiance = utils::encode_rgb_to_log_luv(reservoir.radiance);
    ret.weight = reservoir.weight_sum;
    ret.packed_normal = utils::pack_snorm_2x16(utils::normalized_vector_to_octahedral_mapping(reservoir.normal));
    ret.unused = 0.0;
    return ret;
}

public ReservoirGI unpack(packed_reservoir_t packed_reservoir)
{
    ReservoirGI ret;
    ret.position = packed_reservoir.position;
    ret.age = uint(packed_reservoir.age);
    ret.M = uint(packed_reservoir.M);
    ret.radiance = utils::decode_log_luv_to_rgb(packed_reservoir.packed_radiance);
    ret.weight_sum = packed_reservoir.weight;
    ret.normal = utils::octahedral_mapping_to_normalized_vector(utils::unpack_snorm_2x16(packed_reservoir.packed_normal));
    return ret;
}

// Creates a GI reservoir from a raw light sample.
// Note: the original sample PDF can be embedded into sampleRadiance, in which case sample_pdf should be 1.0.
public ReservoirGI make_reservoir(const float3 sample_pos,
                                  const float3 sample_normal,
                                  const float3 sample_radiance,
                                  const float sample_pdf)
{
    ReservoirGI reservoir;
    reservoir.position = sample_pos;
    reservoir.normal = sample_normal;
    reservoir.radiance = sample_radiance;
    reservoir.weight_sum = sample_pdf > 0.0 ? 1.0 / sample_pdf : 0.0;
    reservoir.M = 1;
    reservoir.age = 0;
    return reservoir;
}

// Rec.709 luminance: scalar target function p̂ for RIS.
public float target_pdf(const float3 radiance)
{
    return dot(max(radiance, float3(0)), float3(0.2126, 0.7152, 0.0722));
}

// Adds `new_reservoir` into `reservoir`, returns true if the new reservoir's sample was selected.
// Assumes new_reservoir is normalized: weightSum means "1/g * 1/M * sum{g/p}".
// target_pdf is a conversion factor from new_reservoir's space to reservoir's space (integrand).
public bool combine_reservoirs(inout ReservoirGI reservoir,
                               const ReservoirGI new_reservoir,
                               float random,
                               float target_pdf)
{
    const float ris_weight = target_pdf * new_reservoir.weight_sum * new_reservoir.M;

    reservoir.M += new_reservoir.M;
    reservoir.weight_sum += ris_weight;

    bool select_sample = (random * reservoir.weight_sum <= ris_weight);

    if(select_sample)
    {
        reservoir.position = new_reservoir.position;
        reservoir.normal = new_reservoir.normal;
        reservoir.radiance = new_reservoir.radiance;
        reservoir.age = new_reservoir.age;
    }
    return select_sample;
}

}// namespace ray
