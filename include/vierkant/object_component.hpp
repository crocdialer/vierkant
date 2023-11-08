//
// Created by crocdialer on 08.11.23.
//

#pragma once

#include <concepts>

namespace vierkant
{

namespace internal
{
struct object_component_guard_t;
}

//! helper macro to enable arbitrary types to be used as object-components
#define VIERKANT_ENABLE_AS_COMPONENT() using Guard = vierkant::internal::object_component_guard_t

//! concept definition for an object-component
template<class T>
concept object_component =
        requires() { std::same_as<typename std::remove_pointer_t<T>::Guard, internal::object_component_guard_t>; };
}// namespace vierkant
