#pragma once

#include "vierkant/model/model_loading.hpp"
#include <crocore/ThreadPool.hpp>

namespace vierkant::model
{

//! load a single 3D model from file
mesh_assets_t load_model(const std::string &path, const crocore::ThreadPool &threadpool);

} //namespace vierkant::model