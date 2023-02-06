//
// Created by crocdialer on 04.02.23.
//

#pragma once

#include <vierkant/hash.hpp>
#include <vierkant/math.hpp>

namespace vierkant
{

//! transform_t_ is a struct grouping data for rigid transforms with non-uniform scaling
template<typename T>
struct transform_t_
{
    glm::vec<3, T> translation = glm::vec<3, T>(0);
    glm::qua<T> rotation = glm::qua<T>(1, 0, 0, 0);
    glm::vec<3, T> scale = glm::vec<3, T>(1);
};
using transform_t = transform_t_<float>;

/**
 * @brief   operator to apply a vierkant::transform_t to a 3d-vector.
 *
 * @param   t   a provided vierkant::transform_t
 * @param   v   a provided vector
 * @return  a transformed vector.
 */
template<typename T1, typename T2>
inline glm::vec<3, T2> operator*(const transform_t_<T1> &transform, const glm::vec<3, T2> &v)
{
    using vec3_t = typename std::decay<decltype(transform.translation)>::type;
    vec3_t ret = v;
    ret *= transform.scale;
    ret = glm::rotate(transform.rotation, ret);
    ret += transform.translation;
    return ret;
}

/**
 * @brief   operator to combine/chain two vierkant::transform_t, analog to multiplying two mat4.
 *
 * @param   lhs     1st provided vierkant::transform_t
 * @param   rhs     2nd provided vierkant::transform_t
 * @return  a combined vierkant::transform.
 */
template<typename T>
inline transform_t_<T> operator*(const transform_t_<T> &lhs, const transform_t_<T> &rhs)
{
    transform_t_<T> ret = lhs;
    ret.translation += glm::rotate(lhs.rotation, rhs.translation * lhs.scale);
    ret.rotation *= rhs.rotation;
    ret.scale *= rhs.scale;
    return ret;
}

template<typename T>
inline bool operator==(const transform_t_<T> &lhs, const transform_t_<T> &rhs)
{
    if(lhs.translation != rhs.translation) { return false; }
    if(lhs.rotation != rhs.rotation) { return false; }
    if(lhs.scale != rhs.scale) { return false; }
    return true;
}

template<typename T>
inline bool operator!=(const transform_t_<T> &lhs, const transform_t_<T> &rhs)
{
    return !(lhs == rhs);
}

template<typename T>
inline bool epsilon_equal(const transform_t_<T> &lhs, const transform_t_<T> &rhs, T epsilon)
{
    return glm::all(glm::epsilonEqual(lhs.translation, rhs.translation, epsilon)) &&
           glm::all(glm::epsilonEqual(lhs.rotation, rhs.rotation, epsilon)) &&
           glm::all(glm::epsilonEqual(lhs.scale, rhs.scale, epsilon));
}

/**
 * @brief   inverse can be used to inverse a vierkant::transform_t so that a * inverse(a) == identity.
 *
 * @param   t   a provided vierkant::transform_t
 * @return  the inverted vierkant::transform.
 */
template<typename T>
inline transform_t_<T> inverse(const transform_t_<T> &t)
{
    transform_t_<T> ret;
    ret.scale = T(1) / t.scale;
    ret.rotation = glm::inverse(t.rotation);
    ret.translation = -(ret.rotation * (t.translation * ret.scale));
    return ret;
}

template<typename T1 = float, typename T2>
inline glm::mat<4, 4, T1> mat4_cast(const transform_t_<T2> &t)
{
    glm::mat<4, 4, T1> tmp = glm::mat4_cast(t.rotation);
    tmp[0] *= t.scale.x;
    tmp[1] *= t.scale.y;
    tmp[2] *= t.scale.z;
    tmp[3] = glm::vec<4, T1>(t.translation, 1);
    return tmp;
}

template<typename T1 = float, typename T2>
inline transform_t_<T1> transform_cast(const glm::mat<4, 4, T2> &m)
{
    transform_t_<T1> ret;
    glm::vec<3, T1> skew;
    glm::vec<4, T1> perspective;
    glm::decompose(glm::mat<4, 4, T1>(m), ret.scale, ret.rotation, ret.translation, skew, perspective);
    return ret;
}

template<typename T, typename U>
inline transform_t_<T> mix(const transform_t_<T> &lhs, const transform_t_<T> &rhs, U v)
{
    transform_t_<T> ret;
    ret.translation = glm::mix(lhs.translation, rhs.translation, v);
    ret.rotation = glm::slerp(lhs.rotation, rhs.rotation, v);
    ret.scale = glm::mix(lhs.scale, rhs.scale, v);
    return ret;
}

}// namespace vierkant

namespace std
{

template<typename T>
struct hash<vierkant::transform_t_<T>>
{
    size_t operator()(const vierkant::transform_t_<T> &t) const
    {
        size_t h = 0;
        vierkant::hash_combine(h, t.translation);
        vierkant::hash_combine(h, t.rotation);
        vierkant::hash_combine(h, t.scale);
        return h;
    }
};

}// namespace std
