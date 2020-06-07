#include "types.glsl"

float map_roughness(float r)
{
    return mix(0.025, 0.975, r);
}

vec3 BRDF_Lambertian(vec3 color, float metalness)
{
	return mix(color, vec3(0.0), metalness) * ONE_OVER_PI;
}

vec3 F_schlick(vec3 f0, float u)
{
    return f0 + (vec3(1.0) - f0) * pow(1.0 - u, 5.0);
}

float Vis_schlick(float ndotl, float ndotv, float roughness)
{
	// = G_Schlick / (4 * ndotv * ndotl)
	float a = roughness + 1.0;
	float k = a * a * 0.125;

	float Vis_SchlickV = ndotv * (1 - k) + k;
	float Vis_SchlickL = ndotl * (1 - k) + k;

	return 0.25 / (Vis_SchlickV * Vis_SchlickL);
}

float D_GGX(float NoH, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float denom = NoH * NoH * (a2 - 1.0) + 1.0;
	denom = 1.0 / (denom * denom);
	return a2 * denom * ONE_OVER_PI;
}

vec4 shade(in lightsource_t light, in vec3 normal, in vec3 eyeVec, in vec4 base_color,
           float roughness, float metalness, float shade_factor)
{
    roughness = map_roughness(roughness);
    vec3 lightDir = light.type > 0 ? (light.position - eyeVec) : -light.direction;
    vec3 L = normalize(lightDir);
    vec3 E = normalize(-eyeVec);
    vec3 H = normalize(L + E);

    // vec3 ambient = light.ambient.rgb;
    float nDotL = max(0.f, dot(normal, L));
    float nDotH = max(0.f, dot(normal, H));
    float nDotV = max(0.f, dot(normal, E));
    float lDotH = max(0.f, dot(L, H));
    float att = shade_factor;

    if(light.type > 0)
    {
        // distance^2
        float dist2 = dot(lightDir, lightDir);
        float v = dist2 / (light.radius * light.radius);
        v = clamp(1.f - v * v, 0.f, 1.f);
        att *= v * v / (1.f + dist2 * light.quadraticAttenuation);

        if(light.type > 1)
        {
            float spot_effect = dot(normalize(light.direction), -L);
            att *= spot_effect < light.spotCosCutoff ? 0 : 1;
            spot_effect = pow(spot_effect, light.spotExponent);
            att *= spot_effect;
        }
    }

    // brdf term
    const vec3 dielectricF0 = vec3(0.04);
    vec3 f0 = mix(dielectricF0, base_color.rgb, metalness);
    vec3 F = F_schlick(f0, lDotH);
    float D = D_GGX(nDotH, roughness);
    float Vis = Vis_schlick(nDotL, nDotV, roughness);
    vec3 Ir = light.diffuse.rgb * light.intensity;
    vec3 diffuse = BRDF_Lambertian(base_color.rgb, metalness);
    vec3 specular = F * D * Vis;
    return vec4((diffuse + specular) * nDotL * Ir * att, 1.0);
}
