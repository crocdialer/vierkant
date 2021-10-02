// rnd(state)
#include "../utils/random.glsl"

// microfacet-math
#include "../renderer/bsdf_common.glsl"

// for material_t
#include "types.glsl"

vec3 UE4Eval(in vec3 L, in vec3 N, in vec3 V, in vec3 albedo, float roughness, float metalness)
{
    float NoL = dot(N, L);
    float NoV = dot(N, V);

    if (NoL <= 0.0 || NoV <= 0.0){ return vec3(0.0); }

    vec3 H = normalize(L + V);
    float NoH = dot(N, H);
    float LoH = dot(L, H);

    // Specular
    vec3 specularCol = mix(vec3(0.04), albedo, metalness);
    float a = max(0.001, roughness);
    float D = GTR2(NoH, a);
    float FH = SchlickFresnel(LoH);
    vec3 F = mix(specularCol, vec3(1.0), FH);
    float roughg = (roughness * 0.5 + 0.5);
    roughg = roughg * roughg;

    float G = SmithGGX(NoL, roughg) * SmithGGX(NoV, roughg);

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

bsdf_sample_t sample_UE4(in vec3 N,
                         in vec3 V,
                         in vec3 albedo,
                         in float roughness,
                         in float metalness,
                         inout uint rngState)
{
    bsdf_sample_t ret;

    ret.transmission = false;

    // local coordinate-frame
    mat3 local_basis = local_frame(N);

    vec2 Xi = vec2(rnd(rngState), rnd(rngState));

    // possible half-vector from (visible) GGX distribution
    // take V into local_basis (mutliplied by transpose)
    vec3 H = local_basis * sample_GGX_VNDF(Xi, V * local_basis, vec2(roughness));
//    vec3 H = local_basis * sample_GGX(Xi, roughness);

    // no diffuse rays for metal
    float diffuse_ratio = 0.5 * (1.0 - metalness);
    float reflect_prob = rnd(rngState);

//    const bool hit_front = gl_HitKindEXT == gl_HitKindFrontFacingTriangleEXT;

    // diffuse or transmission case. no internal reflections
//    if (payload.inside_media || reflect_prob < diffuse_ratio)
    if (reflect_prob < diffuse_ratio)
    {
//        float transmission_prob = hit_front ? rnd(rngState) : 0.0;
//
//        if (transmission_prob < material.transmission)
//        {
//            float ior = hit_front ? material.ior : 1.0;
//
//            // volume attenuation
//            payload.beta *= transmittance(payload.attenuation, payload.attenuation_distance, gl_HitTEXT);
//
//            payload.attenuation = hit_front ? material.attenuation_color.rgb : vec3(1);
//            payload.attenuation_distance = material.attenuation_distance;
//            payload.inside_media = hit_front;
//
//            // transmission/refraction
//            float eta = payload.ior / ior;
//            payload.ior = ior;
//
//            // refraction into medium
//            payload.ray.direction = refract(gl_WorldRayDirectionEXT, H, eta);
//
//            payload.normal *= -1.0;
//
//            // TODO: doesn't make any sense here
//            //            V = reflect(payload.ray.direction, payload.normal);
//            V = faceforward(V, gl_WorldRayDirectionEXT, payload.normal);
//        }
//        else
        {
            // diffuse reflection
            ret.direction = local_basis * sample_cosine(Xi);
        }
    }
    else
    {
        // surface/glossy reflection
        ret.direction = reflect(-V, H);
    }

    ret.F = UE4Eval(ret.direction, N, V, albedo, roughness, metalness);
    ret.pdf = UE4Pdf(ret.direction, N, V, roughness, metalness);
    return ret;
}