#pragma once

#include <vierkant/math.hpp>
#include <vierkant/object_component.hpp>

namespace vierkant
{

//! tuning- and input-state for a character-controller.
//! pure data: the input-fields are filled by an arbitrary producer (player-input, ai, ...),
//! the tuning-fields are consumed when turning that input into forces.
struct player_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();

    //! desired horizontal top-speed (m/s)
    float max_speed = 4.5f;

    //! time to reach the desired velocity, on the ground / in the air (s)
    float t_accel_ground = .08f;
    float t_accel_air = .6f;

    //! cap for the acceleration used to reach the desired velocity (m/s^2)
    float max_accel_ground = 40.f;
    float max_accel_air = 8.f;

    //! apex of a jump from standing (m)
    float jump_height = 1.1f;

    //! maximum slope-angle that can still be walked on (radians)
    float max_slope_angle = glm::radians(50.f);

    //! eye-/camera-offset above the object's origin (m)
    float eye_height = 1.7f;

    //! desired movement, clamped to the unit-disc. x: right, y: forward
    glm::vec2 move = {};

    //! jump-input, consumed on its rising edge
    bool jump = false;

    //! view-orientation (radians). movement is derived from yaw only, pitch must not tilt it
    float yaw = 0.f;
    float pitch = 0.f;
};

}// namespace vierkant
