#pragma once

#include "model_loading.hpp"

namespace vierkant::model
{

/**
 *  @brief  wavefront_obj can be used to load 3d-models in the Wavefront obj-format.
 *
 *  @param  path    path to a model-file with .obj extension.
 *
 *  @return a struct grouping the loaded assets.
 */
mesh_assets_t wavefront_obj(const std::filesystem::path &path, crocore::ThreadPool* pool = nullptr);

}// namespace vierkant::model

