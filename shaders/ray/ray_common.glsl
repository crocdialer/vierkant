#include "../renderer/brdf.glsl"

#define FLOAT_MAX 3.402823466e+38
#define FLOAT_MIN 1.175494351e-38

#define EPS 0.001

struct Ray
{
    vec3 origin;
    vec3 direction;
};

//! simple struct to groupt rayhit-parameters
struct payload_t
{
    // the ray that generated this payload
    Ray ray;

    // terminate path
    bool stop;

    // worldspace position
    vec3 position;

    // faceforward, worldspace normal
    vec3 normal;

    // accumulated radiance along a path
    vec3 radiance;

    // material absorbtion
    vec3 beta;

    // probality density
    float pdf;
};

struct material_t
{
    vec4 color;
    vec4 emission;
    float metalness;
    float roughness;
    float transmission;
    float attenuation_distance;
    vec4 attenuation_color;
    float ior;
    uint texture_index;
    uint normalmap_index;
    uint emission_index;
    uint ao_rough_metal_index;
};

struct push_constants_t
{
    //! current time since start in seconds
    float time;

    //! sample-batch index
    uint batch_index;

    //! override albedo colors
    bool disable_material;

    //! a provided random seed
    uint random_seed;
};

uint hash(uint x)
{
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

void hash_combine(inout uint seed, uint v)
{
    seed ^= hash(v) + 0x9e3779b9 + (seed << 6U) + (seed >> 2U);
}

//! helper to generate a seed
uint rng_seed(uint seed)
{
    hash_combine(seed, gl_LaunchSizeEXT.x * gl_LaunchIDEXT.y + gl_LaunchIDEXT.x);
    return seed;
}

//! random number generation using pcg32i_random_t, using inc = 1. Our random state is a uint.
uint rng_step(uint rng_state)
{
    return rng_state * 747796405 + 1;
}

//! steps the RNG and returns a floating-point value between 0 and 1 inclusive.
float rng_float(inout uint rng_state)
{
    // condensed version of pcg_output_rxs_m_xs_32_32, with simple conversion to floating-point [0,1].
    rng_state  = rng_step(rng_state);
    uint word = ((rng_state >> ((rng_state >> 28) + 4)) ^ rng_state) * 277803737;
    word      = (word >> 22) ^ word;
    return float(word) / 4294967295.0f;
}

/*
 * Power heuristic often reduces variance even further for multiple importance sampling
 * Chapter 13.10.1 of pbrbook
 */
float powerHeuristic(float a, float b)
{
    float t = a * a;
    return t / (b * b + t);
}

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