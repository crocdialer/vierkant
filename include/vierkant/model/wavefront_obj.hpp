#pragma once

#include "model_loading.hpp"

namespace vierkant::model
{

/**
 *  @brief  wavefront_obj can be used to load 3d-models in the Wavefront obj-format.
 *
 *  @param  path    path to a model-file with .obj extension.
 *
 *  @return an optional struct grouping the loaded assets.
 */
std::optional<model_assets_t> wavefront_obj(const std::filesystem::path &path,
                                            crocore::ThreadPoolClassic *pool = nullptr);

}// namespace vierkant::model
