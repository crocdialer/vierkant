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

    enum TextureType : uint32_t
    {
        Color = 0x01,
        Normal = 0x02,
        Ao_rough_metal = 0x04,
        Emission = 0x08,
        Displacement = 0x10,
        Thickness = 0x20,
        Transmission = 0x40,
        Clearcoat = 0x80,
        SheenColor = 0x100,
        SheenRoughness = 0x200,
        Environment = 0x400
    };

    static MaterialPtr create(){ return MaterialPtr(new Material()); };

    [[nodiscard]] std::size_t hash() const;

    std::string name;

    glm::vec4 color = glm::vec4(1);

    glm::vec4 emission = glm::vec4(0, 0, 0, 1);

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