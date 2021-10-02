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

#include "../renderer/bsdf_common.glsl"

vec3 EvalDielectricReflection(vec3 albedo, float roughness, vec3 V, vec3 N, vec3 L, vec3 H, inout float pdf)
{
    pdf = 0.0;
    if (dot(N, L) <= 0.0)
        return vec3(0.0);

    float eta = payload.eta;

    float F = DielectricFresnel(dot(V, H), eta);
    float D = GTR2(dot(N, H), roughness);

    pdf = D * dot(N, H) * F / (4.0 * abs(dot(V, H)));

    float G = SmithGGX(abs(dot(N, L)), roughness) * SmithGGX(abs(dot(N, V)), roughness);
    
    return albedo * F * D * G;
}


vec3 EvalDielectricRefraction(vec3 albedo, float roughness, float eta, vec3 V, vec3 N, vec3 L, vec3 H, inout float pdf)
{
    pdf = 0.0;
    if (dot(N, L) >= 0.0)
        return vec3(0.0);

    float F = DielectricFresnel(abs(dot(V, H)), eta);
    float D = GTR2(dot(N, H), roughness);

    float denomSqrt = dot(L, H) + dot(V, H) * eta;
    pdf = D * dot(N, H) * (1.0 - F) * abs(dot(L, H)) / (denomSqrt * denomSqrt);

    float G = SmithGGX(abs(dot(N, L)), roughness) * SmithGGX(abs(dot(N, V)), roughness);

    return albedo * (1.0 - F) * D * G * abs(dot(V, H)) * abs(dot(L, H)) * 4.0 * eta * eta / (denomSqrt * denomSqrt);
}

vec3 EvalSpecular(float roughness, in vec3 Cspec0, vec3 V, vec3 N, vec3 L, vec3 H, inout float pdf)
{
    pdf = 0.0;
    if (dot(N, L) <= 0.0)
        return vec3(0.0);

    float D = GTR2(dot(N, H), roughness);
    pdf = D * dot(N, H) / (4.0 * dot(V, H));

    float FH = SchlickFresnel(dot(L, H));
    vec3 F = mix(Cspec0, vec3(1.0), FH);
    float G = SmithGGX(abs(dot(N, L)), roughness) * SmithGGX(abs(dot(N, V)), roughness);

    return F * D * G;
}

vec3 EvalClearcoat(float clearcoat, float clearcoatGloss, vec3 V, vec3 N, vec3 L, vec3 H, inout float pdf)
{
    pdf = 0.0;
    if (dot(N, L) <= 0.0)
        return vec3(0.0);

    float D = GTR1(dot(N, H), mix(0.1, 0.001, clearcoatGloss));
    pdf = D * dot(N, H) / (4.0 * dot(V, H));

    float FH = SchlickFresnel(dot(L, H));
    float F = mix(0.04, 1.0, FH);
    float G = SmithGGX(dot(N, L), 0.25) * SmithGGX(dot(N, V), 0.25);

    return vec3(0.25 * clearcoat * F * D * G);
}


vec3 EvalDiffuse(vec3 albedo, float roughness, float metallic, float sheen_roughness, vec3 Csheen, vec3 V, vec3 N,
                 vec3 L, vec3 H, inout float pdf)
{
    pdf = 0.0;
    if (dot(N, L) <= 0.0)
        return vec3(0.0);

    pdf = dot(N, L) * ONE_OVER_PI;

    // Diffuse
    float FL = SchlickFresnel(dot(N, L));
    float FV = SchlickFresnel(dot(N, V));
    float FH = SchlickFresnel(dot(L, H));
    float Fd90 = 0.5 + 2.0 * dot(L, H) * dot(L, H) * roughness;
    float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

//    // Fake Subsurface TODO: Replace with volumetric scattering
//    float Fss90 = dot(L, H) * dot(L, H) * roughness;
//    float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
//    float ss = 1.25 * (Fss * (1.0 / (dot(N, L) + dot(N, V)) - 0.5) + 0.5);

    vec3 Fsheen = FH * sheen * Csheen;

//    return ((1.0 / PI) * mix(Fd, ss, material.subsurface) * material.albedo.xyz + Fsheen) * (1.0 - material.metallic);
    return (ONE_OVER_PI * Fd * albedo.xyz + Fsheen) * (1.0 - metallic);
}


