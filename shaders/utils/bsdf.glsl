#ifndef UTILS_BSDF_GLSL
#define UTILS_BSDF_GLSL

#include "sampling.glsl"

/*
 * Schlick's approximation of the specular reflection coefficient R
 * (1 - cosTheta)^5
 */
float SchlickFresnel(float u)
{
    float m = clamp(1.0 - u, 0.0, 1.0);
    return m * m * m * m * m; // power of 5
}

float F_Schlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 F_Schlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float DielectricFresnel(float cos_theta_i, float eta)
{
    float sinThetaTSq = eta * eta * (1.0f - cos_theta_i * cos_theta_i);

    // Total internal reflection
    if (sinThetaTSq > 1.0)
    return 1.0;

    float cos_theta_t = sqrt(max(1.0 - sinThetaTSq, 0.0));

    float rs = (eta * cos_theta_t - cos_theta_i) / (eta * cos_theta_t + cos_theta_i);
    float rp = (eta * cos_theta_i - cos_theta_t) / (eta * cos_theta_i + cos_theta_t);

    return 0.5f * (rs * rs + rp * rp);
}

// iridescence fresnel ////////
// see: https://github.com/ux3d/glTF/tree/extensions/KHR_materials_iridescence/extensions/2.0/Khronos/KHR_materials_iridescence#theory-documentation-and-implementations

vec3 iridescent_fresnel_mix(vec3 iridescence_fresnel, vec3 base, vec3 specular_brdf)
{
    // Get maximum component value of iridescence fresnel color
    float iridescence_fresnel_max = max(max(iridescence_fresnel.r, iridescence_fresnel.g), iridescence_fresnel.b);

    return (1 - iridescence_fresnel_max) * base + iridescence_fresnel * specular_brdf;
}

// Assume air interface for top
vec3 Fresnel0ToIor(vec3 F0)
{
    vec3 sqrtF0 = sqrt(F0);
    return (vec3(1.0) + sqrtF0) / (vec3(1.0) - sqrtF0);
}

float IorToFresnel0(float transmittedIor, float incidentIor)
{
    return pow((transmittedIor - incidentIor) / (transmittedIor + incidentIor), 2.0);
}

vec3 IorToFresnel0(vec3 transmittedIor, float incidentIor)
{
    vec3 v = (transmittedIor - incidentIor) / (transmittedIor + incidentIor);
    return v * v;
}

const mat3 XYZ_TO_REC709 = mat3(3.2404542, -0.9692660,  0.0556434,
                                -1.5371385,  1.8760108, -0.2040259,
                                -0.4985314,  0.0415560,  1.0572252);

vec3 evalSensitivity(float OPD, vec3 shift)
{
    float phase = 2.0 * PI * OPD * 1.0e-9;
    float phase2 = phase * phase;
    vec3 val = vec3(5.4856e-13, 4.4201e-13, 5.2481e-13);
    vec3 pos = vec3(1.6810e+06, 1.7953e+06, 2.2084e+06);
    vec3 var = vec3(4.3278e+09, 9.3046e+09, 6.6121e+09);

    vec3 xyz = val * sqrt(2.0 * PI * var) * cos(pos * phase + shift) * exp(-phase2 * var);
    xyz.x += 9.7470e-14 * sqrt(2.0 * PI * 4.5282e+09) * cos(2.2399e+06 * phase + shift[0]) * exp(-4.5282e+09 * phase2);
    xyz /= 1.0685e-7;

    vec3 rgb = XYZ_TO_REC709 * xyz;
    return rgb;
}

vec3 iridescent_fresnel(float outsideIOR, float iridescenceIOR, vec3 baseF0, float iridescenceThickness, float cosTheta1)
{
    vec3 F_iridescence = vec3(0.0);

    // Calculation of the iridescence Fresnel for the viewing angle theta1
    // First interface
    float R0 = IorToFresnel0(iridescenceIOR, outsideIOR);
    float R12 = F_Schlick(cosTheta1, R0);
    float R21 = R12;
    float T121 = 1.0 - R12;

    float sinTheta2Sq = pow(outsideIOR / iridescenceIOR, 2.0) * (1.0 - pow(cosTheta1, 2.0));
    float cosTheta2Sq = 1.0 - sinTheta2Sq;

    // Handle total internal reflection
    if (cosTheta2Sq < 0.0){ return vec3(1.0); }

    float cosTheta2 = sqrt(cosTheta2Sq);

    // Second interface
    vec3 baseIOR = Fresnel0ToIor(baseF0 + 0.0001); // guard against 1.0
    vec3 R1 = IorToFresnel0(baseIOR, iridescenceIOR);
    vec3 R23 = F_Schlick(cosTheta2, R1);

    // First interface
    float phi12 = 0.0;
    if (iridescenceIOR < outsideIOR){ phi12 = PI; }
    float phi21 = PI - phi12;

    // Second interface
    vec3 phi23 = vec3(0.0);
    if (baseIOR[0] < iridescenceIOR){ phi23[0] = PI; }
    if (baseIOR[1] < iridescenceIOR){ phi23[1] = PI; }
    if (baseIOR[2] < iridescenceIOR){ phi23[2] = PI; }

    // Phase shift
    vec3 phi = vec3(phi21) + phi23;

    // first-order optical path difference
    float OPD = 2.0 * iridescenceIOR * iridescenceThickness * cosTheta1;

    // perform spectral integration in Fourier-space

    // Compound terms
    vec3 R123 = clamp(R12 * R23, 1e-5, 0.9999);
    vec3 r123 = sqrt(R123);
    vec3 Rs = T121 * T121 * R23 / (vec3(1.0) - R123);

    // Reflectance term for m = 0 (DC term amplitude)
    vec3 C0 = R12 + Rs;
    vec3 I = C0;

    // Reflectance term for m > 0 (pairs of diracs)
    vec3 Cm = Rs - T121;

    for (int m = 1; m <= 2; ++m)
    {
        Cm *= r123;
        vec3 Sm = 2.0 * evalSensitivity(float(m) * OPD, float(m) * phi);
        I += Cm * Sm;
    }

    F_iridescence = max(I, vec3(0.0));

    return F_iridescence;
}

/////////////////

/*
 * Generalized-Trowbridge-Reitz (D)
 */
float GTR1(float NDotH, float a)
{
    if (a >= 1.0)
    return (1.0 / PI);

    float a2 = a * a;
    float t = 1.0 + (a2 - 1.0) * NDotH * NDotH;

    return (a2 - 1.0) / (PI * log(a2) * t);
}

/*
 * Generalized-Trowbridge-Reitz (D)
 * Describes differential area of microfacets for the surface normal
 */
float GTR2(float NDotH, float a)
{
    float a2 = a * a;
    float t = 1.0 + (a2 - 1.0) * NDotH * NDotH;
    return a2 / (PI * t * t);
}

/*
 * Smith's geometric masking/shadowing function for the GGX distribution (G)
 */
float SmithGGX(float NDotv, float alphaG)
{
    float a = alphaG * alphaG;
    float b = NDotv * NDotv;
    return 1.0 / (NDotv + sqrt(a + b - a * b));
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}
float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

/*
 * Power heuristic often reduces variance even further for multiple importance sampling
 * Chapter 13.10.1 of pbrbook
 */
float power_heuristic(float a, float b)
{
    float t = a * a;
    return t / (b * b + t);
}

vec3 transmittance(vec3 attenuation_color, float attenuation_distance, float distance)
{
    return exp(log(attenuation_color) / attenuation_distance * distance);
}

#endif // UTILS_BSDF_GLSL