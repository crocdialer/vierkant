#pragma once

#include <crocore/ThreadPool.hpp>
#include <vierkant/model_loading.hpp>

namespace vierkant::model
{

//! load a single 3D model from file
mesh_assets_t load_model(const std::string &path, const crocore::ThreadPool &threadpool);

//! load animations from file and add to existing geometry
size_t add_animations_to_mesh(const std::string &path, mesh_assets_t &mesh_assets);

} //namespace vierkant::model
