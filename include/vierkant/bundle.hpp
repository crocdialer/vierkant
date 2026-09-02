#pragma once

#include <filesystem>
#include <iosfwd>
#include <optional>

#include <vierkant/Material.hpp>
#include <vierkant/cubemap_data.hpp>
#include <vierkant/model/model_loading.hpp>

namespace vierkant
{

void save(std::ostream &os, const vierkant::model::model_assets_t &assets);
std::optional<vierkant::model::model_assets_t> load_model_assets(std::istream &is);

void save(std::ostream &os, const vierkant::material_data_t &data);
std::optional<vierkant::material_data_t> load_material_data(std::istream &is);

void save(std::ostream &os, const vierkant::texture_variant_t &texture);
std::optional<vierkant::texture_variant_t> load_texture_variant(std::istream &is);

//! the derived cubemaps for an equirectangular environment-map.
struct environment_assets_t
{
    //! the panorama projected onto a mipmapped cubemap
    vierkant::cubemap_data_t skybox;

    //! diffuse (lambert) convolution of the skybox
    vierkant::cubemap_data_t conv_lambert;

    //! roughness-cascade of specular (ggx) convolutions, one per mip-level
    vierkant::cubemap_data_t conv_ggx;
};

void save(std::ostream &os, const vierkant::environment_assets_t &assets);
std::optional<vierkant::environment_assets_t> load_environment_assets(std::istream &is);

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

//! canonical suffix for cached, decoded/compressed images.
constexpr char texture_bundle_file_suffix[] = "4kt";

//! canonical suffix for cached environment-cubemaps.
constexpr char environment_bundle_file_suffix[] = "4ke";

//! schema-versions folded into the texture-/environment-cache keys; bump on any payload change.
constexpr uint32_t texture_schema_version = 1;
constexpr uint32_t environment_schema_version = 2;

//! compute the canonical bundle-filename for a model (e.g. "model.glb_<hash>.4km"). the hash
//! covers the filename + bake-parameters + schema-version + serialized layout-identity.
std::string model_bundle_filename(const std::filesystem::path &model_path,
                                  const vierkant::mesh_buffer_params_t &mesh_buffer_params, bool compress_textures,
                                  const std::optional<vierkant::model::omm_gen_params_t> &omm_params = {});

//! compute the canonical cache-filename for an image (e.g. "gobo.png_<hash>.4kt"). the hash covers
//! the whole key (not just the filename), the compression-settings and the schema-version.
std::string texture_bundle_filename(const std::filesystem::path &image_key, bool compress_texture,
                                    vierkant::bcn::CompressionMode mode);

//! compute the canonical cache-filename for an environment-map (e.g. "sky.hdr_<hash>.4ke").
std::string environment_bundle_filename(const std::filesystem::path &image_key, VkFormat format,
                                        uint32_t lambert_size);

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

//! save a decoded/compressed image to 'path' (optionally into 'zip_archive').
void save_bundle_file(const vierkant::texture_variant_t &texture, const std::filesystem::path &path,
                      const std::optional<std::filesystem::path> &zip_archive = {});

//! load a cached image from 'path' (with fallback to 'zip_archive').
std::optional<vierkant::texture_variant_t>
load_texture_bundle_file(const std::filesystem::path &path,
                         const std::optional<std::filesystem::path> &zip_archive = {});

//! save baked environment-cubemaps to 'path' (optionally into 'zip_archive').
void save_bundle_file(const vierkant::environment_assets_t &assets, const std::filesystem::path &path,
                      const std::optional<std::filesystem::path> &zip_archive = {});

//! load baked environment-cubemaps from 'path' (with fallback to 'zip_archive').
std::optional<vierkant::environment_assets_t>
load_environment_bundle_file(const std::filesystem::path &path,
                             const std::optional<std::filesystem::path> &zip_archive = {});

}// namespace vierkant
