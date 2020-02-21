#pragma once

#include <vierkant/Geometry.hpp>

namespace vierkant::assimp
{

//! load a single 3D model from file
vierkant::GeometryPtr load_model(const std::string &path);

//! load animations from file and add to existing geometry
size_t add_animations_to_mesh(const std::string &path, vierkant::GeometryPtr mesh);
    
} //namespace vierkant::assimp
