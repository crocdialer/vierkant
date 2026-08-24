#pragma once

#include <iosfwd>
#include <optional>

#include "vierkant_ed.hpp"

namespace vierkant_ed
{

void save_settings(std::ostream &os, const VierkantEd::settings_t &settings);
std::optional<VierkantEd::settings_t> load_settings(std::istream &is);

void save_scene_data(std::ostream &os, const scene_data_t &data);
std::optional<scene_data_t> load_scene_data(std::istream &is);

}// namespace vierkant_ed
