#ifndef RAY_BSDF_DISNEY_GLSL
#define RAY_BSDF_DISNEY_GLSL

/*
 * MIT License
 *
 * Copyright(c) 2019-2021 Asif Ali
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this softwareand associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// rnd(state)
#include "../utils/random.glsl"

// bsdf utils
#include "../utils/bsdf.glsl"

// for material_t
#include "types.glsl"

//! simple struct to group results from a bsdf-sample
struct bsdf_sample_t
{
    // reflect/refract direction
    vec3 direction;

    // reflect/scatter-value
    vec3 F;

    // probability density
    float pdf;

    // sample passed through a surface
    bool transmission;
};

vec3 EvalDielectricReflection(const in material_t material, float eta, vec3 V, vec3 N, vec3 L, vec3 H, out float pdf)
{
    pdf = 0.0;
    if (dot(N, L) <= 0.0)
        return vec3(0.0);

    float F = DielectricFresnel(dot(V, H), eta);
    float D = GTR2(dot(N, H), material.roughness);

    pdf = D * dot(N, H) * F / (4.0 * abs(dot(V, H)));

    float G = SmithGGX(abs(dot(N, L)), material.roughness) * SmithGGX(abs(dot(N, V)), material.roughness);
    
    return material.color.rgb * F * D * G;
}

vec3 EvalDielectricReflectionIridescence(const in material_t material, vec3 Cspec0, float eta, vec3 V, vec3 N, vec3 L,
                                         vec3 H, out float pdf)
{
    pdf = 0.0;
    if (dot(N, L) <= 0.0)
    return vec3(0.0);

    float VoH = dot(V, H);
    float F = DielectricFresnel(VoH, eta);

    float D = GTR2(dot(N, H), material.roughness);

    pdf = D * dot(N, H) * F / (4.0 * abs(dot(V, H)));

    float G = SmithGGX(abs(dot(N, L)), material.roughness) * SmithGGX(abs(dot(N, V)), material.roughness);
    vec3 irF = iridescent_fresnel(1.0, material.iridescence_ior, Cspec0, material.iridescence_thickness_range.y, VoH);
    return material.color.rgb * irF * D * G;
}

vec3 EvalDielectricRefraction(const in material_t material, float eta, vec3 V, vec3 N, vec3 L, vec3 H, out float pdf)
{
    pdf = 0.0;
    if (dot(N, L) >= 0.0)
        return vec3(0.0);

    float F = DielectricFresnel(abs(dot(V, H)), eta);
    float D = GTR2(dot(N, H), material.roughness);

    float denomSqrt = dot(L, H) + dot(V, H) * eta;
    pdf = D * dot(N, H) * (1.0 - F) * abs(dot(L, H)) / (denomSqrt * denomSqrt);

    float G = SmithGGX(abs(dot(N, L)), material.roughness) * SmithGGX(abs(dot(N, V)), material.roughness);

    return material.color.rgb * (1.0 - F) * D * G * abs(dot(V, H)) * abs(dot(L, H)) * 4.0 * eta * eta / (denomSqrt * denomSqrt);
}

vec3 EvalSpecular(const in material_t material, in vec3 Cspec0, vec3 V, vec3 N, vec3 L, vec3 H, out float pdf)
{
    pdf = 0.0;
    if (dot(N, L) <= 0.0)
        return vec3(0.0);

    float D = GTR2(dot(N, H), material.roughness);
    pdf = D * dot(N, H) / (4.0 * dot(V, H));

    float FH = SchlickFresnel(dot(L, H));
    vec3 F = mix(Cspec0, vec3(1.0), FH);
    float G = SmithGGX(abs(dot(N, L)), material.roughness) * SmithGGX(abs(dot(N, V)), material.roughness);

    return F * D * G;
}

vec3 EvalSpecularIridescence(const in material_t material, in vec3 Cspec0, vec3 V, vec3 N, vec3 L, vec3 H, out float pdf)
{
    pdf = 0.0;
    if (dot(N, L) <= 0.0)
    return vec3(0.0);

    float VoH = dot(V, H);
    float D = GTR2(dot(N, H), material.roughness);
    pdf = D * dot(N, H) / (4.0 * VoH);
    vec3 F = iridescent_fresnel(1.0, material.iridescence_ior, Cspec0, material.iridescence_thickness_range.y, VoH);
    float G = SmithGGX(abs(dot(N, L)), material.roughness) * SmithGGX(abs(dot(N, V)), material.roughness);
    return F * D * G;
}

vec3 EvalClearcoat(const in material_t material, vec3 V, vec3 N, vec3 L, vec3 H, out float pdf)
{
    pdf = 0.0;

    if (dot(N, L) <= 0.0)
        return vec3(0.0);

//    float D = GTR1(dot(N, H), mix(0.1, 0.001, clearcoatGloss));
    float D = GTR2(dot(N, H), material.clearcoat_roughness);
    pdf = D * dot(N, H) / (4.0 * dot(V, H));
    float FH = SchlickFresnel(dot(L, H));
    float F = mix(0.04, 1.0, FH);
    float G = SmithGGX(dot(N, L), 0.25) * SmithGGX(dot(N, V), 0.25);
    return vec3(0.25 * material.clearcoat * F * D * G);
}


vec3 EvalDiffuse(const in material_t material, vec3 V, vec3 N, vec3 L, vec3 H, out float pdf)
{
    pdf = 0.0;
    float NoL = dot(N, L);

    if (NoL <= 0.0)
        return vec3(0.0);

    pdf = NoL * ONE_OVER_PI;

    // Diffuse
    float LoH = dot(L, H);
    float FL = SchlickFresnel(NoL);
    float FV = SchlickFresnel(dot(N, V));
    float FH = SchlickFresnel(LoH);
    float Fd90 = 0.5 + 2.0 * LoH * LoH * material.roughness;
    float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

    vec3 Fsheen = FH * material.sheen_roughness * material.sheen_color.rgb;
    return ONE_OVER_PI * (Fd * material.color.rgb + Fsheen) * (1.0 - material.metalness);
}


bsdf_sample_t sample_disney(in material_t material, vec3 N, vec3 V, float eta, inout uint rng_state)
{
    bsdf_sample_t ret;
    ret.pdf = 0.0;
    ret.F = vec3(0.0);
    ret.transmission = false;

    float diffuseRatio = 0.5 * (1.0 - material.metalness);
    float trans_weight = (1.0 - material.metalness) * material.transmission;

    material.roughness = max(0.001, material.roughness * material.roughness);
    material.sheen_roughness = max(0.001, material.sheen_roughness);

//    vec3 Cdlin = material.color.rgb;
//    float Cdlum = 0.3 * Cdlin.x + 0.6 * Cdlin.y + 0.1 * Cdlin.z; // luminance approx.

//    vec3 Ctint = Cdlum > 0.0 ? Cdlin / Cdlum : vec3(1.0f); // normalize lum. to isolate hue+sat
//    vec3 Cspec0 = mix(/*material.color.w * */ 0.08 * mix(vec3(1.0), Ctint, specularTint), Cdlin, material.metalness);

    vec3 spec_color = mix(vec3(0.08), material.color.rgb, material.metalness);

    mat3 frame = local_frame(N);

    vec2 Xi = vec2(rnd(rng_state), rnd(rng_state));

    // possible half-vector from GGX distribution
    vec3 H = frame * sample_GGX(Xi, material.roughness);
    if (dot(V, H) < 0.0){ H = -H; }

    vec3 H_vndf = frame * sample_GGX_VNDF(Xi, transpose(frame) * V, vec2(material.roughness));
    if (dot(V, H_vndf) < 0.0){ H_vndf = -H_vndf; }

    // transmission
    if (rnd(rng_state) < trans_weight)
    {
        vec3 R = reflect(-V, H_vndf);
        float F = DielectricFresnel(abs(dot(R, H_vndf)), eta);

        // Reflection/Total internal reflection
        if (Xi.y < F)
        {
            ret.direction = normalize(R);

            if(rnd(rng_state) < material.iridescence_strength)
            {
                ret.F = EvalDielectricReflectionIridescence(material, spec_color, eta, V, N, ret.direction, H_vndf, ret.pdf);
                ret.pdf *= material.iridescence_strength;
            }
            else
            {
                ret.F = EvalDielectricReflection(material, eta, V, N, ret.direction, H_vndf, ret.pdf);
                ret.pdf *= 1.0 - material.iridescence_strength;
            }
        }
        else // Transmission
        {
            material.roughness = min(material.roughness, .4);
            ret.direction = normalize(refract(-V, H, eta));
            ret.F = EvalDielectricRefraction(material, eta, V, N, ret.direction, H, ret.pdf);
            ret.transmission = true;
        }

        ret.F *= trans_weight;
        ret.pdf *= trans_weight;
    }
    // reflection
    else
    {
        // reflection - diffuse
        if (rnd(rng_state) < diffuseRatio)
        {
            ret.direction = frame * sample_hemisphere_cosine(Xi);

            H = normalize(ret.direction + V);

            ret.F = EvalDiffuse(material, V, N, ret.direction, H, ret.pdf);
            ret.pdf *= diffuseRatio;
        }
        // reflection - specular
        else
        {
            float primarySpecRatio = 1.0 / (1.0 + material.clearcoat);

            // reflection - specular (primary)
            if (rnd(rng_state) < primarySpecRatio)
            {
                ret.direction = normalize(reflect(-V, H_vndf));

                // reflection - specular (primary) -> iridescence
                if(rnd(rng_state) < material.iridescence_strength)
                {
                    ret.F = EvalSpecularIridescence(material, spec_color, V, N, ret.direction, H_vndf, ret.pdf);
                    ret.pdf *= primarySpecRatio * (1.0 - diffuseRatio) * material.iridescence_strength;
                }
                // reflection - specular (primary) -> regular
                else
                {
                    ret.F = EvalSpecular(material, spec_color, V, N, ret.direction, H_vndf, ret.pdf);
                    ret.pdf *= primarySpecRatio * (1.0 - diffuseRatio) * (1.0 - material.iridescence_strength);
                }
            }
            // reflection - specular (clearcoat)
            else
            {
                H = frame * sample_GTR1(Xi, mix(0.1, 0.001, material.clearcoat));
                if (dot(V, H) < 0.0){ H = -H; }

                ret.direction = normalize(reflect(-V, H));

                ret.F = EvalClearcoat(material, V, N, ret.direction, H, ret.pdf);
                ret.pdf *= (1.0 - primarySpecRatio) * (1.0 - diffuseRatio);
            }
        }

        ret.F *= (1.0 - trans_weight);
        ret.pdf *= (1.0 - trans_weight);
    }

    // TODO: make nan-correction obsolete
    ret.pdf = isnan(ret.pdf) ? 0.0 : ret.pdf;
    return ret;
}
    
