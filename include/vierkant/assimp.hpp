#pragma once

#include <vierkant/Geometry.hpp>
#include <vierkant/Material.hpp>

namespace vierkant::assimp
{

struct mesh_assets_t
{
    std::vector<vierkant::GeometryPtr> geometries;
    std::vector<vierkant::MaterialPtr> materials;
    vierkant::bones::BonePtr root_bone;
    std::vector<vierkant::bones::animation_t> animations;
};

//! load a single 3D model from file
mesh_assets_t load_model(const std::string &path);

//! load animations from file and add to existing geometry
//size_t add_animations_to_mesh(const std::string &path, vierkant::MeshPtr mesh);

} //namespace vierkant::assimp
