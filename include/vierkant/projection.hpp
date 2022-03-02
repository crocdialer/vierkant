//
// Created by crocdialer on 3/2/22.
//

#include <vierkant/math.hpp>

namespace vierkant
{

/**
 * @brief   'perspective_infinite_reverse_RH_ZO' returns a perspective projection-matrix with following properties:
 *          - right-handed coordinate-system (RH)
 *          - depth-range is inverted and falls in range [1..0] (reverse | ZO)
 *          - far-clipping plane is at infinity
 *
 * @param   fovY    vertical field-of-view in radians.
 * @param   aspect  aspect-ratio (width / height)
 * @param   zNear   near clipping distance
 * @return  a perspective projection-matrix
 */
template<typename T>
inline glm::mat<4, 4, T, glm::defaultp> perspective_infinite_reverse_RH_ZO(T fovY, T aspect, T z_near)
{
    const T f = T(1) / std::tan(fovY / T(2));
    glm::mat<4, 4, T, glm::defaultp> ret(T(0));
    ret[0][0] = f / aspect;
    ret[1][1] = -f;
    ret[2][3] = -T(1);
    ret[3][2] = z_near;
    return ret;
}

/**
 * @brief   'perspective_infinite_reverse_LH_ZO' returns a perspective projection-matrix with following properties:
 *          - left-handed coordinate-system (LH)
 *          - depth-range is inverted and falls in range [1..0] (reverse | ZO)
 *          - far-clipping plane is at infinity
 *
 * @param   fovY    vertical field-of-view in radians.
 * @param   aspect  aspect-ratio (width / height)
 * @param   zNear   near clipping distance
 * @return  a perspective projection-matrix
 */
template<typename T>
inline glm::mat<4, 4, T, glm::defaultp> perspective_infinite_reverse_LH_ZO(T fovY, T aspect, T z_near)
{
    const T f = T(1) / std::tan(fovY / T(2));
    glm::mat<4, 4, T, glm::defaultp> ret(T(0));
    ret[0][0] = f / aspect;
    ret[1][1] = -f;
    ret[2][3] = T(1);
    ret[3][2] = z_near;
    return ret;
}

}
