//
// Created by crocdialer on 7/27/19.
//

#pragma once

#include <crocore/NamedUUID.hpp>

#include "vierkant/Geometry.hpp"
#include "vierkant/Image.hpp"
#include "vierkant/Pipeline.hpp"

namespace vierkant
{

//! define resource-identifiers
DEFINE_NAMED_UUID(MaterialId)
DEFINE_NAMED_UUID(TextureSourceId)
DEFINE_NAMED_UUID(SamplerId)

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

enum class TextureType : uint32_t
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

struct material_t
{
    std::string name;

    glm::vec4 base_color = glm::vec4(1.f);
    glm::vec3 emission;
    float emissive_strength = 1.f;

    float roughness = 1.f;
    float metalness = 0.f;

    //! null-surface (skip surface interaction)
    bool null_surface = false;

    bool twosided = false;

    // transmission
    float ior = 1.5f;
    glm::vec3 attenuation_color = glm::vec3(1.f);

    // volumes
    float transmission = 0.f;
    float attenuation_distance = std::numeric_limits<float>::infinity();

    // idk rasterizer only thingy
    float thickness = 1.f;

    vierkant::BlendMode blend_mode = vierkant::BlendMode::Opaque;
    float alpha_cutoff = 0.5f;

    // specular
    float specular_factor = 1.f;
    glm::vec3 specular_color = glm::vec3(1.f);

    // clearcoat
    float clearcoat_factor = 0.f;
    float clearcoat_roughness_factor = 0.f;

    // sheen
    glm::vec3 sheen_color = glm::vec3(0.f);
    float sheen_roughness = 0.f;

    // iridescence
    float iridescence_factor = 0.f;
    float iridescence_ior = 1.3f;

    // iridescence thin-film layer given in nanometers (nm)
    glm::vec2 iridescence_thickness_range = {100.f, 400.f};

    // optional texture-transform (todo: per image)
    glm::mat4 texture_transform = glm::mat4(1);

    // maps TextureType to a TextureId/SamplerId. sorted in enum order, which is important in other places.
    std::map<vierkant::TextureType, vierkant::TextureSourceId> textures;
    std::map<vierkant::TextureType, vierkant::SamplerId> samplers;
};

struct texture_sampler_t
{
    enum class Filter
    {
        NEAREST = 0,
        LINEAR,
        CUBIC
    };

    enum class AddressMode
    {
        REPEAT = 0,
        MIRRORED_REPEAT,
        CLAMP_TO_EDGE,
        CLAMP_TO_BORDER,
        MIRROR_CLAMP_TO_EDGE,
    };

    AddressMode address_mode_u = AddressMode::REPEAT;
    AddressMode address_mode_v = AddressMode::REPEAT;

    Filter min_filter = Filter::LINEAR;
    Filter mag_filter = Filter::LINEAR;
    glm::mat4 transform = glm::mat4(1);
};

DEFINE_CLASS_PTR(Material)

class Material
{
public:

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