vec3 eval_disney(in material_t material, vec3 L, vec3 N, vec3 V, float eta, inout float pdf)
{
    bool refl = dot(N, L) > 0.0;
    vec3 H = normalize(refl ? L + V : L + V * eta);

    if (dot(V, H) < 0.0)
        H = -H;

    float diffuseRatio = 0.5 * (1.0 - material.metalness);
    float primarySpecRatio = 1.0 / (1.0 + material.clearcoat);
    float trans_weight = (1.0 - material.metalness) * material.transmission;

    material.roughness = max(0.001, material.roughness * material.roughness);
    material.sheen_roughness = max(0.001, material.sheen_roughness);

    vec3 spec_color = mix(vec3(0.08), material.color.rgb, material.metalness);

    vec3 brdf = vec3(0.0);
    vec3 bsdf = vec3(0.0);
    float brdfPdf = 0.0;
    float bsdfPdf = 0.0;

    float m_pdf;

    if (trans_weight > 0.0)
    {
        // Reflection
        if (refl)
        {
            bsdf += EvalDielectricReflection(material, eta, V, N, L, H, m_pdf) * (1.0 - material.iridescence_strength);
            bsdfPdf += m_pdf * (1.0 - material.iridescence_strength);
            bsdf += EvalDielectricReflectionIridescence(material, spec_color, eta, V, N, L, H, m_pdf) * material.iridescence_strength;
            bsdfPdf += m_pdf * material.iridescence_strength;

        }
        else // Transmission
        {
            bsdf = EvalDielectricRefraction(material, eta, V, N, L, H, bsdfPdf);
        }
    }

    if (trans_weight < 1.0)
    {
        vec3 Cdlin = material.color.rgb;
        float Cdlum = 0.3 * Cdlin.x + 0.6 * Cdlin.y + 0.1 * Cdlin.z; // luminance approx.

        vec3 spec_color = mix(vec3(0.08), material.color.rgb, material.metalness);

        // Diffuse
        brdf += EvalDiffuse(material, V, N, L, H, m_pdf);
        brdfPdf += m_pdf * diffuseRatio;

        // Specular
        brdf += EvalSpecular(material, spec_color, V, N, L, H, m_pdf) * (1.0 - material.iridescence_strength);
        brdfPdf += m_pdf * primarySpecRatio * (1.0 - diffuseRatio) * (1.0 - material.iridescence_strength);

        // Specular (iridescence)
        brdf += EvalSpecularIridescence(material, spec_color, V, N, L, H, m_pdf) * material.iridescence_strength;
        brdfPdf += m_pdf * primarySpecRatio * (1.0 - diffuseRatio) * material.iridescence_strength;

        // Clearcoat
        brdf += EvalClearcoat(material, V, N, L, H, m_pdf);
        brdfPdf += m_pdf * (1.0 - primarySpecRatio) * (1.0 - diffuseRatio);
    }

    pdf = mix(brdfPdf, bsdfPdf, trans_weight);
    return mix(brdf, bsdf, trans_weight);
}

#endif // RAY_BSDF_DISNEY_GLSL