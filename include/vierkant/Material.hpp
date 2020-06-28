//
// Created by crocdialer on 7/27/19.
//

#pragma once

#include "vierkant/Image.hpp"
#include "vierkant/Pipeline.hpp"
#include "vierkant/Geometry.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(Material);

class Material
{
public:

    enum TextureType
    {
        Color, Normal, Specular, Ao_rough_metal, Emission, Displacement, Environment
    };

    static MaterialPtr create(){ return MaterialPtr(new Material()); };

    glm::vec4 color = glm::vec4(1);

    glm::vec4 emission = glm::vec4(0);

    float metalness = 0.f;

    float roughness = 1.f;

    float occlusion = 0.f;

    bool blending = false;

    bool depth_test = true;

    bool depth_write = true;

    VkCullModeFlagBits cull_mode = VK_CULL_MODE_BACK_BIT;

    std::map<TextureType, vierkant::ImagePtr> textures;

private:
    Material() = default;
};

}
