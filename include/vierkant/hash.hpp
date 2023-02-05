//
// Created by crocdialer on 05.02.23.
//

#pragma once

#include <functional>

namespace vierkant
{

template<class T>
inline void hash_combine(std::size_t &seed, const T &v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6U) + (seed >> 2U);
}

}// namespace vierkant