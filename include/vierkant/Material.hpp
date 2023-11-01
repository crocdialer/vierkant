//
// Created by crocdialer on 7/27/19.
//

#pragma once

#include "vierkant/Geometry.hpp"
#include "vierkant/Image.hpp"
#include "vierkant/Pipeline.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(Material)

class Material
{
public:
    enum class BlendMode : uint8_t
    {
        Opaque = 0,
        Blend = 1,
        Mask = 2
    };

    enum class CullMode : uint8_t
    {
        None = 0,
        Front,
        Back,
        FrontAndBack
    };

    enum TextureType : uint32_t
    {
        Color = 0x001,
        Normal = 0x002,
        Ao_rough_metal = 0x004,
        Emission = 0x008,
        Displacement = 0x010,
        VolumeThickness = 0x020,
        Transmission = 0x040,
        Clearcoat = 0x080,
        SheenColor = 0x100,
        SheenRoughness = 0x200,
        Iridescence = 0x400,
        IridescenceThickness = 0x800,
        Specular = 0x1000,
        SpecularColor = 0x2000,
        Environment = 0x4000
    };

    static MaterialPtr create() { return MaterialPtr(new Material()); };

    [[nodiscard]] std::size_t hash() const;

    std::string name;

    glm::vec4 color = glm::vec4(1);

    glm::vec4 emission = glm::vec4(0, 0, 0, 1);

    float metalness = 0.f;

    float roughness = 1.f;

    float occlusion = 1.f;

    bool two_sided = false;

    //! null-surface (skip surface interaction)
    bool null_surface = false;

    BlendMode blend_mode = BlendMode::Opaque;

    float alpha_cutoff = 0.5f;

    float transmission = 0.f;

    glm::vec3 attenuation_color = glm::vec3(1.f);

    float attenuation_distance = std::numeric_limits<float>::infinity();

    // phase-function asymmetry parameter (forward- vs. back-scattering) [-1, 1]
    float phase_asymmetry_g = 0.f;

    // ratio of scattering vs. absorption (sigma_s / sigma_t)
    float scattering_ratio = 0.f;

    float ior = 1.5f;

    float clearcoat_factor = 0.f;

    float clearcoat_roughness_factor = 0.f;

    glm::vec3 sheen_color = glm::vec3(0.f);

    float sheen_roughness = 0.f;

    // iridescence
    float iridescence_factor = 0.f;
    float iridescence_ior = 1.3f;

    // iridescence thin-film layer given in nanometers (nm)
    glm::vec2 iridescence_thickness_range = {100.f, 400.f};

    bool depth_test = true;

    bool depth_write = true;

    VkCullModeFlagBits cull_mode = VK_CULL_MODE_BACK_BIT;

    std::map<TextureType, vierkant::ImagePtr> textures;

    // optional texture-transform (todo: per image)
    glm::mat4 texture_transform = glm::mat4(1);

private:
    Material() = default;
};

}// namespace vierkant