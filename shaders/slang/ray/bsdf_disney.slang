/*
 * MIT License
 *
 * Copyright(c) 2019-2021 Asif Ali
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

module bsdf_disney;

import "../utils/random.slang";
import "../utils/bsdf.slang";
import "../utils/sampling.slang";
import "../utils/constants.slang";
import "types.slang";

namespace ray
{

public struct bsdf_sample_t
{
    public float3 direction;
    public float pdf;
    public float3 F;
    public bool transmission;
};

float3 EvalDielectricReflection(material_t material, float eta, float3 V, float3 N, float3 L, float3 H, out float pdf)
{
    pdf = 0.0;
    if (dot(N, L) <= 0.0) return float3(0.0);

    float F = utils::DielectricFresnel(dot(V, H), eta);
    float D = utils::GTR2(dot(N, H), material.roughness);
    pdf = D * dot(N, H) * F / (4.0 * abs(dot(V, H)));

    float G = utils::SmithGGX(abs(dot(N, L)), material.roughness) * utils::SmithGGX(abs(dot(N, V)), material.roughness);
    return material.color.rgb * F * D * G;
}

float3 EvalDielectricReflectionIridescence(material_t material, float3 Cspec0, float eta, float3 V, float3 N, float3 L, float3 H, out float pdf)
{
    pdf = 0.0;
    if (dot(N, L) <= 0.0) return float3(0.0);

    float VoH = dot(V, H);
    float F = utils::DielectricFresnel(VoH, eta);
    float D = utils::GTR2(dot(N, H), material.roughness);
    pdf = D * dot(N, H) * F / (4.0 * abs(dot(V, H)));

    float G = utils::SmithGGX(abs(dot(N, L)), material.roughness) * utils::SmithGGX(abs(dot(N, V)), material.roughness);
    float3 irF = utils::iridescent_fresnel(1.0, material.iridescence_ior, Cspec0, material.iridescence_thickness_range.y, VoH);
    return material.color.rgb * irF * D * G;
}

float3 EvalDielectricRefraction(material_t material, float eta, float3 V, float3 N, float3 L, float3 H, out float pdf)
{
    pdf = 0.0;
    if (dot(N, L) >= 0.0) return float3(0.0);

    float F = utils::DielectricFresnel(abs(dot(V, H)), eta);
    float D = utils::GTR2(dot(N, H), material.roughness);
    float denomSqrt = dot(L, H) + dot(V, H) * eta;
    pdf = D * dot(N, H) * (1.0 - F) * abs(dot(L, H)) / (denomSqrt * denomSqrt);

    float G = utils::SmithGGX(abs(dot(N, L)), material.roughness) * utils::SmithGGX(abs(dot(N, V)), material.roughness);
    return material.color.rgb * (1.0 - F) * D * G * abs(dot(V, H)) * abs(dot(L, H)) * 4.0 * eta * eta / (denomSqrt * denomSqrt);
}

float3 EvalSpecular(material_t material, float3 Cspec0, float3 V, float3 N, float3 L, float3 H, out float pdf)
{
    pdf = 0.0;
    if (dot(N, L) <= 0.0) return float3(0.0);

    float D = utils::GTR2(dot(N, H), material.roughness);
    pdf = D * dot(N, H) / (4.0 * dot(V, H));

    float FH = utils::SchlickFresnel(dot(L, H));
    float3 F = lerp(Cspec0, float3(1.0), FH);
    float G = utils::SmithGGX(abs(dot(N, L)), material.roughness) * utils::SmithGGX(abs(dot(N, V)), material.roughness);
    return F * D * G;
}

float3 EvalSpecularIridescence(material_t material, float3 Cspec0, float3 V, float3 N, float3 L, float3 H, out float pdf)
{
    pdf = 0.0;
    if (dot(N, L) <= 0.0) return float3(0.0);

    float VoH = dot(V, H);
    float D = utils::GTR2(dot(N, H), material.roughness);
    pdf = D * dot(N, H) / (4.0 * VoH);
    float3 F = utils::iridescent_fresnel(1.0, material.iridescence_ior, Cspec0, material.iridescence_thickness_range.y, VoH);
    float G = utils::SmithGGX(abs(dot(N, L)), material.roughness) * utils::SmithGGX(abs(dot(N, V)), material.roughness);
    return F * D * G;
}

float3 EvalClearcoat(material_t material, float3 V, float3 N, float3 L, float3 H, out float pdf)
{
    pdf = 0.0;
    if (dot(N, L) <= 0.0) return float3(0.0);

    float D = utils::GTR2(dot(N, H), material.clearcoat_roughness);
    pdf = D * dot(N, H) / (4.0 * dot(V, H));
    float FH = utils::SchlickFresnel(dot(L, H));
    float F = lerp(0.04, 1.0, FH);
    float G = utils::SmithGGX(dot(N, L), 0.25) * utils::SmithGGX(dot(N, V), 0.25);
    return float3(0.25 * material.clearcoat * F * D * G);
}

float3 EvalDiffuse(material_t material, float3 V, float3 N, float3 L, float3 H, out float pdf)
{
    pdf = 0.0;
    float NoL = dot(N, L);
    if (NoL <= 0.0) return float3(0.0);

    pdf = NoL * utils::ONE_OVER_PI;

    float LoH = dot(L, H);
    float FL = utils::SchlickFresnel(NoL);
    float FV = utils::SchlickFresnel(dot(N, V));
    float FH = utils::SchlickFresnel(LoH);
    float Fd90 = 0.5 + 2.0 * LoH * LoH * material.roughness;
    float Fd = lerp(1.0, Fd90, FL) * lerp(1.0, Fd90, FV);

    float3 Fsheen = FH * material.sheen_roughness * material.sheen_color.rgb;
    return utils::ONE_OVER_PI * (Fd * material.color.rgb + Fsheen) * (1.0 - material.metalness);
}

public bsdf_sample_t sample_disney(material_t material, float3 N, float3 V, float eta, inout uint rng_state)
{
    bsdf_sample_t ret;
    ret.pdf = 0.0;
    ret.F = float3(0.0);
    ret.transmission = false;

    float diffuseRatio = 0.5 * (1.0 - material.metalness);
    float trans_weight = (1.0 - material.metalness) * material.transmission;

    material.roughness = max(0.001, material.roughness * material.roughness);
    material.sheen_roughness = max(0.001, material.sheen_roughness);

    float3 spec_color = lerp(float3(0.08), material.color.rgb, material.metalness);

    float3x3 frame = utils::local_frame(N);

    float2 Xi = float2(utils::rnd(rng_state), utils::rnd(rng_state));

    float3 H = mul(frame, utils::sample_GGX(Xi, material.roughness));
    if (dot(V, H) < 0.0) { H = -H; }

    float3 H_vndf = mul(frame, utils::sample_GGX_VNDF(Xi, mul(transpose(frame), V), float2(material.roughness)));
    if (dot(V, H_vndf) < 0.0) { H_vndf = -H_vndf; }

    // transmission
    if (utils::rnd(rng_state) < trans_weight)
    {
        float3 R = reflect(-V, H_vndf);
        float F = utils::DielectricFresnel(abs(dot(R, H_vndf)), eta);

        // Reflection/Total internal reflection
        if (Xi.y < F)
        {
            ret.direction = normalize(R);

            if (utils::rnd(rng_state) < material.iridescence_strength)
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
            material.roughness = min(material.roughness, 0.4);
            ret.direction = normalize(refract(-V, H, eta));
            ret.F = EvalDielectricRefraction(material, eta, V, N, ret.direction, H, ret.pdf);
            ret.transmission = true;
        }

        ret.F *= trans_weight;
        ret.pdf *= trans_weight;
    }
    else // reflection
    {
        // diffuse
        if (utils::rnd(rng_state) < diffuseRatio)
        {
            ret.direction = mul(frame, utils::sample_hemisphere_cosine(Xi));
            H = normalize(ret.direction + V);
            ret.F = EvalDiffuse(material, V, N, ret.direction, H, ret.pdf);
            ret.pdf *= diffuseRatio;
        }
        else // specular
        {
            float primarySpecRatio = 1.0 / (1.0 + material.clearcoat);

            // primary specular
            if (utils::rnd(rng_state) < primarySpecRatio)
            {
                ret.direction = normalize(reflect(-V, H_vndf));

                if (utils::rnd(rng_state) < material.iridescence_strength)
                {
                    ret.F = EvalSpecularIridescence(material, spec_color, V, N, ret.direction, H_vndf, ret.pdf);
                    ret.pdf *= primarySpecRatio * (1.0 - diffuseRatio) * material.iridescence_strength;
                }
                else
                {
                    ret.F = EvalSpecular(material, spec_color, V, N, ret.direction, H_vndf, ret.pdf);
                    ret.pdf *= primarySpecRatio * (1.0 - diffuseRatio) * (1.0 - material.iridescence_strength);
                }
            }
            else // clearcoat
            {
                H = mul(frame, utils::sample_GTR1(Xi, lerp(0.1, 0.001, material.clearcoat)));
                if (dot(V, H) < 0.0) { H = -H; }
                ret.direction = normalize(reflect(-V, H));
                ret.F = EvalClearcoat(material, V, N, ret.direction, H, ret.pdf);
                ret.pdf *= (1.0 - primarySpecRatio) * (1.0 - diffuseRatio);
            }
        }

        ret.F *= (1.0 - trans_weight);
        ret.pdf *= (1.0 - trans_weight);
    }

    ret.pdf = isnan(ret.pdf) ? 0.0 : ret.pdf;
    return ret;
}

public float3 eval_disney(material_t material, float3 L, float3 N, float3 V, float eta, inout float pdf)
{
    bool refl = dot(N, L) > 0.0;
    float3 H = normalize(refl ? L + V : L + V * eta);
    if (dot(V, H) < 0.0) H = -H;

    float diffuseRatio = 0.5 * (1.0 - material.metalness);
    float primarySpecRatio = 1.0 / (1.0 + material.clearcoat);
    float trans_weight = (1.0 - material.metalness) * material.transmission;

    material.roughness = max(0.001, material.roughness * material.roughness);
    material.sheen_roughness = max(0.001, material.sheen_roughness);

    float3 spec_color = lerp(float3(0.08), material.color.rgb, material.metalness);

    float3 brdf = float3(0.0);
    float3 bsdf = float3(0.0);
    float brdfPdf = 0.0;
    float bsdfPdf = 0.0;
    float m_pdf;

    if (trans_weight > 0.0)
    {
        if (refl)
        {
            bsdf += EvalDielectricReflection(material, eta, V, N, L, H, m_pdf) * (1.0 - material.iridescence_strength);
            bsdfPdf += m_pdf * (1.0 - material.iridescence_strength);
            bsdf += EvalDielectricReflectionIridescence(material, spec_color, eta, V, N, L, H, m_pdf) * material.iridescence_strength;
            bsdfPdf += m_pdf * material.iridescence_strength;
        }
        else
        {
            bsdf = EvalDielectricRefraction(material, eta, V, N, L, H, bsdfPdf);
        }
    }

    if (trans_weight < 1.0)
    {
        brdf += EvalDiffuse(material, V, N, L, H, m_pdf);
        brdfPdf += m_pdf * diffuseRatio;

        brdf += EvalSpecular(material, spec_color, V, N, L, H, m_pdf) * (1.0 - material.iridescence_strength);
        brdfPdf += m_pdf * primarySpecRatio * (1.0 - diffuseRatio) * (1.0 - material.iridescence_strength);

        brdf += EvalSpecularIridescence(material, spec_color, V, N, L, H, m_pdf) * material.iridescence_strength;
        brdfPdf += m_pdf * primarySpecRatio * (1.0 - diffuseRatio) * material.iridescence_strength;

        brdf += EvalClearcoat(material, V, N, L, H, m_pdf);
        brdfPdf += m_pdf * (1.0 - primarySpecRatio) * (1.0 - diffuseRatio);
    }

    pdf = lerp(brdfPdf, bsdfPdf, trans_weight);
    return lerp(brdf, bsdf, trans_weight);
}

} // namespace ray
