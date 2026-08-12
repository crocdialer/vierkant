module octahedral_map;

namespace utils
{

// Converts normalized direction to the octahedral map (non-equal area, signed normalized).
//  - n: Normalized direction.
//  - returns: Position in octahedral map in [-1,1] for each component.
public float2 normalized_vector_to_octahedral_mapping(float3 n)
{
    // Project the sphere onto the octahedron (|x|+|y|+|z| = 1) and then onto the xy-plane.
    float2 p = float2(n.x, n.y) * (1.0 / (abs(n.x) + abs(n.y) + abs(n.z)));

    // Reflect the folds of the lower hemisphere over the diagonals.
    if(n.z < 0.0)
    {
        p = float2((1.0 - abs(p.y)) * (p.x >= 0.0 ? 1.0 : -1.0), (1.0 - abs(p.x)) * (p.y >= 0.0 ? 1.0 : -1.0));
    }
    return p;
}

// Converts point in the octahedral map to normalized direction (non-equal area, signed normalized).
//  - p: Position in octahedral map in [-1,1] for each component.
//  - returns: Normalized direction.
public float3 octahedral_mapping_to_normalized_vector(float2 p)
{
    float3 n = float3(p.x, p.y, 1.0 - abs(p.x) - abs(p.y));

    // Reflect the folds of the lower hemisphere over the diagonals.
    if(n.z < 0.0)
    {
        n.xy = float2((1.0 - abs(n.y)) * (n.x >= 0.0 ? 1.0 : -1.0), (1.0 - abs(n.x)) * (n.y >= 0.0 ? 1.0 : -1.0));
    }
    return normalize(n);
}

// Unpack two 16-bit snorm values from the lo/hi bits of a dword.
//  - packed: Two 16-bit snorm in low/high bits.
//  - returns: Two float values in [-1,1].
public float2 unpack_snorm_2x16(uint packed)
{
    int2 bits = int2(packed << 16, packed) >> 16;
    float2 unpacked = max(float2(bits) / 32767.0, -1.0);
    return unpacked;
}

// Pack two floats into 16-bit snorm values in the lo/hi bits of a dword.
//  - returns: Two 16-bit snorm in low/high bits.
public uint pack_snorm_2x16(float2 v)
{
    v = any(isnan(v)) ? float2(0, 0) : clamp(v, -1.0, 1.0);
    int2 iv = int2(round(v * 32767.0));
    uint packed = (iv.x & 0x0000ffff) | (iv.y << 16);
    return packed;
}

// Unpack two 11-bit snorm values from the low 22 bits of a dword.
//  - packed: Two 11-bit snorm in bits [10:0] and [21:11].
//  - returns: Two float values in [-1,1].
public float2 unpack_snorm_2x11(uint packed)
{
    int2 bits = int2(packed << 21, packed << 10) >> 21;
    return max(float2(bits) / 1023.0, -1.0);
}

// Pack two floats into 11-bit snorm values in the low 22 bits of a dword.
//  - returns: Two 11-bit snorm in bits [10:0] and [21:11].
public uint pack_snorm_2x11(float2 v)
{
    v = any(isnan(v)) ? float2(0, 0) : clamp(v, -1.0, 1.0);
    int2 iv = int2(round(v * 1023.0));
    return (iv.x & 0x7ff) | ((iv.y & 0x7ff) << 11);
}

// Construct a deterministic orthonormal basis for a direction.
// Duff et al., "Building an Orthonormal Basis, Revisited" (JCGT 2017).
//
// ATTENTION: must stay bit-identical to the host-side implementation in octahedral_map.hpp,
// otherwise packed tangent-angles decode against a different reference frame. The basis flips
// discontinuously across n.z == 0 - safe because both sides evaluate 'n' from the same quantized
// bits and 1 - |p.x| - |p.y| contains no multiply (no fma-contraction can change the sign).
public void reference_basis(float3 n, out float3 b1, out float3 b2)
{
    float s = n.z >= 0.0 ? 1.0 : -1.0;
    float a = -1.0 / (s + n.z);
    float b = n.x * n.y * a;
    b1 = float3(1.0 + s * n.x * n.x * a, s * b, -s * n.x);
    b2 = float3(b, s + n.y * n.y * a, -n.y);
}

// bit-layout of a packed tangent-frame: octahedral normal | tangent-angle | handedness
public static const uint tangent_frame_oct_bits = 22;
public static const uint tangent_frame_angle_bits = 9;
public static const uint tangent_frame_angle_steps = 1u << tangent_frame_angle_bits;
public static const uint tangent_frame_angle_mask = tangent_frame_angle_steps - 1;
public static const uint tangent_frame_sign_bit = 1u << 31;

static const float TWO_PI = 6.28318530717958647692;

// Pack an orthonormal tangent-frame into a single dword.
// The normal is stored as an 11:11 octahedral mapping, the tangent as its rotation around that
// normal relative to reference_basis(), plus one bit of bitangent-handedness. The angle is measured
// against the *quantized* normal, so the two quantization-errors do not compound.
//
// NOTE: the host packs with vierkant::pack_octahedral_2x11, which searches all four floor/ceil
// roundings of the octahedral pair and reaches 0.082 vs. the 0.117 degrees here. Decoding is
// identical either way, so the two may differ - but vertices repacked below (skin/morph) stay coarser.
//  - n: normalized normal.
//  - t: tangent, not required to be orthogonal to n (the projection is implicit).
//  - w: bitangent handedness, glTF convention: bitangent = cross(n, t) * w.
public uint pack_tangent_frame(float3 n, float3 t, float w)
{
    uint oct = pack_snorm_2x11(normalized_vector_to_octahedral_mapping(n));

    // re-derive the basis from the quantized normal, exactly as the unpack-side will
    float3 quantized_normal = octahedral_mapping_to_normalized_vector(unpack_snorm_2x11(oct));
    float3 b1, b2;
    reference_basis(quantized_normal, b1, b2);

    // a tangent parallel to n projects to (0,0) -> atan2(0,0) == 0, an arbitrary but valid frame
    float angle = atan2(dot(t, b2), dot(t, b1));
    int steps = int(round(angle * (float(tangent_frame_angle_steps) / TWO_PI)));
    return oct | ((uint(steps) & tangent_frame_angle_mask) << tangent_frame_oct_bits) |
           (w < 0.0 ? tangent_frame_sign_bit : 0u);
}

// Unpack a tangent-frame packed by pack_tangent_frame.
//  - n: output normal.
//  - t: output tangent, orthogonal to n.
//  - w: output bitangent handedness, bitangent = cross(n, t) * w.
public void unpack_tangent_frame(uint packed, out float3 n, out float3 t, out float w)
{
    n = octahedral_mapping_to_normalized_vector(unpack_snorm_2x11(packed));
    float3 b1, b2;
    reference_basis(n, b1, b2);

    float angle = float((packed >> tangent_frame_oct_bits) & tangent_frame_angle_mask) *
                  (TWO_PI / float(tangent_frame_angle_steps));
    t = cos(angle) * b1 + sin(angle) * b2;
    w = (packed & tangent_frame_sign_bit) != 0u ? -1.0 : 1.0;
}

// Unpack only the normal of a packed tangent-frame - skips the basis-construction and sincos.
// Prefer this in paths that never sample a normal-map (shadow-rays, alpha-test any-hit).
public float3 unpack_tangent_frame_normal(uint packed)
{
    return octahedral_mapping_to_normalized_vector(unpack_snorm_2x11(packed));
}

}
