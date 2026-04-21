module color;

namespace utils
{

public float4 color_cast(uint c)
{
    return float4((c & 0xFF) / 255.0,
                  ((c >> 8) & 0xFF) / 255.0,
                  ((c >> 16) & 0xFF) / 255.0,
                  ((c >> 24) & 0xFF) / 255.0);
}

public uint color_cast(float4 color)
{
    uint4 c = uint4(clamp(color, 0.0, 1.0) * 255.0);
    return c.x | (c.y << 8) | (c.z << 16) | (c.w << 24);
}

public static const float3 LuminanceNTSC = float3(0.298999994993, 0.587000012398, 0.114000000060);

// column-major GLSL matrices transposed to row-major Slang float3x3
public static const float3x3 RGB2YCoCg = float3x3(float3( 0.25,  0.5,  0.25),
                                                   float3( 0.5,   0.0, -0.5),
                                                   float3(-0.25,  0.5, -0.25));

public static const float3x3 YCoCg2RGB = float3x3(float3( 1.0,  1.0, -1.0),   // R = Y + Co - Cg
                                                   float3( 1.0,  0.0,  1.0),   // G = Y + Cg
                                                   float3( 1.0, -1.0, -1.0));  // B = Y - Co - Cg

public float3 jet(float val)
{
    return clamp(float3(min(4.0 * val - 1.5, -4.0 * val + 4.5),
                        min(4.0 * val - 0.5, -4.0 * val + 3.5),
                        min(4.0 * val + 0.5, -4.0 * val + 2.5)), 0.0, 1.0);
}

}// namespace utils
