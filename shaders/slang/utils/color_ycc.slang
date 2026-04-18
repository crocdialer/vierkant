module color_ycc;

namespace utils
{

public static const float3 LuminanceNTSC = float3(0.298999994993, 0.587000012398, 0.114000000060);

// column-major GLSL matrices transposed to row-major Slang float3x3
public static const float3x3 RGB2YCoCg = float3x3(float3( 0.25,  0.5,  0.25),
                                                   float3( 0.5,   0.0, -0.5),
                                                   float3(-0.25,  0.5, -0.25));

public static const float3x3 YCoCg2RGB = float3x3(float3( 1.0,  1.0, -1.0),   // R = Y + Co - Cg
                                                   float3( 1.0,  0.0,  1.0),   // G = Y + Cg
                                                   float3( 1.0, -1.0, -1.0));  // B = Y - Co - Cg

}// namespace utils
