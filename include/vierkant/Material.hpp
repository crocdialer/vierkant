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
        Color,
        Normal,
        Specular,
        Ao_rough_metal,
        Emission,
        Displacement,
        Thickness,
        Transmission,
        Clearcoat,
        SheenColor,
        SheenRoughness,
        Environment
    };

    static MaterialPtr create(){ return MaterialPtr(new Material()); };

    std::string name;

    glm::vec4 color = glm::vec4(1);

    glm::vec3 emission = glm::vec3(0);

    float metalness = 1.f;

    float roughness = 1.f;

    float occlusion = 1.f;

    bool two_sided = false;

    BlendMode blend_mode = BlendMode::Opaque;

    float alpha_cutoff = 0.5f;

    float transmission = 0.f;

    glm::vec3 attenuation_color = glm::vec3(1.f);

    float attenuation_distance = std::numeric_limits<float>::infinity();

    float ior = 1.5f;

    float clearcoat_factor = 0.f;

    float clearcoat_roughness_factor = 0.f;

    glm::vec3 sheen_color = glm::vec3(0.f);

    float sheen_roughness = 0.f;

    bool depth_test = true;

    bool depth_write = true;

    VkCullModeFlagBits cull_mode = VK_CULL_MODE_BACK_BIT;

    std::map<TextureType, vierkant::ImagePtr> textures;

private:
    Material() = default;
};

}
