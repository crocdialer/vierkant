//
// Created by crocdialer on 3/13/20.
//
#include <glm/gtx/spline.hpp>

#include <vierkant/animation.hpp>

namespace vierkant
{

template<typename T>
T hermite(T const &v1, T const &t1, T const &v2, T const &t2, T const &s)
{
    T s2 = s * s;
    T s3 = s2 * s;

    T f1 = T(2) * s3 - T(3) * s2 + T(1);
    T f2 = T(-2) * s3 + T(3) * s2;
    T f3 = s3 - T(2) * s2 + s;
    T f4 = s3 - s2;

    return f1 * v1 + f2 * v2 + f3 * t1 + f4 * t2;
}

bool create_animation_transform(const animation_keys_t &keys, float time, InterpolationMode interpolation_mode,
                                vierkant::transform_t &out_transform)
{
    // translation
    if(!keys.positions.empty())
    {
        // find a key with equal or greater time
        auto it_rhs = keys.positions.lower_bound(time);

        // lhs iterator (might be invalid)
        auto it_lhs = it_rhs;
        it_lhs--;

        if(it_rhs == keys.positions.begin())
        {
            // time is before first key
            out_transform.translation = it_rhs->second.value;
        }
        else if(it_rhs == keys.positions.end())
        {
            // time is past last key
            out_transform.translation = it_lhs->second.value;
        }
        else
        {
            // interpolate two surrounding keys
            float start_time = it_lhs->first;
            float end_time = it_rhs->first;
            float frac = std::max((time - start_time) / (end_time - start_time), 0.0f);
            glm::vec3 pos = {};

            switch(interpolation_mode)
            {
                case InterpolationMode::Step: frac = 0.f; [[fallthrough]];
                case InterpolationMode::Linear: pos = glm::mix(it_lhs->second.value, it_rhs->second.value, frac); break;
                case InterpolationMode::CubicSpline:
                    pos = glm::hermite(it_lhs->second.value, it_lhs->second.out_tangent, it_rhs->second.value,
                                       it_rhs->second.in_tangent, frac);
                    break;
            }

            out_transform.translation = pos;
        }
    }

    // rotation
    if(!keys.rotations.empty())
    {
        // find a key with equal or greater time
        auto it_rhs = keys.rotations.lower_bound(time);

        // lhs iterator (might be invalid)
        auto it_lhs = it_rhs;
        it_lhs--;

        if(it_rhs == keys.rotations.begin())
        {
            // time is before first key
            out_transform.rotation = it_rhs->second.value;
        }
        else if(it_rhs == keys.rotations.end())
        {
            // time is past last key
            out_transform.rotation = it_lhs->second.value;
        }
        else
        {
            // interpolate two surrounding keys
            float start_time = it_lhs->first;
            float end_time = it_rhs->first;
            float frac = std::max((time - start_time) / (end_time - start_time), 0.0f);

            switch(interpolation_mode)
            {
                case InterpolationMode::Step: frac = 0.f; [[fallthrough]];
                case InterpolationMode::Linear:
                    // quaternion spherical linear interpolation
                    out_transform.rotation = glm::slerp(it_lhs->second.value, it_rhs->second.value, frac);
                    break;

                case InterpolationMode::CubicSpline:
                    //! @see https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#appendix-c-interpolation
                    auto tmp_quat = glm::hermite(it_lhs->second.value, it_lhs->second.out_tangent, it_rhs->second.value,
                                                 it_rhs->second.in_tangent, frac);
                    if(tmp_quat != -tmp_quat)
                    {
                        tmp_quat = glm::normalize(tmp_quat);
                        out_transform.rotation = tmp_quat;
                    }
                    break;
            }
        }
    }

    // scale
    if(!keys.scales.empty())
    {
        // find a key with equal or greater time
        auto it_rhs = keys.scales.lower_bound(time);

        // lhs iterator (might be invalid)
        auto it_lhs = it_rhs;
        it_lhs--;

        if(it_rhs == keys.scales.begin())
        {
            // time is before first key
            out_transform.scale = it_rhs->second.value;
        }
        else if(it_rhs == keys.scales.end())
        {
            // time is past last key
            out_transform.scale = it_lhs->second.value;
        }
        else
        {
            // interpolate two surrounding keys
            float start_time = it_lhs->first;
            float end_time = it_rhs->first;
            float frac = std::max((time - start_time) / (end_time - start_time), 0.0f);

            glm::vec3 scale(1);

            switch(interpolation_mode)
            {
                case InterpolationMode::Step: frac = 0.f; [[fallthrough]];
                case InterpolationMode::Linear:
                    scale = glm::mix(it_lhs->second.value, it_rhs->second.value, frac);
                    break;

                case InterpolationMode::CubicSpline:
                    scale = glm::hermite(it_lhs->second.value, it_lhs->second.out_tangent, it_rhs->second.value,
                                         it_rhs->second.in_tangent, frac);
                    break;
            }
            out_transform.scale = scale;
        }
    }
    return !keys.positions.empty() || !keys.rotations.empty() || !keys.scales.empty();
}

bool create_morph_weights(const animation_keys_t &keys, float time, InterpolationMode interpolation_mode,
                          std::vector<double> &out_weights)
{
    if(!keys.morph_weights.empty())
    {
        // find a key with equal or greater time
        auto it_rhs = keys.morph_weights.lower_bound(time);

        // lhs iterator (might be invalid)
        auto it_lhs = it_rhs;
        it_lhs--;

        if(it_rhs == keys.morph_weights.begin())
        {
            // time is before first key
            out_weights = it_rhs->second.value;
        }
        else if(it_rhs == keys.morph_weights.end())
        {
            // time is past last key
            out_weights = it_lhs->second.value;
        }
        else
        {
            // interpolate two surrounding keys
            auto &[start_time, start_value] = *it_lhs;
            auto &[end_time, end_value] = *it_rhs;
            double frac = std::max<double>((time - start_time) / (end_time - start_time), 0.0);
            out_weights.resize(start_value.value.size(), 0.f);

            for(uint32_t i = 0; i < out_weights.size(); ++i)
            {
                switch(interpolation_mode)
                {
                    case InterpolationMode::Step: frac = 0.f; [[fallthrough]];
                    case InterpolationMode::Linear:
                        out_weights[i] = glm::mix(start_value.value[i], end_value.value[i], frac);
                        break;
                    case InterpolationMode::CubicSpline:
                        out_weights[i] = vierkant::hermite(start_value.value[i], start_value.out_tangent[i],
                                                           end_value.value[i], end_value.in_tangent[i], frac);
                        break;
                }
            }
        }
        return true;
    }
    return false;
}

template<typename T>
bool operator==(const animation_component_t_<T> &lhs, const animation_component_t_<T> &rhs)
{
    if(lhs.index != rhs.index) { return false; }
    if(lhs.current_time != rhs.current_time) { return false; }
    if(lhs.animation_speed != rhs.animation_speed) { return false; }
    if(lhs.playing != rhs.playing) { return false; }
    return true;
}

template bool operator==(const animation_component_t_<float> &lhs, const animation_component_t_<float> &rhs);
template bool operator==(const animation_component_t_<double> &lhs, const animation_component_t_<double> &rhs);

}// namespace vierkant

template<typename T>
size_t
std::hash<vierkant::animation_component_t_<T>>::operator()(vierkant::animation_component_t_<T> const &animation_state) const
{
    size_t h = 0;
    vierkant::hash_combine(h, animation_state.index);
    vierkant::hash_combine(h, animation_state.current_time);
    vierkant::hash_combine(h, animation_state.animation_speed);
    vierkant::hash_combine(h, animation_state.playing);
    return h;
}

template size_t std::hash<vierkant::animation_component_t_<float>>::operator()(
        vierkant::animation_component_t_<float> const &animation_state) const;
template size_t std::hash<vierkant::animation_component_t_<double>>::operator()(
        vierkant::animation_component_t_<double> const &animation_state) const;