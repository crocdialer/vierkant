//
// Created by crocdialer on 12/30/21.
//

#pragma once

#include <future>
#include <functional>

namespace vierkant
{

//! signature for a processing-delegate. 'can' be used to delegate processing to an external thread-pool.
template<typename T>
using delegate_fn_t_ = std::function<std::future<T>(const std::function<T()>&)>;

//! predefine most common void-flavor
using delegate_fn_t = delegate_fn_t_<void>;

}// namespace vierkant
