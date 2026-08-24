#pragma once

#include <filesystem>
#include <iosfwd>
#include <optional>

#include <vierkant/Material.hpp>
#include <vierkant/model/model_loading.hpp>

namespace vierkant
{

void save(std::ostream &os, const vierkant::model::model_assets_t &assets);
std::optional<vierkant::model::model_assets_t> load_model_assets(std::istream &is);

void save(std::ostream &os, const vierkant::material_data_t &data);
std::optional<vierkant::material_data_t> load_material_data(std::istream &is);

//! bundle baking --------------------------------------------------------------------------------

//! parameters controlling how a model-file is baked into a self-contained asset-bundle.
struct bundle_params_t
{
    //! mesh-buffer creation parameters (lods, meshlets, vertex-packing, ...).
    vierkant::mesh_buffer_params_t mesh_buffer_params = {};

    //! run in-place block-compression (BC7/BC5) on all textures.
    bool compress_textures = false;

    //! optional opacity-micromap baking; when set, baked OMM data is cached into the bundle.
    std::optional<vierkant::model::omm_gen_params_t> omm_params;

    //! optional stable string seeding deterministic asset-ids (default: the model-path).
    //! pass a location-independent key (e.g. project-root-relative) to make baked ids portable.
    std::string id_seed;

    //! optional thread-pool used to parallelize loading/compression.
    crocore::ThreadPoolClassic *pool = nullptr;
};

//! canonical suffix for baked asset-bundles.
constexpr char bundle_file_suffix[] = "4km";

//! compute the canonical bundle-filename for a model (e.g. "model.glb_<hash>.4km"). the hash
//! covers the filename + bake-parameters + schema-version + serialized layout-identity.
std::string model_bundle_filename(const std::filesystem::path &model_path,
                                  const vierkant::mesh_buffer_params_t &mesh_buffer_params, bool compress_textures,
                                  const std::optional<vierkant::model::omm_gen_params_t> &omm_params = {});

//! load a model-file and bake a self-contained asset-bundle (CPU-only, no Vulkan device required).
std::optional<vierkant::model::model_assets_t> create_model_bundle(const std::filesystem::path &model_path,
                                                                   const bundle_params_t &params);

//! zip-aware bundle file IO ---------------------------------------------------------------------
//
// the following helpers (de)serialize bundles to/from a file at 'path'. when an optional
// 'zip_archive' path is provided, files are stored zstd-compressed inside that archive (the plain
// file is removed after) and lookups fall back to that archive when the plain file is absent.

//! save a baked model-asset-bundle to 'path' (optionally into 'zip_archive').
void save_bundle_file(const vierkant::model::model_assets_t &assets, const std::filesystem::path &path,
                      const std::optional<std::filesystem::path> &zip_archive = {});

//! load a model-asset-bundle from 'path' (with fallback to 'zip_archive').
std::optional<vierkant::model::model_assets_t>
load_model_bundle_file(const std::filesystem::path &path, const std::optional<std::filesystem::path> &zip_archive = {});

//! save a material-bundle to 'path' (optionally into 'zip_archive').
void save_bundle_file(const vierkant::material_data_t &material_data, const std::filesystem::path &path,
                      const std::optional<std::filesystem::path> &zip_archive = {});

//! load a material-bundle from 'path' (with fallback to 'zip_archive').
std::optional<vierkant::material_data_t>
load_material_bundle_file(const std::filesystem::path &path,
                          const std::optional<std::filesystem::path> &zip_archive = {});

}// namespace vierkant
