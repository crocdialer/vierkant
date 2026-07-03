#!/usr/bin/env python3
"""Derive the baked constants for continuous-wavelength dispersion.

Re-run this if the band limits, the CMF fit, or the dispersion->spread relation
change. The two outputs are pasted into shaders/slang/utils/bsdf.slang:
  - LAMBDA_RGB_NORM : per-channel white-balance k (uniform-lambda avg -> white)
  - the Cauchy B factor used by cauchy_ior()

CMF fit: Wyman, Sloan & Shirley, "Simple Analytic Approximations to the CIE XYZ
Color Matching Functions", JCGT 2013 (multi-lobe Gaussian fit).
"""
import math

BAND_LO, BAND_HI = 400.0, 700.0  # nm

def g(x, mu, s1, s2):
    s = s1 if x < mu else s2
    t = (x - mu) / s
    return math.exp(-0.5 * t * t)

def cie_xyz(l):
    X = 1.056 * g(l, 599.8, 37.9, 31.0) + 0.362 * g(l, 442.0, 16.0, 26.7) - 0.065 * g(l, 501.1, 20.4, 26.2)
    Y = 0.821 * g(l, 568.8, 46.9, 40.5) + 0.286 * g(l, 530.9, 16.3, 31.1)
    Z = 1.217 * g(l, 437.0, 11.8, 36.0) + 0.681 * g(l, 459.0, 26.0, 13.8)
    return (X, Y, Z)

def xyz_to_rec709(X, Y, Z):
    # same matrix as XYZ_TO_REC709 in utils/bsdf.slang
    R =  3.2404542 * X - 1.5371385 * Y - 0.4985314 * Z
    G = -0.9692660 * X + 1.8760108 * Y + 0.0415560 * Z
    B =  0.0556434 * X - 0.2040259 * Y + 1.0572252 * Z
    return (R, G, B)

def lambda_to_rgb(l):
    # clamp out-of-gamut negatives: saturated spectral colors lie outside sRGB, and any negative
    # throughput gets clipped downstream (accumulation/denoiser), which would bias the average.
    return tuple(max(c, 0.0) for c in xyz_to_rec709(*cie_xyz(l)))

if __name__ == "__main__":
    # --- per-channel white balance: (1/band) * integral(k * rgb(l)) == (1,1,1) ---
    N = 30000
    acc = [0.0, 0.0, 0.0]
    for i in range(N):
        l = BAND_LO + (i + 0.5) * (BAND_HI - BAND_LO) / N
        rgb = lambda_to_rgb(l)
        for c in range(3):
            acc[c] += rgb[c]
    mean = [a / N for a in acc]
    k = [1.0 / m for m in mean]
    print(f"band [{BAND_LO:.0f}, {BAND_HI:.0f}] nm")
    print(f"mean rgb over band : {mean}")
    print(f"LAMBDA_RGB_NORM (k): float3({k[0]:.6f}, {k[1]:.6f}, {k[2]:.6f})")

    # --- Cauchy n(l) = A + B/l^2, B fit to glTF dispersion at the F/C lines ---
    #   n(lF) - n(lC) = 2 * half_spread  =>  B = 2 * half_spread / (1/lF^2 - 1/lC^2)
    lF, ld, lC = 486.1, 587.6, 656.3  # F (blue), d (yellow ref), C (red) lines
    factor = 2.0 / (1.0 / lF**2 - 1.0 / lC**2)
    print(f"\nCauchy: B = {factor:.1f} * half_spread   (half_spread = (ior-1)*0.025*dispersion)")
    print(f"        A = ior - B / {ld}^2")
