//
// Created by crocdialer on 9/5/20.
//

#pragma once

#include <cstdint>

namespace vierkant::postfx
{

//! Describes the buffer-layout for Depth
struct dof_settings_t
{
    //! flag indicating if DoF should be applied
    uint32_t enabled = false;

    //! focal distance in meters, but you may use auto_focus option below
    float focal_depth = 5;

    //! focal length in mm
    float focal_length = 180;

    //! f-stop value
    float fstop = 40;

    //! circle of confusion size in mm (35mm film = 0.03mm)
    float circle_of_confusion_sz = 0.03f;

    //! highlight gain
    float gain = 2.f;

    //! bokeh chromatic aberration/fringing
    float fringe = 0.7f;

    //! determines max blur amount
    float max_blur = 2.f;

    //! use auto-focus in shader? disable if you use external focal_depth value
    uint32_t auto_focus = true;

    //! show debug focus point and focal range (red = focal point, green = focal range)
    uint32_t debug_focus = false;

    //! padding to comply with std-140 buffer layout
    uint32_t padding[2]{};
};

}// namespace vierkant
