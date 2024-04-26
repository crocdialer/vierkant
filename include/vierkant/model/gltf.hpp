//
// Created by crocdialer on 9/3/21.
//

#pragma once

#include <vierkant/model/model_loading.hpp>

namespace vierkant::model
{

/**
 *  @brief  gltf can be used to load 3d-models in the GL Transmission Format 2.0 (glTF2).
 *          json-, json-embedded and binary flavours are supported.
 *
 *  @param  path    path to a model-file with either .gltf or .glb extension.
 *
 *  @return an optional struct grouping the loaded assets.
 */
std::optional<model_assets_t> gltf(const std::filesystem::path &path, crocore::ThreadPool* pool = nullptr);

}// namespace vierkant::model
