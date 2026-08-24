#include <spdlog/spdlog.h>

#include <serialization/optional_nvp_json.hpp>

#include "vierkant_ed_serialization.hpp"

namespace vierkant_ed
{

void save_settings(std::ostream &os, const VierkantEd::settings_t &settings)
{
    cereal::JSONOutputArchive archive(os);
    archive(settings);
}

std::optional<VierkantEd::settings_t> load_settings(std::istream &is)
{
    try
    {
        VierkantEd::settings_t settings = {};
        cereal::JSONInputArchive archive(is);
        archive(settings);
        return settings;
    } catch(const std::exception &) { return {}; }
}

void save_scene_data(std::ostream &os, const scene_data_t &data)
{
    cereal::JSONOutputArchive archive(os);
    archive(data);
}

std::optional<scene_data_t> load_scene_data(std::istream &is)
{
    try
    {
        scene_data_t scene_data;
        cereal::JSONInputArchive archive(is);
        archive(scene_data);
        return scene_data;
    } catch(const std::exception &e)
    {
        spdlog::error("could not parse scene-data: {}", e.what());
        return {};
    }
}

}// namespace vierkant_ed
