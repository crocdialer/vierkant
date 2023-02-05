//
// Created by crocdialer on 04.02.23.
//

#pragma once

#include <vierkant/math.hpp>
#include <vierkant/hash.hpp>

namespace vierkant
{

//! simple struct grouping data for a rigid transform (+ non-uniform scale, da fuck: no shear, no tear, non-projective)
template<typename T>
struct transform_t_
{
    glm::vec<3, T> translation = glm::vec<3, T>(0);
    glm::qua<T> rotation = glm::qua<T>(1, 0, 0, 0);
    glm::vec<3, T> scale = glm::vec<3, T>(1);
};
using transform_t = transform_t_<double>;

template<typename T1, typename T2>
inline glm::vec<3, T2> operator*(const transform_t_<T1> &transform, const glm::vec<3, T2> &p)
{
    using elem_t = typename std::decay<decltype(transform.translation)>::type;
    elem_t ret = p;
    ret *= transform.scale;
    ret = glm::rotate(transform.rotation, ret);
    ret += transform.translation;
    return ret;
}

template<typename T>
inline transform_t operator*(const transform_t_<T> &lhs, const transform_t_<T> &rhs)
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
inline bool operator!=(const transform_t_<T> &lhs, const transform_t_<T> &rhs) { return !(lhs == rhs); }

template<typename T1 = float, typename T2>
glm::mat<4, 4, T1> mat4_cast(const transform_t_<T2> &t)
{
    glm::mat<4, 4, T1> tmp = glm::mat4_cast(t.rotation);
    tmp[0] *= t.scale.x;
    tmp[1] *= t.scale.y;
    tmp[2] *= t.scale.z;
    tmp[3] = glm::vec<4, T1>(t.translation, 1);
    return tmp;
}

template<typename T1 = double, typename T2>
transform_t_<T1> transform_cast(const glm::mat<4, 4, T2> &m)
{
    transform_t_<T1> ret;
    glm::vec<3, T1> skew;
    glm::vec<4, T1> perspective;
    glm::decompose(glm::mat<4, 4, T1>(m), ret.scale, ret.rotation, ret.translation, skew, perspective);
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

}
