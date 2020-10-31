/*
 * major portions of the code code adapted from: https://github.com/dmnsgn/glsl-tone-map

 * Copyright (C) 2019 the internet and Damien Seguin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
*/

vec3 tonemap_reinhard(vec3 x)
{
   return x / (1.0 + x);
}

float tonemap_reinhard(float x)
{
   return x / (1.0 + x);
}

vec3 tonemap_reinhard2(vec3 x)
{
   const float L_white = 4.0;
   return (x * (1.0 + x / (L_white * L_white))) / (1.0 + x);
}

float tonemap_reinhard2(float x)
{
   const float L_white = 4.0;
   return (x * (1.0 + x / (L_white * L_white))) / (1.0 + x);
}

//! exposure tonemapping
vec3 tonemap_exposure(vec3 hdr_color, float exposure)
{
   return vec3(1.0) - exp(-hdr_color * exposure);
}

// Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
vec3 tonemap_aces(vec3 x)
{
   const float a = 2.51;
   const float b = 0.03;
   const float c = 2.43;
   const float d = 0.59;
   const float e = 0.14;
   return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float tonemap_aces(float x)
{
   const float a = 2.51;
   const float b = 0.03;
   const float c = 2.43;
   const float d = 0.59;
   const float e = 0.14;
   return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

//! Filmic Tonemapping Operators http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 tonemap_filmic(vec3 x)
{
   vec3 X = max(vec3(0.0), x - 0.004);
   vec3 result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
   return pow(result, vec3(2.2));
}

float tonemap_filmic(float x)
{
   float X = max(0.0, x - 0.004);
   float result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
   return pow(result, 2.2);
}

// Lottes 2016, "Advanced Techniques and Optimization of HDR Color Pipelines"
vec3 tonemap_lottes(vec3 x)
{
   const vec3 a = vec3(1.6);
   const vec3 d = vec3(0.977);
   const vec3 hdrMax = vec3(8.0);
   const vec3 midIn = vec3(0.18);
   const vec3 midOut = vec3(0.267);

   const vec3 b =
   (-pow(midIn, a) + pow(hdrMax, a) * midOut) /
   ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
   const vec3 c =
   (pow(hdrMax, a * d) * pow(midIn, a) - pow(hdrMax, a) * pow(midIn, a * d) * midOut) /
   ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

   return pow(x, a) / (pow(x, a * d) * b + c);
}

float tonemap_lottes(float x)
{
   const float a = 1.6;
   const float d = 0.977;
   const float hdrMax = 8.0;
   const float midIn = 0.18;
   const float midOut = 0.267;

   const float b = (-pow(midIn, a) + pow(hdrMax, a) * midOut) / ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
   const float c = (pow(hdrMax, a * d) * pow(midIn, a) - pow(hdrMax, a) * pow(midIn, a * d) * midOut) /
      ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

   return pow(x, a) / (pow(x, a * d) * b + c);
}

// Uchimura 2017, "HDR theory and practice"
// Math: https://www.desmos.com/calculator/gslcdxvipg
// Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
vec3 tonemap_uchimura(vec3 x, float P, float a, float m, float l, float c, float b)
{
   float l0 = ((P - m) * l) / a;
   float L0 = m - m / a;
   float L1 = m + (1.0 - m) / a;
   float S0 = m + l0;
   float S1 = m + a * l0;
   float C2 = (a * P) / (P - S1);
   float CP = -C2 / P;

   vec3 w0 = vec3(1.0 - smoothstep(0.0, m, x));
   vec3 w2 = vec3(step(m + l0, x));
   vec3 w1 = vec3(1.0 - w0 - w2);

   vec3 T = vec3(m * pow(x / m, vec3(c)) + b);
   vec3 S = vec3(P - (P - S1) * exp(CP * (x - S0)));
   vec3 L = vec3(m + a * (x - m));

   return T * w0 + L * w1 + S * w2;
}

vec3 tonemap_uchimura(vec3 x)
{
   const float P = 1.0;  // max display brightness
   const float a = 1.0;  // contrast
   const float m = 0.22; // linear section start
   const float l = 0.4;  // linear section length
   const float c = 1.33; // black
   const float b = 0.0;  // pedestal

   return tonemap_uchimura(x, P, a, m, l, c, b);
}

float tonemap_uchimura(float x, float P, float a, float m, float l, float c, float b)
{
   float l0 = ((P - m) * l) / a;
   float L0 = m - m / a;
   float L1 = m + (1.0 - m) / a;
   float S0 = m + l0;
   float S1 = m + a * l0;
   float C2 = (a * P) / (P - S1);
   float CP = -C2 / P;

   float w0 = 1.0 - smoothstep(0.0, m, x);
   float w2 = step(m + l0, x);
   float w1 = 1.0 - w0 - w2;

   float T = m * pow(x / m, c) + b;
   float S = P - (P - S1) * exp(CP * (x - S0));
   float L = m + a * (x - m);

   return T * w0 + L * w1 + S * w2;
}

float tonemap_uchimura(float x)
{
   const float P = 1.0;  // max display brightness
   const float a = 1.0;  // contrast
   const float m = 0.22; // linear section start
   const float l = 0.4;  // linear section length
   const float c = 1.33; // black
   const float b = 0.0;  // pedestal

   return tonemap_uchimura(x, P, a, m, l, c, b);
}

vec3 helper_uncharted2(vec3 x)
{
   float A = 0.15;
   float B = 0.50;
   float C = 0.10;
   float D = 0.20;
   float E = 0.02;
   float F = 0.30;
   float W = 11.2;
   return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 tonemap_uncharted2(vec3 color)
{
   const float W = 11.2;
   float exposureBias = 2.0;
   vec3 curr = helper_uncharted2(exposureBias * color);
   vec3 whiteScale = 1.0 / helper_uncharted2(vec3(W));
   return curr * whiteScale;
}

float helper_uncharted2(float x)
{
   float A = 0.15;
   float B = 0.50;
   float C = 0.10;
   float D = 0.20;
   float E = 0.02;
   float F = 0.30;
   float W = 11.2;
   return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float tonemap_uncharted2(float color)
{
   const float W = 11.2;
   const float exposureBias = 2.0;
   float curr = helper_uncharted2(exposureBias * color);
   float whiteScale = 1.0 / helper_uncharted2(W);
   return curr * whiteScale;
}

// Unreal 3, Documentation: "Color Grading"
// Adapted to be close to Tonemap_ACES, with similar range
// Gamma 2.2 correction is baked in, don't use with sRGB conversion!
vec3 tonemap_unreal(vec3 x)
{
   return x / (x + 0.155) * 1.019;
}

float tonemap_unreal(float x)
{
   return x / (x + 0.155) * 1.019;
}
