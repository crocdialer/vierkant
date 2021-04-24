#define FLOAT_MAX 3.402823466e+38
#define FLOAT_MIN 1.175494351e-38

#define PI 3.1415926535897932384626433832795
#define ONE_OVER_PI 0.31830988618379067153776752674503

#define EPS 0.001

struct Ray
{
    vec3 origin;
    vec3 direction;
};

struct Cone
{
    float radius;
    float angle;
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

//! random point on a unit-disc
vec2 sample_unit_disc(vec2 Xi)
{
    // [0, 2pi]
    const float theta = 2.0 * PI * Xi.y;

    return vec2(Xi.x * cos(theta), Xi.x * sin(theta));
}

//! random point on a unit-sphere
vec3 sample_unit_sphere(vec2 Xi)
{
    // [0, 2pi]
    const float theta = 2.0 * PI * Xi.y;

    // [-1, 1]
    float u = 2.0 * Xi.x - 1.0;

    const float r = sqrt(1.0 - u * u);
    return vec3(r * cos(theta), r * sin(theta), u);
}

//! return a Hammersley point in range [0, 1]
vec2 Hammersley(uint i, uint N)
{
    float vdc = float(bitfieldReverse(i)) * 2.3283064365386963e-10; // Van der Corput
    return vec2(float(i) / float(N), vdc);
}

/*
 * Calculates local coordinate frame for a given normal
 */
mat3 local_frame(in vec3 normal)
{
    vec3 up = abs(normal.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 tangentX = normalize(cross(normal, up));
    vec3 tangentY = cross(normal, tangentX);
    return mat3(tangentX, tangentY, normal);
}

vec3 sample_cosine(vec2 Xi)
{
    float cosTheta = sqrt(max(1.0 - Xi.y, 0.0));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    float phi = 2.0 * PI * Xi.x;

    // L
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// Sample a half-vector in world space
vec3 sample_GGX(vec2 Xi, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt(clamp((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y), 0.0, 1.0));
    float sinTheta = sqrt(clamp(1.0 - cosTheta * cosTheta, 0.0, 1.0));

    // H
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

/*
 * Schlick's approximation of the specular reflection coefficient R
 * (1 - cosTheta)^5
 */
float SchlickFresnel(float u)
{
    float m = clamp(1.0 - u, 0.0, 1.0);
    return m * m * m * m * m; // power of 5
}

/*
 * Generalized-Trowbridge-Reitz (D)
 * Describes differential area of microfacets for the surface normal
 */
float GTR2(float NDotH, float a)
{
    float a2 = a * a;
    float t = 1.0 + (a2 - 1.0)*NDotH*NDotH;
    return a2 / (PI * t*t);
}

/*
 * The masking shadowing function Smith for GGX noraml distribution (G)
 */
float SmithGGX(float NDotv, float alphaG)
{
    float a = alphaG * alphaG;
    float b = NDotv * NDotv;
    return 1.0 / (NDotv + sqrt(a + b - a * b));
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