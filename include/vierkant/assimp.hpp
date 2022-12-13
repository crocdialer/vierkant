#pragma once

#include <crocore/ThreadPool.hpp>
#include <vierkant/model_loading.hpp>

namespace vierkant::model
{

//! load a single 3D model from file
mesh_assets_t load_model(const std::string &path, const crocore::ThreadPool &threadpool);

} //namespace vierkant::model