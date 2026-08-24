#include <format>
#include <fstream>
#include <shared_mutex>

#include <crocore/filesystem.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

#include <cereal/archives/binary.hpp>

#include <vierkant/bundle.hpp>
#include <vierkant/hash.hpp>
#include <vierkant/vertex_splicer.hpp>
#include <vierkant/ziparchive.h>

#include "bundle_format.hpp"

namespace vierkant
{

static std::shared_mutex g_bundle_rw_mutex;

//! archive-relative entry-name for a bundle-path, keeping machine-local absolute paths out of archives.
static std::filesystem::path zip_entry_path(const std::filesystem::path &path,
                                            const std::filesystem::path &zip_archive)
{
    auto rel = path.lexically_relative(zip_archive.parent_path());
    if(rel.empty() || *rel.begin() == "..") { return path.generic_string(); }
    return rel.generic_string();
}

template<typename T, typename Reader>
static std::optional<T> load_from_stream(const std::filesystem::path &path,
                                         const std::optional<std::filesystem::path> &zip_archive, Reader &&reader)
{
    {
        std::ifstream f(path);
        if(f.is_open())
        {
            try
            {
                spdlog::debug("loading bundle '{}'", path.string());
                std::shared_lock lock(g_bundle_rw_mutex);
                return reader(f);
            } catch(std::exception &e) { spdlog::error(e.what()); }
        }
    }
    if(zip_archive)
    {
        vierkant::ziparchive zip(*zip_archive);
        auto entry_path = zip_entry_path(path, *zip_archive);
        if(zip.has_file(entry_path))
        {
            try
            {
                spdlog::debug("loading bundle '{}' from archive '{}'", entry_path.string(), zip_archive->string());
                std::shared_lock lock(g_bundle_rw_mutex);
                auto zipstream = zip.open_file(entry_path);
                return reader(zipstream);
            } catch(std::exception &e) { spdlog::error(e.what()); }
        }
    }
    return {};
}

template<typename Writer>
static void save_to_stream(const std::filesystem::path &path, const std::optional<std::filesystem::path> &zip_archive,
                           Writer &&writer)
{
    try
    {
        {
            spdlog::stopwatch sw;
            if(auto dir = crocore::filesystem::get_directory_part(path); !dir.empty())
                std::filesystem::create_directories(dir);
            std::ofstream ofs(path.string(), std::ios_base::out | std::ios_base::binary);
            spdlog::debug("serializing/writing bundle: {}", path.string());
            writer(ofs);
            spdlog::debug("done serializing/writing bundle: {} ({})", path.string(), sw.elapsed());
        }
        if(zip_archive)
        {
            spdlog::stopwatch sw;
            {
                std::unique_lock lock(g_bundle_rw_mutex);
                spdlog::debug("adding bundle to compressed archive: {} -> {}", path.string(), zip_archive->string());
                vierkant::ziparchive zipstream(*zip_archive);
                zipstream.add_file(path, zip_entry_path(path, *zip_archive));
            }
            spdlog::debug("done compressing bundle: {} -> {} ({})", path.string(), zip_archive->string(), sw.elapsed());
            std::filesystem::remove(path);
        }
    } catch(std::exception &e) { spdlog::error(e.what()); }
}


void save(std::ostream &os, const vierkant::model::model_assets_t &assets)
{
    cereal::BinaryOutputArchive archive(os);
    archive(assets);
}

std::optional<vierkant::model::model_assets_t> load_model_assets(std::istream &is)
{
    try
    {
        vierkant::model::model_assets_t ret;
        cereal::BinaryInputArchive archive(is);
        archive(ret);
        return ret;
    } catch(const std::exception &) { return {}; }
}

void save(std::ostream &os, const vierkant::material_data_t &data)
{
    cereal::BinaryOutputArchive archive(os);
    archive(data);
}

std::optional<vierkant::material_data_t> load_material_data(std::istream &is)
{
    try
    {
        vierkant::material_data_t ret;
        cereal::BinaryInputArchive archive(is);
        archive(ret);
        return ret;
    } catch(const std::exception &) { return {}; }
}


std::string model_bundle_filename(const std::filesystem::path &model_path,
                                  const vierkant::mesh_buffer_params_t &mesh_buffer_params, bool compress_textures,
                                  const std::optional<vierkant::model::omm_gen_params_t> &omm_params)
{
    size_t hash_val = std::hash<std::string>()(model_path.filename().string());
    vierkant::hash_combine(hash_val, vierkant::model::bundle_schema_version);
    vierkant::hash_combine(hash_val, vierkant::serialized_layout_hash());
    vierkant::hash_combine(hash_val, mesh_buffer_params);
    vierkant::hash_combine(hash_val, compress_textures);

    // fold OMM settings into the cache-key so toggling/retuning OMM re-bakes the bundle
    vierkant::hash_combine(hash_val, omm_params.has_value());
    if(omm_params)
    {
        vierkant::hash_combine(hash_val, omm_params->max_level);
        vierkant::hash_combine(hash_val, omm_params->target_edge);
        vierkant::hash_combine(hash_val, omm_params->states);
    }
    return std::format("{}_{}.{}", model_path.filename().string(), hash_val, bundle_file_suffix);
}

std::optional<vierkant::model::model_assets_t> create_model_bundle(const std::filesystem::path &model_path,
                                                                   const bundle_params_t &params)
{
    spdlog::stopwatch sw;
    auto model_assets = vierkant::model::load_model(model_path, params.pool, params.id_seed);

    if(!model_assets)
    {
        spdlog::warn("could not load file: {}", model_path.string());
        return {};
    }
    spdlog::debug("loaded model '{}' ({})", model_path.string(), sw.elapsed());

    spdlog::debug("baking asset-bundle '{}' - lods: {} - meshlets: {} - bc7-compression: {}", model_path.string(),
                  params.mesh_buffer_params.generate_lods, params.mesh_buffer_params.generate_meshlets,
                  params.compress_textures);

    // run compression of geometries, creation of meshlets, lods, etc.
    model_assets->geometry_data = vierkant::create_mesh_buffers(
            std::get<std::vector<vierkant::Mesh::entry_create_info_t>>(model_assets->geometry_data),
            params.mesh_buffer_params);

    // bake opacity-micromaps now: the packed bundle AND CPU alpha coexist only here, before
    // the block-compression below destroys CPU alpha. baked data is cached into the bundle.
    if(params.omm_params)
    {
        const auto &bundle = std::get<vierkant::mesh_buffer_bundle_t>(model_assets->geometry_data);
        model_assets->omm_data = vierkant::model::generate_omm_data(*model_assets, bundle, *params.omm_params);
        spdlog::debug("baked opacity-micromaps for '{}': {} entry(ies)", model_path.string(),
                      model_assets->omm_data.size());
    }

    // run in-place block-compression on all textures, store compressed textures in bundle
    if(params.compress_textures) { vierkant::model::compress_textures(*model_assets, params.pool); }

    spdlog::debug("asset-bundle '{}' done -> {}", model_path.string(), sw.elapsed());
    return model_assets;
}

void save_bundle_file(const vierkant::model::model_assets_t &assets, const std::filesystem::path &path,
                      const std::optional<std::filesystem::path> &zip_archive)
{
    save_to_stream(path, zip_archive, [&assets](std::ostream &os) { save(os, assets); });
}

std::optional<vierkant::model::model_assets_t>
load_model_bundle_file(const std::filesystem::path &path, const std::optional<std::filesystem::path> &zip_archive)
{
    return load_from_stream<vierkant::model::model_assets_t>(path, zip_archive,
                                                             [](std::istream &is) { return load_model_assets(is); });
}

void save_bundle_file(const vierkant::material_data_t &material_data, const std::filesystem::path &path,
                      const std::optional<std::filesystem::path> &zip_archive)
{
    save_to_stream(path, zip_archive, [&material_data](std::ostream &os) { save(os, material_data); });
}

std::optional<vierkant::material_data_t>
load_material_bundle_file(const std::filesystem::path &path, const std::optional<std::filesystem::path> &zip_archive)
{
    return load_from_stream<vierkant::material_data_t>(path, zip_archive,
                                                       [](std::istream &is) { return load_material_data(is); });
}

}// namespace vierkant
