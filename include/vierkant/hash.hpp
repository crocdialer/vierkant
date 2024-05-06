//
// Created by crocdialer on 05.02.23.
//

#pragma once

#include <functional>
#include <string_view>

namespace vierkant
{

template<class T>
inline void hash_combine(std::size_t &seed, const T &v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6U) + (seed >> 2U);
}

inline size_t hash(const void* p, size_t num_bytes)
{
    auto data = reinterpret_cast<const char*>(p);
    std::hash<std::string_view> hasher;
    return hasher(std::string_view(data, data + num_bytes));
}

template<class T, class U>
struct pair_hash
{
    inline size_t operator()(const std::pair<T, U> &p) const
    {
        size_t h = 0;
        vierkant::hash_combine(h, p.first);
        vierkant::hash_combine(h, p.second);
        return h;
    }
};

}// namespace vierkant