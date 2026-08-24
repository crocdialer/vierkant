#pragma once

//! JSON-archive support for OptionalNameValuePair: a missing key falls back to the default.
//!
//! kept apart from optional_nvp_cereal.hpp because <cereal/archives/json.hpp> drags in rapidjson
//! (~37 headers). include this from translation-units that archive to JSON; the binary
//! asset-bundle path never pays for it.

#include <cereal/archives/json.hpp>

#include "optional_nvp_cereal.hpp"

namespace cereal
{

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
