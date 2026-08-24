#pragma once

#include <vierkant/serialization/optional_nvp_cereal.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/memory.hpp>
#include <vierkant/physics_context.hpp>
#include <vierkant/character.hpp>

namespace vierkant::collision
{

template<class Archive>
void serialize(Archive &, vierkant::collision::none_t &)
{}

template<class Archive>
void serialize(Archive &archive, vierkant::collision::plane_t &s)
{ archive(cereal::make_nvp("coefficients", s.coefficients), cereal::make_nvp("half_extents", s.half_extent)); }

template<class Archive>
void serialize(Archive &archive, vierkant::collision::box_t &s)
{ archive(cereal::make_nvp("half_extents", s.half_extents)); }

template<class Archive>
void serialize(Archive &archive, vierkant::collision::sphere_t &s)
{ archive(cereal::make_nvp("normal", s.radius)); }

template<class Archive>
void serialize(Archive &archive, vierkant::collision::cylinder_t &s)
{ archive(cereal::make_nvp("radius", s.radius), cereal::make_nvp("height", s.height)); }

template<class Archive>
void serialize(Archive &archive, vierkant::collision::capsule_t &s)
{ archive(cereal::make_nvp("radius", s.radius), cereal::make_nvp("height", s.height)); }

template<class Archive>
void serialize(Archive &archive, vierkant::collision::mesh_t &s)
{
    archive(cereal::make_nvp("mesh_id", s.mesh_id), cereal::make_nvp("entry_indices", s.entry_indices),
            cereal::make_nvp("library", s.library), cereal::make_nvp("convex_hull", s.convex_hull),
            cereal::make_nvp("lod_bias", s.lod_bias));
}

}// namespace vierkant::collision

namespace vierkant::constraint
{

template<class Archive>
void serialize(Archive &archive, vierkant::constraint::spring_settings_t &s)
{
    archive(cereal::make_nvp("mode", s.mode), cereal::make_nvp("damping", s.damping),
            cereal::make_nvp("frequency_or_stiffness", s.frequency_or_stiffness));
}

template<class Archive>
void serialize(Archive &archive, vierkant::constraint::motor_t &s)
{
    archive(cereal::make_nvp("spring_settings", s.spring_settings),
            cereal::make_nvp("min_force_limit", s.min_force_limit),
            cereal::make_nvp("max_force_limit", s.max_force_limit),
            cereal::make_nvp("min_torque_limit", s.min_torque_limit),
            cereal::make_nvp("max_torque_limit", s.max_torque_limit), cereal::make_nvp("state", s.state),
            cereal::make_nvp("target_velocity", s.target_velocity),
            cereal::make_nvp("target_position", s.target_position));
}

template<class Archive>
void serialize(Archive &, vierkant::constraint::none_t &)
{}

template<class Archive>
void serialize(Archive &archive, vierkant::constraint::point_t &c)
{
    archive(cereal::make_nvp("space", c.space), cereal::make_nvp("point1", c.point1),
            cereal::make_nvp("point2", c.point2));
}

template<class Archive>
void serialize(Archive &archive, vierkant::constraint::distance_t &c)
{
    archive(cereal::make_nvp("space", c.space), cereal::make_nvp("point1", c.point1),
            cereal::make_nvp("point2", c.point2), cereal::make_nvp("min_distance", c.min_distance),
            cereal::make_nvp("max_distance", c.max_distance), cereal::make_nvp("spring_settings", c.spring_settings));
}

template<class Archive>
void serialize(Archive &archive, vierkant::constraint::slider_t &c)
{
    archive(cereal::make_nvp("space", c.space), cereal::make_nvp("auto_detect_point", c.auto_detect_point),
            cereal::make_nvp("point1", c.point1), cereal::make_nvp("slider_axis1", c.slider_axis1),
            cereal::make_nvp("point2", c.point2), cereal::make_nvp("slider_axis2", c.slider_axis2),
            cereal::make_nvp("limits_min", c.limits_min), cereal::make_nvp("limits_max", c.limits_max),
            cereal::make_nvp("limits_spring_settings", c.limits_spring_settings),
            cereal::make_nvp("max_friction_force", c.max_friction_force), cereal::make_nvp("motor", c.motor));
}

template<class Archive>
void serialize(Archive &archive, vierkant::constraint::hinge_t &c)
{
    archive(cereal::make_nvp("space", c.space), cereal::make_nvp("point1", c.point1),
            cereal::make_nvp("hinge_axis1", c.hinge_axis1), cereal::make_nvp("point2", c.point2),
            cereal::make_nvp("hinge_axis2", c.hinge_axis2), cereal::make_nvp("limits_min", c.limits_min),
            cereal::make_nvp("limits_spring_settings", c.limits_spring_settings),
            cereal::make_nvp("max_friction_torque", c.max_friction_torque), cereal::make_nvp("motor", c.motor));
}

template<class Archive>
void serialize(Archive &archive, vierkant::constraint::gear_t &c)
{
    archive(cereal::make_nvp("space", c.space), cereal::make_nvp("hinge_axis1", c.hinge_axis1),
            cereal::make_nvp("hinge_axis2", c.hinge_axis2), cereal::make_nvp("ratio", c.ratio));
}

template<class Archive>
void serialize(Archive &archive, vierkant::constraint::cone_t &c)
{
    archive(cereal::make_nvp("space", c.space), cereal::make_nvp("point1", c.point1),
            cereal::make_nvp("twist_axis1", c.twist_axis1), cereal::make_nvp("point2", c.point2),
            cereal::make_nvp("twist_axis2", c.twist_axis2), cereal::make_nvp("half_cone_angle", c.half_cone_angle));
}

template<class Archive>
void serialize(Archive &archive, vierkant::constraint::swing_twist_t &c)
{
    archive(cereal::make_nvp("space", c.space), cereal::make_nvp("position1", c.position1),
            cereal::make_nvp("twist_axis1", c.twist_axis1), cereal::make_nvp("position2", c.position2),
            cereal::make_nvp("twist_axis2", c.twist_axis2), cereal::make_nvp("swing_type", c.swing_type),
            cereal::make_nvp("normal_half_cone_angle", c.normal_half_cone_angle),
            cereal::make_nvp("plane_half_cone_angle", c.plane_half_cone_angle),
            cereal::make_nvp("twist_min_angle", c.twist_min_angle),
            cereal::make_nvp("twist_max_angle", c.twist_max_angle),
            cereal::make_nvp("max_friction_torque", c.max_friction_torque),
            cereal::make_nvp("swing_motor", c.swing_motor), cereal::make_nvp("twist_motor", c.twist_motor));
}

}// namespace vierkant::constraint