bsdf_sample_t DisneySample(in Material material, vec3 N, vec3 V)
{
    bsdf_sample_t ret;
    ret.pdf = 0.0;
    ret.F = vec3(0.0);

//    pdf = 0.0;
//    vec3 f = vec3(0.0);

    // TODO: missing params
    //    vec3 N = payload.ffnormal;
    //    vec3 V = -gl_WorldRayDirectionEXT;
    vec4 albedo;
    float roughness;
    float metallic;
    float transmission;
    float eta;// = payload.eta;
    vec3 specularTint = vec3(1.0);
    vec3 sheenTint = vec3(0);
    float sheen_roughness = 0.0;

    float diffuseRatio = 0.5 * (1.0 - metallic);
    float transWeight = (1.0 - metallic) * transmission;

    vec3 Cdlin = albedo.xyz;
    float Cdlum = 0.3 * Cdlin.x + 0.6 * Cdlin.y + 0.1 * Cdlin.z; // luminance approx.

    vec3 Ctint = Cdlum > 0.0 ? Cdlin / Cdlum : vec3(1.0f); // normalize lum. to isolate hue+sat
    vec3 Cspec0 = mix(albedo.w * 0.08 * mix(vec3(1.0), Ctint, specularTint), Cdlin, metallic);
    vec3 Csheen = mix(vec3(1.0), Ctint, sheenTint);

    mat3 frame = local_frame(N);

    vec2 Xi = vec2(rnd(rngState), rnd(rngState));

//    float r1 = rnd(seed);
//    float r2 = rnd(seed);

    if (rnd(seed) < transWeight)
    {
        // possible half-vector from GGX distribution
        vec3 H = frame * sample_GGX(Xi, roughness);
//        vec3 H = frame * sample_GGX_VNDF(Xi, V * local_basis, vec2(roughness));

//        vec3 H = ImportanceSampleGTR2(roughness, r1, r2);
//        H = frame * H;

        if (dot(V, H) < 0.0)
            H = -H;

        vec3 R = reflect(-V, H);
        float F = DielectricFresnel(abs(dot(R, H)), eta);

        // Reflection/Total internal reflection
        if (Xi.y < F)
        {
            ret.direction = normalize(R);
            ret.F = EvalDielectricReflection(albedo, roughness, V, N, ret.direction, H, ret.pdf);
        }
        else // Transmission
        {
            ret.direction = normalize(refract(-V, H, eta));
            ret.F = EvalDielectricRefraction(albedo, roughness, eta, V, N, ret.direction, H, ret.pdf);
        }

        ret.F *= transWeight;
        ret.pdf *= transWeight;
    }
    else
    {
        if (rnd(seed) < diffuseRatio)
        {
            ret.direction = frame * sample_cosine(Xi);

            vec3 H = normalize(ret.direction + V);

            ret.F = EvalDiffuse(albedo, roughness, metallic, sheen_roughness, Csheen, V, N, ret.direction, H, ret.pdf);
            ret.pdf *= diffuseRatio;
        }
        else // Specular
        {
            float primarySpecRatio = 1.0 / (1.0 + material.clearcoat);

            // Sample primary specular lobe
//            if (rnd(seed) < primarySpecRatio)
            {
//                // TODO: Implement http://jcgt.org/published/0007/04/01/
//                vec3 H = ImportanceSampleGTR2(roughness, r1, r2);
//                H = frame * H;
                // possible half-vector from GGX distribution
                vec3 H = frame * sample_GGX(Xi, roughness);
                //        vec3 H = frame * sample_GGX_VNDF(Xi, V * local_basis, vec2(roughness));

                if (dot(V, H) < 0.0)
                    H = -H;

                ret.direction = normalize(reflect(-V, H));

                ret.F = EvalSpecular(material.roughness, Cspec0, V, N, ret.direction, H, ret.pdf);
                ret.pdf *= primarySpecRatio * (1.0 - diffuseRatio);
            }
//            else // Sample clearcoat lobe
//            {
//                vec3 H = ImportanceSampleGTR1(mix(0.1, 0.001, material.clearcoatGloss), r1, r2);
//                H = frame * H;
//
//                if (dot(V, H) < 0.0)
//                    H = -H;
//
//                L = normalize(reflect(-V, H));
//
//                f = EvalClearcoat(material, V, N, L, H, pdf);
//                pdf *= (1.0 - primarySpecRatio) * (1.0 - diffuseRatio);
//            }
        }

        ret.F *= (1.0 - transWeight);
        ret.pdf *= (1.0 - transWeight);
    }

    return ret;
}
    
