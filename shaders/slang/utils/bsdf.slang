module bsdf;

import "sampling.slang";
import "constants.slang";

namespace utils
{

public float SchlickFresnel(float u)
{
    float m = clamp(1.0 - u, 0.0, 1.0);
    return m * m * m * m * m;
}

public float F_Schlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

public float3 F_Schlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

public float3 F_SchlickR(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

public float DielectricFresnel(float cos_theta_i, float eta)
{
    float sinThetaTSq = eta * eta * (1.0 - cos_theta_i * cos_theta_i);
    if (sinThetaTSq > 1.0) return 1.0;

    float cos_theta_t = sqrt(max(1.0 - sinThetaTSq, 0.0));
    float rs = (eta * cos_theta_t - cos_theta_i) / (eta * cos_theta_t + cos_theta_i);
    float rp = (eta * cos_theta_i - cos_theta_t) / (eta * cos_theta_i + cos_theta_t);
    return 0.5 * (rs * rs + rp * rp);
}

public float3 Fresnel0ToIor(float3 F0)
{
    float3 sqrtF0 = sqrt(F0);
    return (float3(1.0) + sqrtF0) / (float3(1.0) - sqrtF0);
}

public float IorToFresnel0(float transmittedIor, float incidentIor)
{
    return pow((transmittedIor - incidentIor) / (transmittedIor + incidentIor), 2.0);
}

public float3 IorToFresnel0(float3 transmittedIor, float incidentIor)
{
    float3 v = (transmittedIor - incidentIor) / (transmittedIor + incidentIor);
    return v * v;
}

// XYZ to sRGB/Rec.709 matrix (column-major, same as GLSL mat3 constructor)
static const float3x3 XYZ_TO_REC709 = float3x3(
    float3(3.2404542, -0.9692660,  0.0556434),
    float3(-1.5371385,  1.8760108, -0.2040259),
    float3(-0.4985314,  0.0415560,  1.0572252)
);

public float3 evalSensitivity(float OPD, float3 shift)
{
    float phase = 2.0 * PI * OPD * 1.0e-9;
    float phase2 = phase * phase;
    float3 val = float3(5.4856e-13, 4.4201e-13, 5.2481e-13);
    float3 pos = float3(1.6810e+06, 1.7953e+06, 2.2084e+06);
    float3 var = float3(4.3278e+09, 9.3046e+09, 6.6121e+09);

    float3 xyz = val * sqrt(2.0 * PI * var) * cos(pos * phase + shift) * exp(-phase2 * var);
    xyz.x += 9.7470e-14 * sqrt(2.0 * PI * 4.5282e+09) * cos(2.2399e+06 * phase + shift.x) * exp(-4.5282e+09 * phase2);
    xyz /= 1.0685e-7;

    return mul(XYZ_TO_REC709, xyz);
}

public float3 iridescent_fresnel(float outsideIOR, float iridescenceIOR, float3 baseF0, float iridescenceThickness, float cosTheta1)
{
    float R0 = IorToFresnel0(iridescenceIOR, outsideIOR);
    float R12 = F_Schlick(cosTheta1, R0);
    float T121 = 1.0 - R12;

    float sinTheta2Sq = pow(outsideIOR / iridescenceIOR, 2.0) * (1.0 - pow(cosTheta1, 2.0));
    float cosTheta2Sq = 1.0 - sinTheta2Sq;
    if (cosTheta2Sq < 0.0) return float3(1.0);
    float cosTheta2 = sqrt(cosTheta2Sq);

    float3 baseIOR = Fresnel0ToIor(clamp(baseF0, 0.0, 0.9999) + 0.0001);
    float3 R1 = IorToFresnel0(baseIOR, iridescenceIOR);
    float3 R23 = F_Schlick(cosTheta2, R1);

    float phi12 = iridescenceIOR < outsideIOR ? PI : 0.0;
    float phi21 = PI - phi12;

    float3 phi23 = float3(0.0);
    if (baseIOR.x < iridescenceIOR) phi23.x = PI;
    if (baseIOR.y < iridescenceIOR) phi23.y = PI;
    if (baseIOR.z < iridescenceIOR) phi23.z = PI;

    float3 phi = float3(phi21) + phi23;
    float OPD = 2.0 * iridescenceIOR * iridescenceThickness * cosTheta1;

    float3 R123 = clamp(R12 * R23, 1e-5, 0.9999);
    float3 r123 = sqrt(R123);
    float3 Rs = T121 * T121 * R23 / (float3(1.0) - R123);

    float3 C0 = R12 + Rs;
    float3 I = C0;
    float3 Cm = Rs - T121;

    for (int m = 1; m <= 2; ++m)
    {
        Cm *= r123;
        float3 Sm = 2.0 * evalSensitivity(float(m) * OPD, float(m) * phi);
        I += Cm * Sm;
    }
    return max(I, float3(0.0));
}

public float GTR1(float NDotH, float a)
{
    if (a >= 1.0) return (1.0 / PI);
    float a2 = a * a;
    float t = 1.0 + (a2 - 1.0) * NDotH * NDotH;
    return (a2 - 1.0) / (PI * log(a2) * t);
}

public float GTR2(float NDotH, float a)
{
    float a2 = a * a;
    float t = 1.0 + (a2 - 1.0) * NDotH * NDotH;
    return a2 / (PI * t * t);
}

public float SmithGGX(float NDotv, float alphaG)
{
    float a = alphaG * alphaG;
    float b = NDotv * NDotv;
    return 1.0 / (NDotv + sqrt(a + b - a * b));
}

public float power_heuristic(float a, float b)
{
    float t = a * a;
    return t / (b * b + t);
}

} // namespace utils
