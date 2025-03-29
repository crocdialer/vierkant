//
// Created by crocdialer on 3/2/22.
//

#include <vierkant/math.hpp>

namespace vierkant
{

/**
 * @brief   'ortho_reverse_RH_ZO' returns an orthographic projection-matrix with following properties:
 *          - right-handed coordinate-system (RH)
 *          - depth-range is inverted and falls in range [1..0] (reverse | ZO)
 *
 * @param   left    left clipping distance
 * @param   right   right clipping distance
 * @param   bottom  bottom clipping distance
 * @param   top     top clipping distance
 * @param   z_near  z_near clipping distance
 * @param   z_far   z_far clipping distance
 * @return  an orthographic projection-matrix
 */
template<typename T>
inline glm::mat<4, 4, T, glm::defaultp> ortho_reverse_RH_ZO(T left, T right, T bottom, T top, T z_near, T z_far)
{
    glm::mat<4, 4, T, glm::defaultp> ret(1);
    ret[0][0] = static_cast<T>(2) / (right - left);
    ret[1][1] = -static_cast<T>(2) / (top - bottom);
    ret[2][2] = static_cast<T>(1) / (z_far - z_near);
    ret[3][0] = -(right + left) / (right - left);
    ret[3][1] = -(top + bottom) / (top - bottom);
    ret[3][2] = static_cast<T>(1) + z_near / (z_far - z_near);
    return ret;
}

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

}// namespace vierkant
