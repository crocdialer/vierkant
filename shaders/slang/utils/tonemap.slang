module tonemap;

namespace utils
{

public float3 tonemap_reinhard(float3 x) { return x / (1.0 + x); }
public float  tonemap_reinhard(float x)  { return x / (1.0 + x); }

public float3 tonemap_reinhard2(float3 x)
{
    const float L_white = 4.0;
    return (x * (1.0 + x / (L_white * L_white))) / (1.0 + x);
}
public float tonemap_reinhard2(float x)
{
    const float L_white = 4.0;
    return (x * (1.0 + x / (L_white * L_white))) / (1.0 + x);
}

public float3 tonemap_exposure(float3 hdr_color, float exposure)
{
    return float3(1.0) - exp(-hdr_color * exposure);
}

public float3 tonemap_aces(float3 x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
public float tonemap_aces(float x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

public float3 tonemap_filmic(float3 x)
{
    float3 X = max(float3(0.0), x - 0.004);
    float3 result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
    return pow(result, float3(2.2));
}
public float tonemap_filmic(float x)
{
    float X = max(0.0, x - 0.004);
    float result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
    return pow(result, 2.2);
}

public float3 tonemap_unreal(float3 x) { return x / (x + 0.155) * 1.019; }
public float  tonemap_unreal(float x)  { return x / (x + 0.155) * 1.019; }

}// namespace utils
