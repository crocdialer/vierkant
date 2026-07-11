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
 *  @param  id_seed optional stable string seeding deterministic asset-ids (default: path).
 *
 *  @return an optional struct grouping the loaded assets.
 */
std::optional<model_assets_t> gltf(const std::filesystem::path &path, crocore::ThreadPoolClassic* pool = nullptr,
                                   const std::string &id_seed = {});

}// namespace vierkant::model
