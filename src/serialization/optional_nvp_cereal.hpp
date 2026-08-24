#pragma once

#include <cereal/cereal.hpp>

namespace cereal
{

//! optional value support
template<typename T>
struct OptionalNameValuePair : public NameValuePair<T>
{
    OptionalNameValuePair(char const *name, T &&value, std::remove_reference_t<T> defaultValue_)
        : NameValuePair<T>(name, std::forward<T>(value)), defaultValue(std::move(defaultValue_))
    {}

    std::remove_reference_t<T> defaultValue;
};

template<typename T>
OptionalNameValuePair<T> make_optional_nvp(const std::string &name, T &&value,
                                           std::remove_reference_t<T> defaultValue = std::remove_reference_t<T>())
{
    return {name.c_str(), std::forward<T>(value), std::move(defaultValue)};
}

template<typename T>
OptionalNameValuePair<T> make_optional_nvp(const char *name, T &&value,
                                           std::remove_reference_t<T> defaultValue = std::remove_reference_t<T>())
{
    return {name, std::forward<T>(value), std::move(defaultValue)};
}
}// namespace cereal
