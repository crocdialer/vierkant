module psrdnoise;

//
// Slang port of psrdnoise3.glsl, default path (no PERLINGRID, no FASTROTATION).
//
// Authors: Stefan Gustavson (stefan.gustavson@gmail.com)
// and Ian McEwan (ijm567@gmail.com)
// Version 2021-12-02, published under the MIT license (see below)
// https://github.com/stegu/psrdnoise
//
// Copyright (c) 2021 Stefan Gustavson and Ian McEwan.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

namespace utils
{

//! floor-based mod, like GLSL mod. fmod truncates and differs for negative input
float4 _psrd_mod(float4 x, float y) { return x - y * floor(x / y); }

//! permutation polynomial for the hash value
float4 _psrd_permute(float4 i)
{
    float4 im = _psrd_mod(i, 289.0);
    return _psrd_mod((im * 34.0 + 10.0) * im, 289.0);
}

//! texture space -> simplex space, M = ((0,1,1), (1,0,1), (1,1,0))
float3 _psrd_to_simplex(float3 x) { return (x.x + x.y + x.z) - x; }

//! simplex space -> texture space, inverse of M
float3 _psrd_to_texture(float3 i) { return 0.5 * (i.x + i.y + i.z) - i; }

/**
 * @brief   3D tiling simplex noise with rotating gradients and analytic first derivatives.
 *
 * @param   x           point to evaluate
 * @param   period      integer periods per axis, up to 289. a value <= 0 disables wrapping on that axis
 * @param   alpha       rotation of the gradients in radians, for swirling animation
 * @param   gradient    returns the partial derivatives (dn/dx, dn/dy, dn/dz)
 * @return  noise value, approx. in [-1, 1]
 */
public float psrdnoise(float3 x, float3 period, float alpha, out float3 gradient)
{
    // determine which simplex we're in, i0 is the base corner
    float3 uvw = _psrd_to_simplex(x);
    float3 i0 = floor(uvw);
    float3 f0 = frac(uvw);

    // rank order u,v,w (ties in priority u,v,w), o1/o2 are offsets to the 2nd and 3rd corners
    float3 g_ = step(f0.xyx, f0.yzz);
    float3 l_ = 1.0 - g_;
    float3 g = float3(l_.z, g_.xy);
    float3 l = float3(l_.xy, g_.z);
    float3 o1 = min(g, l);
    float3 o2 = max(g, l);

    float3 i1 = i0 + o1;
    float3 i2 = i0 + o2;
    float3 i3 = i0 + 1.0;

    float3 v0 = _psrd_to_texture(i0);
    float3 v1 = _psrd_to_texture(i1);
    float3 v2 = _psrd_to_texture(i2);
    float3 v3 = _psrd_to_texture(i3);

    // vectors to each of the simplex corners
    float3 x0 = x - v0;
    float3 x1 = x - v1;
    float3 x2 = x - v2;
    float3 x3 = x - v3;

    if(any(period > 0.0))
    {
        // wrap corners to periods and transform back to simplex space
        float4 vx = float4(v0.x, v1.x, v2.x, v3.x);
        float4 vy = float4(v0.y, v1.y, v2.y, v3.y);
        float4 vz = float4(v0.z, v1.z, v2.z, v3.z);
        if(period.x > 0.0) { vx = _psrd_mod(vx, period.x); }
        if(period.y > 0.0) { vy = _psrd_mod(vy, period.y); }
        if(period.z > 0.0) { vz = _psrd_mod(vz, period.z); }

        // round to fix float errors
        i0 = floor(_psrd_to_simplex(float3(vx.x, vy.x, vz.x)) + 0.5);
        i1 = floor(_psrd_to_simplex(float3(vx.y, vy.y, vz.y)) + 0.5);
        i2 = floor(_psrd_to_simplex(float3(vx.z, vy.z, vz.z)) + 0.5);
        i3 = floor(_psrd_to_simplex(float3(vx.w, vy.w, vz.w)) + 0.5);
    }

    // one pseudo-random hash value per corner
    float4 hash = _psrd_permute(_psrd_permute(_psrd_permute(float4(i0.z, i1.z, i2.z, i3.z)) +
                                              float4(i0.y, i1.y, i2.y, i3.y)) +
                                float4(i0.x, i1.x, i2.x, i3.x));

    // generating gradients from a Fibonacci spiral on the unit sphere
    float4 theta = hash * 3.883222077;               // 2*pi/golden ratio
    float4 sz = hash * -0.006920415 + 0.996539792;   // 1-(hash+0.5)*2/289
    float4 psi = hash * 0.108705628;                 // 10*pi/289, chosen to avoid correlation

    float4 Ct = cos(theta);
    float4 St = sin(theta);
    float4 sz_prime = sqrt(1.0 - sz * sz);

    float4 gx, gy, gz;

    // rotate gradients by alpha around a pseudo-random orthogonal axis.
    // the branch is cheap when alpha is uniform
    if(alpha != 0.0)
    {
        float4 Sp = sin(psi);
        float4 Cp = cos(psi);

        float4 px = Ct * sz_prime;
        float4 py = St * sz_prime;
        float4 pz = sz;

        float4 Ctp = St * Sp - Ct * Cp;
        float4 qx = lerp(Ctp * St, Sp, sz);
        float4 qy = lerp(-Ctp * Ct, Cp, sz);
        float4 qz = -(py * Cp + px * Sp);

        float Sa = sin(alpha);
        float Ca = cos(alpha);

        gx = Ca * px + Sa * qx;
        gy = Ca * py + Sa * qy;
        gz = Ca * pz + Sa * qz;
    }
    else
    {
        gx = Ct * sz_prime;
        gy = St * sz_prime;
        gz = sz;
    }

    float3 g0 = float3(gx.x, gy.x, gz.x);
    float3 g1 = float3(gx.y, gy.y, gz.y);
    float3 g2 = float3(gx.z, gy.z, gz.z);
    float3 g3 = float3(gx.w, gy.w, gz.w);

    // radial decay with distance from each simplex corner
    float4 w = max(0.5 - float4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    float4 w2 = w * w;
    float4 w3 = w2 * w;

    // value of the linear ramp from each corner
    float4 gdotx = float4(dot(g0, x0), dot(g1, x1), dot(g2, x2), dot(g3, x3));

    float n = dot(w3, gdotx);

    // first order partial derivatives
    float4 dw = -6.0 * w2 * gdotx;
    float3 dn0 = w3.x * g0 + dw.x * x0;
    float3 dn1 = w3.y * g1 + dw.y * x1;
    float3 dn2 = w3.z * g2 + dw.z * x2;
    float3 dn3 = w3.w * g3 + dw.w * x3;
    gradient = 39.5 * (dn0 + dn1 + dn2 + dn3);

    // scale the value into [-1, 1]
    return 39.5 * n;
}

}// namespace utils
