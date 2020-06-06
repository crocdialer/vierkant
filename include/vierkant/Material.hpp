//
// Created by crocdialer on 7/27/19.
//

#pragma once

#include "vierkant/Image.hpp"
#include "vierkant/Pipeline.hpp"
#include "vierkant/Geometry.hpp"

namespace vierkant {

DEFINE_CLASS_PTR(Material);

class Material
{
public:

    static MaterialPtr create() { return MaterialPtr(new Material()); };

    vierkant::ShaderType shader_type = vierkant::ShaderType::UNLIT_COLOR;

    glm::vec4 color = glm::vec4(1);

    glm::vec4 emission = glm::vec4(0);

    float metalness = 0.f;

    float roughness = 1.f;

    bool blending = false;

    bool depth_test = true;

    bool depth_write = true;

    VkCullModeFlagBits cull_mode = VK_CULL_MODE_BACK_BIT;

    std::vector<vierkant::ImagePtr> images;

    std::vector<std::vector<uint8_t>> ubos;


private:
    Material() = default;
};

}
