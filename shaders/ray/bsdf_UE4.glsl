#include "../renderer/bsdf_common.glsl"

vec3 UE4Eval(in vec3 L, in vec3 N, in vec3 V, in vec3 albedo,
float roughness, float metalness)
{
    float NDotL = dot(N, L);
    float NDotV = dot(N, V);

    if (NDotL <= 0.0 || NDotV <= 0.0){ return vec3(0.0); }

    vec3 H = normalize(L + V);
    float NDotH = dot(N, H);
    float LDotH = dot(L, H);

    // Specular
    vec3 specularCol = mix(vec3(0.04), albedo, metalness);
    float a = max(0.001, roughness);
    float D = GTR2(NDotH, a);
    float FH = SchlickFresnel(LDotH);
    vec3 F = mix(specularCol, vec3(1.0), FH);
    float roughg = (roughness * 0.5 + 0.5);
    roughg = roughg * roughg;

    float G = SmithGGX(NDotL, roughg) * SmithGGX(NDotV, roughg);

    // Diffuse + Specular components
    return (ONE_OVER_PI * albedo) * (1.0 - metalness) + F * D * G;
}

/*
 *	Based on    https://github.com/knightcrawler25/GLSL-PathTracer
 *  UE4 SIGGAPH https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
 */

float UE4Pdf(in vec3 L, in vec3 N, in vec3 V, float roughness, float metalness)
{
    float specularAlpha = max(0.001, roughness);

    float diffuseRatio = 0.5 * (1.0 - metalness);
    float specularRatio = 1.0 - diffuseRatio;

    vec3 halfVec = normalize(L + V);

    float cosTheta = abs(dot(halfVec, N));
    float pdfGTR2 = GTR2(cosTheta, specularAlpha) * cosTheta;

    // calculate diffuse and specular pdfs and mix ratio
    float pdfSpec = pdfGTR2 / (4.0 * abs(dot(L, halfVec)));
    float pdfDiff = abs(dot(L, N)) * ONE_OVER_PI;

    // weight pdfs according to ratios
    return diffuseRatio * pdfDiff + specularRatio * pdfSpec;
}

bsdf_sample_t sample_UE4(in vec3 L,
                         in vec3 N,
                         in vec3 V,
                         in vec3 albedo,
                         in float roughness,
                         in float metalness)
{
    bsdf_sample_t ret;
    ret.direction = L;
    ret.F = UE4Eval(L, N, V, albedo, roughness, metalness);
    ret.pdf = UE4Pdf(L, N, V, roughness, metalness);
    return ret;
}