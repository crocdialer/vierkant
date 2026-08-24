//
// Created by crocdialer on 2/5/22.
//

#pragma once

#include <cereal/cereal.hpp>
#include <vierkant/math.hpp>

namespace glm
{

template<class Archive, class T>
void serialize(Archive &archive, glm::vec<2, T, glm::defaultp> &v)
{
    archive(cereal::make_nvp("x", v.x),
            cereal::make_nvp("y", v.y));
}

template<class Archive, class T>
void serialize(Archive &archive, glm::vec<3, T, glm::defaultp> &v)
{
    archive(cereal::make_nvp("x", v.x),
            cereal::make_nvp("y", v.y),
            cereal::make_nvp("z", v.z));
}

template<class Archive, class T>
void serialize(Archive &archive, glm::vec<4, T, glm::defaultp> &v)
{
    archive(cereal::make_nvp("x", v.x),
            cereal::make_nvp("y", v.y),
            cereal::make_nvp("z", v.z),
            cereal::make_nvp("w", v.w));
}

// glm matrices serialization
template<class Archive, class T>
void serialize(Archive &archive, glm::mat<2, 2, T, glm::defaultp> &m){ archive(m[0], m[1]); }

template<class Archive, class T>
void serialize(Archive &archive, glm::mat<3, 3, T, glm::defaultp> &m){ archive(m[0], m[1], m[2]); }

template<class Archive, class T>
void serialize(Archive &archive, glm::mat<4, 4, T, glm::defaultp> &m){ archive(m[0], m[1], m[2], m[3]); }

template<class Archive, class T>
void serialize(Archive &archive, glm::qua<T, glm::defaultp> &q)
{
    archive(cereal::make_nvp("x", q.x),
            cereal::make_nvp("y", q.y),
            cereal::make_nvp("z", q.z),
            cereal::make_nvp("w", q.w));
}

}// namespace glm
