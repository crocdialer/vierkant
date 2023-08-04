#ifndef RAY_RESERVOIR_GLSL
#define RAY_RESERVOIR_GLSL

#include "../utils/octahedral_map.glsl"
#include "../utils/rgb_log_luv.glsl"

#extension GL_EXT_shader_explicit_arithmetic_types: require

//! ReservoirGI represents an indirect lighting reservoir that stores radiance and weight,
//! as well as the position the radiance came from.
struct ReservoirGI
{
    //! postion of 2nd bounce surface.
    vec3 position;

    //! normal vector of 2nd bounce surface.
    vec3 normal;

    //! incoming radiance from 2nd bounce surface.
    vec3 radiance;

    //! Overloaded: represents RIS weight sum during streaming,
    //! then reservoir weight (inverse PDF) after FinalizeResampling
    float weight_sum;

    //! number of samples considered for this reservoir
    uint M;

    // number of frames the chosen sample has survived.
    uint age;
};

struct packed_reservoir_t
{
    //! position as-is
    vec3 position;

    //! age encoded unsigned/16
    uint16_t age;

    //! num samples encoded unsigned/16
    uint16_t M;

    //! rgb-radiance stored in 32bit LogLUV format.
    uint32_t packed_radiance;

    //! weight as-is
    float weight;

    //! stored as 2x 16-bit snorms in octahedral mapping
    uint32_t packed_normal;

    //! 16-byte padding
    float unused;
};

packed_reservoir_t pack(ReservoirGI reservoir)
{
    packed_reservoir_t ret;
    ret.position = reservoir.position;
    ret.age = uint16_t(reservoir.age);
    ret.M = uint16_t(reservoir.M);
    ret.packed_radiance = encode_rgb_to_log_luv(reservoir.radiance);
    ret.weight = reservoir.weight_sum;
    ret.packed_normal = pack_snorm_2x16(normalized_vector_to_octahedral_mapping(reservoir.normal));
    return ret;
}

ReservoirGI unpack(packed_reservoir_t packed_reservoir)
{
    ReservoirGI ret;
    ret.position = packed_reservoir.position;
    ret.age = uint(packed_reservoir.age);
    ret.M = uint(packed_reservoir.M);
    ret.radiance = decode_log_luv_to_rgb(packed_reservoir.packed_radiance);
    ret.weight_sum = packed_reservoir.weight;
    ret.normal = octahedral_mapping_to_normalized_vector(unpack_snorm_2x16(packed_reservoir.packed_normal));
    return ret;
}

// Creates a GI reservoir from a raw light sample.
// Note: the original sample PDF can be embedded into sampleRadiance, in which case the sample_pdf parameter should be set to 1.0.
ReservoirGI make_reservoir(const vec3 sample_pos,
                           const vec3 sample_normal,
                           const vec3 sample_radiance,
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

// Adds `newReservoir` into `reservoir`, returns true if the new reservoir's sample was selected.
// This function assumes the newReservoir has been normalized, so its weightSum means "1/g * 1/M * \sum{g/p}"
// and the targetPdf is a conversion factor from the newReservoir's space to the reservoir's space (integrand).
bool combine_reservoirs(inout ReservoirGI reservoir,
                        const ReservoirGI new_reservoir,
                        float random,
                        float target_pdf)
{
    // What's the current weight (times any prior-step RIS normalization factor)
    const float ris_weight = target_pdf * new_reservoir.weight_sum * new_reservoir.M;

    // Our *effective* candidate pool is the sum of our candidates plus those of our neighbors
    reservoir.M += new_reservoir.M;

    // Update the weight sum
    reservoir.weight_sum += ris_weight;

    // Decide if we will randomly pick this sample
    bool select_sample = (random * reservoir.weight_sum <= ris_weight);

    if (select_sample)
    {
        reservoir.position = new_reservoir.position;
        reservoir.normal = new_reservoir.normal;
        reservoir.radiance = new_reservoir.radiance;
        reservoir.age = new_reservoir.age;
    }
    return select_sample;
}

#endif// RAY_RESERVOIR_GLSL
