#pragma once

#include <vierkant/math.hpp>

namespace vierkant
{

inline static uint32_t cast_color_unorm(const glm::vec4 &color)
{
    uint32_t ret = 0;
    ret |= static_cast<uint32_t>(std::clamp(color.x, 0.f, 1.f) * 255);
    ret |= static_cast<uint32_t>(std::clamp(color.y, 0.f, 1.f) * 255) << 8;
    ret |= static_cast<uint32_t>(std::clamp(color.z, 0.f, 1.f) * 255) << 16;
    ret |= static_cast<uint32_t>(std::clamp(color.w, 0.f, 1.f) * 255) << 24;
    return ret;
}

inline static glm::vec4 cast_color_unorm(uint32_t color)
{
    glm::vec4 ret;
    ret.x = (color & 0xFF) / 255.f;
    ret.y = ((color >> 8) & 0xFF) / 255.f;
    ret.z = ((color >> 16) & 0xFF) / 255.f;
    ret.w = ((color >> 24) & 0xFF) / 255.f;
    return ret;
}

}// namespace vierkant
