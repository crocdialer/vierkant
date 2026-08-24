#pragma once

#include <cereal/cereal.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/unordered_set.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/vector.hpp>

#include <cereal/archives/json.hpp>

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

template<typename T>
void prologue(JSONInputArchive &, const OptionalNameValuePair<T> &)
{}

template<typename T>
void prologue(JSONOutputArchive &, const OptionalNameValuePair<T> &)
{}

template<typename T>
void epilogue(JSONInputArchive &, const OptionalNameValuePair<T> &)
{}

template<typename T>
void epilogue(JSONOutputArchive &, const OptionalNameValuePair<T> &)
{}

template<class T>
inline void CEREAL_SAVE_FUNCTION_NAME(JSONOutputArchive &ar, OptionalNameValuePair<T> const &t)
{
    ar.setNextName(t.name);
    ar(t.value);
}

template<class T>
inline void CEREAL_LOAD_FUNCTION_NAME(JSONInputArchive &ar, OptionalNameValuePair<T> &t)
{
    ar.setNextName(t.name);

    try
    {
        ar(t.value);
    } catch(const Exception &e)
    {
        ar.setNextName(nullptr);

        if(std::string(e.what()).find("provided NVP (" + std::string(t.name)) == std::string::npos) { throw; }
        else { t.value = t.defaultValue; }
    }
}

}// namespace cereal