vec3 DisneyEval(Material material, vec3 L, inout float pdf)
{
//    vec3 N = payload.ffnormal;
//    vec3 V = -gl_WorldRayDirectionEXT;
    vec4 albedo;
    float roughness;
    float metallic;
    float transmission;
    float eta;// = payload.eta;
    vec3 specularTint = vec3(1.0);
    vec3 sheenTint = vec3(0);
    float sheen_roughness = 0.0;
    float eta = payload.eta;

    vec3 H;
    bool refl = dot(N, L) > 0.0;

//    if (refl)
//        H = normalize(L + V);
//    else
//        H = normalize(L + V * eta);

    H = normalize(refl ? L + V : L + V * eta);

    if (dot(V, H) < 0.0)
        H = -H;

    float diffuseRatio = 0.5 * (1.0 - material.metallic);
    float primarySpecRatio = 1.0 / (1.0 + material.clearcoat);
    float transWeight = (1.0 - material.metallic) * material.transmission;

    vec3 brdf = vec3(0.0);
    vec3 bsdf = vec3(0.0);
    float brdfPdf = 0.0;
    float bsdfPdf = 0.0;

    if (transWeight > 0.0)
    {
        // Reflection
        if (refl)
        {
            bsdf = EvalDielectricReflection(material, V, N, L, H, bsdfPdf);
        }
        else // Transmission
        {
            bsdf = EvalDielectricRefraction(material, V, N, L, H, bsdfPdf);
        }
    }

    float m_pdf;

    if (transWeight < 1.0)
    {
        vec3 Cdlin = material.albedo.xyz;
        float Cdlum = 0.3 * Cdlin.x + 0.6 * Cdlin.y + 0.1 * Cdlin.z; // luminance approx.

        vec3 Ctint = Cdlum > 0.0 ? Cdlin / Cdlum : vec3(1.0f); // normalize lum. to isolate hue+sat
        vec3 Cspec0 = mix(material.albedo.w * 0.08 * mix(vec3(1.0), Ctint, material.specularTint), Cdlin, material.metallic);
        vec3 Csheen = mix(vec3(1.0), Ctint, material.sheenTint);

        // Diffuse
        brdf += EvalDiffuse(material, Csheen, V, N, L, H, m_pdf);
        brdfPdf += m_pdf * diffuseRatio;

        // Specular
        brdf += EvalSpecular(material, Cspec0, V, N, L, H, m_pdf);
        brdfPdf += m_pdf * primarySpecRatio * (1.0 - diffuseRatio);

        // Clearcoat
        brdf += EvalClearcoat(material, V, N, L, H, m_pdf);
        brdfPdf += m_pdf * (1.0 - primarySpecRatio) * (1.0 - diffuseRatio);
    }

    pdf = mix(brdfPdf, bsdfPdf, transWeight);

    return mix(brdf, bsdf, transWeight);
}
