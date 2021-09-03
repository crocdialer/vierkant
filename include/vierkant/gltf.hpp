//
// Created by crocdialer on 9/3/21.
//

#pragma once

#include <vierkant/model_loading.hpp>

namespace vierkant::model
{

/**
 *
 * @param path
 * @return
 */
mesh_assets_t gltf(const std::filesystem::path &path);

}// namespace vierkant::model
