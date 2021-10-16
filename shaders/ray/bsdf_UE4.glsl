// rnd(state)
#include "../utils/random.glsl"

// microfacet-math
#include "../renderer/bsdf_common.glsl"

// for material_t
#include "types.glsl"

vec3 eval_diffuse(vec3 albedo, float roughness, float metalness,vec3 V, vec3 N, vec3 L, inout float pdf)
{
    float NoL = dot(N, L);

    pdf = 0.0;
    if (NoL <= 0.0){ return vec3(0.0); }

    pdf = NoL * ONE_OVER_PI;
    return (ONE_OVER_PI * albedo) * (1.0 - metalness);
}

vec3 eval_spec(vec3 albedo, float roughness, float metalness, vec3 V, vec3 N, vec3 L, vec3 H, inout float pdf)
{
    float NoL = dot(N, L);
    float NoV = dot(N, V);

    pdf = 0.0;
    if (NoL <= 0.0 || NoV <= 0.0){ return vec3(0.0); }

    roughness = max(0.001, roughness);
    float NoH = dot(N, H);
    float LoH = dot(L, H);

    float D = GTR2(NoH, roughness);
    pdf = D * NoH / (4.0 * dot(V, H));

    float FH = SchlickFresnel(LoH);
    vec3 specularF0 = mix(vec3(0.04), albedo, metalness);
    vec3 F = mix(specularF0, vec3(1.0), FH);

    float roughg = (roughness * 0.5 + 0.5);
    roughg = roughg * roughg;

    float G = SmithGGX(NoL, roughness) * SmithGGX(NoV, roughg);

    // Specular F
    return F * D * G;
}

vec3 eval_refract(vec3 albedo, float roughness, float eta, vec3 V, vec3 N, vec3 L, vec3 H, inout float pdf)
{
    pdf = 0.0;
    if (dot(N, L) >= 0.0){ return vec3(0.0); }

    float F = DielectricFresnel(abs(dot(V, H)), eta);
    float D = GTR2(dot(N, H), roughness);

    float denomSqrt = dot(L, H) + dot(V, H) * eta;
    pdf = D * dot(N, H) * (1.0 - F) * abs(dot(L, H)) / (denomSqrt * denomSqrt);

    float G = SmithGGX(abs(dot(N, L)), roughness) * SmithGGX(abs(dot(N, V)), roughness);

    return albedo * (1.0 - F) * D * G * abs(dot(V, H)) * abs(dot(L, H)) * 4.0 * eta * eta / (denomSqrt * denomSqrt);
}

bsdf_sample_t sample_UE4(in material_t material,
                         in vec3 N,
                         in vec3 V,
                         float eta,
                         inout uint rngState)
{
    bsdf_sample_t ret;
    ret.transmission = false;

    // local coordinate-frame
    mat3 local_basis = local_frame(N);

    vec2 Xi = vec2(rnd(rngState), rnd(rngState));

    // possible half-vector from (visible) GGX distribution
    // take V into local_basis (mutliplied by transpose)
//    vec3 H = local_basis * sample_GGX_VNDF(Xi, V * local_basis, vec2(material.roughness));
    vec3 H = local_basis * sample_GGX(Xi, material.roughness);

    // flip half vector
    if (dot(V, H) < 0.0){ H = -H; }

    // no diffuse rays for metal
    float diffuse_ratio = 0.5 * (1.0 - material.metalness);

    // diffuse or transmission case. no internal reflections
    if (rnd(rngState) < diffuse_ratio)
    {
        if (rnd(rngState) < material.transmission)
        {
            ret.transmission = true;

            // refraction into medium
            ret.direction = normalize(refract(-V, H, eta));
            ret.F = eval_refract(material.color.rgb, material.roughness, eta, V, N, ret.direction, H, ret.pdf);
            ret.pdf *= diffuse_ratio * material.transmission;
        }
        else
        {
            // diffuse reflection
            ret.direction = local_basis * sample_cosine(Xi);

            ret.F = eval_diffuse(material.color.rgb, material.roughness, material.metalness, V, N, ret.direction, ret.pdf);
            ret.pdf *= diffuse_ratio;
        }
    }
    else
    {
        // surface/glossy reflection
        ret.direction = reflect(-V, H);

        ret.F = eval_spec(material.color.rgb, material.roughness, material.metalness, V, N, ret.direction, H, ret.pdf);
        ret.pdf *= 1.0 - diffuse_ratio;
    }

//    ret.F = UE4Eval(ret.direction, N, V, material.color.rgb, material.roughness, material.metalness);
//    ret.pdf = UE4Pdf(ret.direction, N, V, material.roughness, material.metalness);
    return ret;
}