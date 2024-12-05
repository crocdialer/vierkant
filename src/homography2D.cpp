//
// Created by crocdialer on 12/11/21.
//

#include <vierkant/homography2D.hpp>

namespace vierkant
{

// Adapted from code found here: http://forum.openframeworks.cc/t/quad-warping-homography-without-opencv/3121/19
void gaussian_elimination(float *a, int n)
{
    int i = 0;
    int j = 0;
    int m = n - 1;

    while(i < m && j < n)
    {
        int maxi = i;
        for(int k = i + 1; k < m; ++k)
        {
            if(fabsf(a[k * n + j]) > fabsf(a[maxi * n + j])) { maxi = k; }
        }

        if(a[maxi * n + j] != 0)
        {
            if(i != maxi)
            {
                for(int k = 0; k < n; k++)
                {
                    float aux = a[i * n + k];
                    a[i * n + k] = a[maxi * n + k];
                    a[maxi * n + k] = aux;
                }
            }

            float a_ij = a[i * n + j];
            for(int k = 0; k < n; k++) { a[i * n + k] /= a_ij; }

            for(int u = i + 1; u < m; u++)
            {
                float a_uj = a[u * n + j];
                for(int k = 0; k < n; k++) { a[u * n + k] -= a_uj * a[i * n + k]; }
            }
            ++i;
        }
        ++j;
    }

    for(int k = m - 2; k >= 0; --k)
    {
        for(int l = k + 1; l < n - 1; l++) { a[k * n + m] -= a[k * n + l] * a[l * n + m]; }
    }
}

// Adapted from code found here: http://forum.openframeworks.cc/t/quad-warping-homography-without-opencv/3121/19
glm::mat4 compute_homography(const glm::vec2 *src, const glm::vec2 *dst)
{
    float p[8][9] = {
            {-src[0][0], -src[0][1], -1, 0, 0, 0, src[0][0] * dst[0][0], src[0][1] * dst[0][0], -dst[0][0]},// h11
            {0, 0, 0, -src[0][0], -src[0][1], -1, src[0][0] * dst[0][1], src[0][1] * dst[0][1], -dst[0][1]},// h12
            {-src[1][0], -src[1][1], -1, 0, 0, 0, src[1][0] * dst[1][0], src[1][1] * dst[1][0], -dst[1][0]},// h13
            {0, 0, 0, -src[1][0], -src[1][1], -1, src[1][0] * dst[1][1], src[1][1] * dst[1][1], -dst[1][1]},// h21
            {-src[2][0], -src[2][1], -1, 0, 0, 0, src[2][0] * dst[2][0], src[2][1] * dst[2][0], -dst[2][0]},// h22
            {0, 0, 0, -src[2][0], -src[2][1], -1, src[2][0] * dst[2][1], src[2][1] * dst[2][1], -dst[2][1]},// h23
            {-src[3][0], -src[3][1], -1, 0, 0, 0, src[3][0] * dst[3][0], src[3][1] * dst[3][0], -dst[3][0]},// h31
            {0, 0, 0, -src[3][0], -src[3][1], -1, src[3][0] * dst[3][1], src[3][1] * dst[3][1], -dst[3][1]},// h32
    };
    gaussian_elimination(&p[0][0], 9);
    return glm::mat4(p[0][8], p[3][8], 0, p[6][8], p[1][8], p[4][8], 0, p[7][8], 0, 0, 1, 0, p[2][8], p[5][8], 0, 1);
}

}// namespace vierkant