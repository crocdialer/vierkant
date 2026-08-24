//
// Created by crocdialer on 10/16/23.
//

#pragma once

#include <cereal/cereal.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/memory.hpp>
#include <vierkant/animation.hpp>
#include <vierkant/nodes.hpp>

namespace vierkant
{

template<class Archive, class T>
void serialize(Archive &archive, vierkant::animation_value_t<T> &v)
{
    archive(cereal::make_nvp("value", v.value),
            cereal::make_nvp("in_tangent", v.in_tangent),
            cereal::make_nvp("out_tangent", v.out_tangent));
}

template<class Archive, class T>
void serialize(Archive &archive, vierkant::animation_keys_t_<T> &keys)
{
    archive(cereal::make_nvp("positions", keys.positions),
            cereal::make_nvp("rotations", keys.rotations),
            cereal::make_nvp("scales", keys.scales),
            cereal::make_nvp("morph_weights", keys.morph_weights));
}

template<class Archive, class T>
void serialize(Archive &archive, vierkant::animation_t<T> &animation)
{
    archive(cereal::make_nvp("name", animation.name),
            cereal::make_nvp("duration", animation.duration),
            cereal::make_nvp("ticks_per_sec", animation.ticks_per_sec),
            cereal::make_nvp("keys", animation.keys),
            cereal::make_nvp("interpolation_mode", animation.interpolation_mode));
}

template<class Archive, class T>
void serialize(Archive &archive, vierkant::animation_component_t_<T> &a)
{
    archive(cereal::make_nvp("index", a.index),
            cereal::make_nvp("playing", a.playing),
            cereal::make_nvp("animation_speed", a.animation_speed),
            cereal::make_nvp("current_time", a.current_time));
}

}// namespace vierkant

namespace vierkant::nodes
{
template<class Archive>
void serialize(Archive &archive, vierkant::nodes::node_t &n)
{
    archive(cereal::make_nvp("name", n.name),
            cereal::make_nvp("transform", n.transform),
            cereal::make_nvp("offset", n.offset),
            cereal::make_nvp("index", n.index),
            cereal::make_nvp("parent", n.parent),
            cereal::make_nvp("children", n.children));
}

}// namespace vierkant::nodes
