#ifndef UTILS_RGB_LOG_LUV_GLSL
#define UTILS_RGB_LOG_LUV_GLSL

//! transforms an RGB color in Rec.709 to CIE XYZ.
vec3 rgb_to_xyz_rec709(vec3 c)
{
    const mat3 M = mat3(0.4123907992659595, 0.2126390058715104, 0.0193308187155918,
                        0.3575843393838780, 0.7151686787677559, 0.1191947797946259,
                        0.1804807884018343, 0.0721923153607337, 0.9505321522496608);
    return M * c;
}

// transforms an XYZ color to RGB in Rec.709.
vec3 xyz_to_rgb_rec709(vec3 c)
{
    const mat3 M = mat3(3.240969941904522, -0.9692436362808803, 0.05563007969699373,
                        -1.537383177570094, 1.875967501507721, -0.2039769588889765,
                        -0.4986107602930032, 0.04155505740717569, 1.056971514242878);
    return M * c;
}

// Encode an RGB color into a 32-bit LogLuv HDR format.
//
// The supported luminance range is roughly 10^-6..10^6 in 0.17% steps.
// The log-luminance is encoded with 14 bits and chroma with 9 bits each.
// This was empirically more accurate than using 8 bit chroma.
// Black (all zeros) is handled exactly.
uint encode_rgb_to_log_luv(vec3 color)
{
    // Convert RGB to XYZ.
    vec3 XYZ = rgb_to_xyz_rec709(color);

    // Encode log2(Y) over the range [-20,20) in 14 bits (no sign bit).
    // TODO: Fast path that uses the bits from the fp32 representation directly.
    float logY = 409.6 * (log2(XYZ.y) + 20.0); // -inf if Y==0
    uint Le = uint(clamp(logY, 0.0, 16383.0));

    // Early out if zero luminance to avoid NaN in chroma computation.
    // Note Le==0 if Y < 9.55e-7. We'll decode that as exactly zero.
    if (Le == 0) return 0;

    // Compute chroma (u,v) values by:
    //  x = X / (X + Y + Z)
    //  y = Y / (X + Y + Z)
    //  u = 4x / (-2x + 12y + 3)
    //  v = 9y / (-2x + 12y + 3)
    //
    // These expressions can be refactored to avoid a division by:
    //  u = 4X / (-2X + 12Y + 3(X + Y + Z))
    //  v = 9Y / (-2X + 12Y + 3(X + Y + Z))
    //
    float invDenom = 1.0 / (-2.0 * XYZ.x + 12.0 * XYZ.y + 3.0 * (XYZ.x + XYZ.y + XYZ.z));
    vec2 uv = vec2(4.0, 9.0) * XYZ.xy * invDenom;

    // Encode chroma (u,v) in 9 bits each.
    // The gamut of perceivable uv values is roughly [0,0.62], so scale by 820 to get 9-bit values.
    uvec2 uve = uvec2(clamp(820.0 * uv, 0.0, 511.0));

    return (Le << 18) | (uve.x << 9) | uve.y;
}

// Decode an RGB color stored in a 32-bit LogLuv HDR format.
//    See RTXDI_EncodeRGBToLogLuv() for details.
vec3 decode_log_luv_to_rgb(uint packedColor)
{
    // Decode luminance Y from encoded log-luminance.
    uint Le = packedColor >> 18;
    if (Le == 0) return vec3(0);

    float logY = (float(Le) + 0.5) / 409.6 - 20.0;
    float Y = pow(2.0, logY);

    // Decode normalized chromaticity xy from encoded chroma (u,v).
    //
    //  x = 9u / (6u - 16v + 12)
    //  y = 4v / (6u - 16v + 12)
    //
    uvec2 uve = uvec2(packedColor >> 9, packedColor) & 0x1ff;
    vec2 uv = (vec2(uve)+0.5) / 820.0;

    float invDenom = 1.0 / (6.0 * uv.x - 16.0 * uv.y + 12.0);
    vec2 xy = vec2(9.0, 4.0) * uv * invDenom;

    // Convert chromaticity to XYZ and back to RGB.
    //  X = Y / y * x
    //  Z = Y / y * (1 - x - y)
    //
    float s = Y / xy.y;
    vec3 XYZ = vec3(s * xy.x, Y, s * (1.f - xy.x - xy.y));

    // Convert back to RGB and clamp to avoid out-of-gamut colors.
    return max(xyz_to_rgb_rec709(XYZ), 0.0);
}

#endif // UTILS_RGB_LOG_LUV_GLSL