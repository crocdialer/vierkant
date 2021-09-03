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

    enum class BlendMode : uint8_t
    {
        Opaque = 0,
        Blend = 1,
        Mask = 2
    };

    enum TextureType
    {
        Color, Normal, Specular, Ao_rough_metal, Emission, Displacement, Environment
    };

    static MaterialPtr create(){ return MaterialPtr(new Material()); };

    glm::vec4 color = glm::vec4(1);

    glm::vec4 emission = glm::vec4(0);

    float metalness = 0.f;

    float roughness = 1.f;

    float ambient = 1.f;

    bool two_sided = false;

    BlendMode blend_mode = BlendMode::Opaque;

    float alpha_cutoff = 0.5f;

    float refraction = 1.5f;

    bool depth_test = true;

    bool depth_write = true;

    VkCullModeFlagBits cull_mode = VK_CULL_MODE_BACK_BIT;

    std::map<TextureType, vierkant::ImagePtr> textures;

private:
    Material() = default;
};

}
