//
// Created by crocdialer on 09.09.23.
//

#pragma once

#include <variant>
#include <vierkant/math.hpp>
#include <vierkant/object_component.hpp>

namespace vierkant
{

struct ortho_camera_params_t
{
    // clipping planes distances
    // NOTE: fun-fact: 'near' and 'far' are reserved (non-standard) keywords for msvc
    float left = -1.f;
    float right = 1.f;
    float bottom = -1.f;
    float top = 1.f;
    float near_ = 0.f;
    float far_ = 100.f;
};

struct physical_camera_params_t
{
    //! focal length in m
    float focal_length = 0.05f;

    //! horizontal sensor-size in m
    float sensor_width = 0.036f;

    //! sensor aspect-ratio (w/h)
    float aspect = 16.f / 9.f;

    //! camera near/far clipping distances in meter
    glm::vec2 clipping_distances = {0.1f, 100.f};

    //! focal distance in meter
    float focal_distance = 10.f;

    //! f-stop value
    float fstop = 2.8f;

    //! aperture/lens size in m
    [[nodiscard]] inline double aperture_size() const { return focal_length / fstop; }

    //! horizontal field-of-view (fov) in radians
    [[nodiscard]] inline float fovx() const { return 2 * std::atan(0.5f * sensor_width / focal_length); }

    //! horizontal field-of-view (fov) in radians
    [[nodiscard]] inline float fovy() const { return fovx() / aspect; }

    //! will adjust focal_length to match provided field-of-view (fov) in radians
    inline void set_fovx(float fovx) { focal_length = 0.5f * sensor_width / std::tan(fovx * 0.5f); }
};

using camera_params_variant_t = std::variant<ortho_camera_params_t, physical_camera_params_t>;

}// namespace vierkant