namespace vierkant
{
//! configuration and tuning only: move/jump/yaw/pitch are per-frame input, ground_state a simulation-result
template<class Archive>
void serialize(Archive &archive, vierkant::character_t &c)
{
    archive(cereal::make_nvp("max_slope_angle", c.max_slope_angle), cereal::make_nvp("max_speed", c.max_speed),
            cereal::make_nvp("t_accel_ground", c.t_accel_ground), cereal::make_nvp("t_accel_air", c.t_accel_air),
            cereal::make_nvp("max_accel_ground", c.max_accel_ground),
            cereal::make_nvp("max_accel_air", c.max_accel_air), cereal::make_nvp("jump_height", c.jump_height),
            cereal::make_optional_nvp("coyote_time", c.coyote_time, vierkant::character_t{}.coyote_time),
            cereal::make_nvp("eye_height", c.eye_height));
}

template<class Archive>
void serialize(Archive &archive, vierkant::physics_component_t &c)
{
    archive(cereal::make_optional_nvp("body_id", c.body_id), cereal::make_nvp("shape", c.shape),
            cereal::make_nvp("shape_transform", c.shape_transform), cereal::make_nvp("mass", c.mass),
            cereal::make_nvp("friction", c.friction), cereal::make_nvp("restitution", c.restitution),
            cereal::make_nvp("linear_damping", c.linear_damping),
            cereal::make_nvp("angular_damping", c.angular_damping), cereal::make_nvp("kinematic", c.kinematic),
            cereal::make_nvp("sensor", c.sensor), cereal::make_optional_nvp("character_state", c.character));
}

template<class Archive>
void serialize(Archive &archive, vierkant::constraint_component_t::body_constraint_t &c)
{
    archive(cereal::make_nvp("constraint", c.constraint), cereal::make_nvp("body_id1", c.body_id1),
            cereal::make_nvp("body_id2", c.body_id2));
}

template<class Archive>
void serialize(Archive &archive, vierkant::constraint_component_t &c)
{ archive(cereal::make_nvp("body_constraints", c.body_constraints)); }

}// namespace vierkant
