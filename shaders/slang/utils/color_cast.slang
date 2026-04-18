module color_cast;

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

}// namespace utils
