//
// Created by crocdialer on 9/3/21.
//

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-const-variable"
#endif

#define TINYGLTF_IMPLEMENTATION

#include <deque>
#include <tiny_gltf.h>

#include <vierkant/model/gltf.hpp>
#include <vierkant/transform.hpp>

namespace vierkant::model
{

// vertex attributes
constexpr char attrib_position[] = "POSITION";
constexpr char attrib_normal[] = "NORMAL";
constexpr char attrib_tangent[] = "TANGENT";
constexpr char attrib_color[] = "COLOR_0";
constexpr char attrib_texcoord[] = "TEXCOORD_0";
constexpr char attrib_joints[] = "JOINTS_0";
constexpr char attrib_weights[] = "WEIGHTS_0";

// animation targets
constexpr char animation_target_translation[] = "translation";
constexpr char animation_target_rotation[] = "rotation";
constexpr char animation_target_scale[] = "scale";
constexpr char animation_target_weights[] = "weights";

// blend modes
[[maybe_unused]] constexpr char blend_mode_opaque[] = "OPAQUE";
constexpr char blend_mode_blend[] = "BLEND";
constexpr char blend_mode_mask[] = "MASK";

// interpolation modes
[[maybe_unused]] constexpr char interpolation_linear[] = "LINEAR";
constexpr char interpolation_step[] = "STEP";
constexpr char interpolation_cubic_spline[] = "CUBICSPLINE";

// cameras
constexpr char camera_type_perspective[] = "perspective";
constexpr char camera_type_orthographic[] = "orthographic";

// extensions
constexpr char KHR_materials_emissive_strength[] = "KHR_materials_emissive_strength";
constexpr char KHR_materials_specular[] = "KHR_materials_specular";
constexpr char KHR_materials_transmission[] = "KHR_materials_transmission";
constexpr char KHR_materials_volume[] = "KHR_materials_volume";
constexpr char KHR_materials_ior[] = "KHR_materials_ior";
constexpr char KHR_materials_clearcoat[] = "KHR_materials_clearcoat";
constexpr char KHR_materials_sheen[] = "KHR_materials_sheen";
constexpr char KHR_materials_iridescence[] = "KHR_materials_iridescence";
constexpr char KHR_texture_transform[] = "KHR_texture_transform";
constexpr char KHR_lights_punctual[] = "KHR_lights_punctual";

// KHR_materials_emissive_strength
constexpr char ext_emissive_strength[] = "emissiveStrength";

// KHR_materials_specular
constexpr char ext_specular_factor[] = "specularFactor";
constexpr char ext_specular_color_factor[] = "specularColorFactor";
constexpr char ext_specular_texture[] = "specularTexture";
constexpr char ext_specular_color_texture[] = "specularColorTexture";

// KHR_materials_transmission
constexpr char ext_transmission_factor[] = "transmissionFactor";
constexpr char ext_transmission_texture[] = "transmissionTexture";
constexpr char ext_volume_thickness_factor[] = "thicknessFactor";
constexpr char ext_volume_thickness_texture[] = "thicknessTexture";

// KHR_materials_volume
constexpr char ext_volume_attenuation_distance[] = "attenuationDistance";
constexpr char ext_volume_attenuation_color[] = "attenuationColor";

// KHR_materials_clearcoat
constexpr char ext_clearcoat_factor[] = "clearcoatFactor";
constexpr char ext_clearcoat_roughness_factor[] = "clearcoatRoughnessFactor";

// KHR_materials_sheen
constexpr char ext_sheen_color_factor[] = "sheenColorFactor";
constexpr char ext_sheen_color_texture[] = "sheenColorTexture";
constexpr char ext_sheen_roughness_factor[] = "sheenRoughnessFactor";
constexpr char ext_sheen_roughness_texture[] = "sheenRoughnessTexture";

// KHR_materials_iridescence
constexpr char ext_iridescence_factor[] = "iridescenceFactor";
constexpr char ext_iridescence_texture[] = "iridescenceTexture";
constexpr char ext_iridescence_ior[] = "iridescenceIOR";
constexpr char ext_iridescence_thickness_min[] = "iridescenceThicknessMinimum";
constexpr char ext_iridescence_thickness_max[] = "iridescenceThicknessMaximum";
constexpr char ext_iridescence_thickness_texture[] = "iridescenceThicknessTexture";

// KHR_texture_transform
constexpr char ext_texture_offset[] = "offset";
constexpr char ext_texture_rotation[] = "rotation";
constexpr char ext_texture_scale[] = "scale";
[[maybe_unused]] constexpr char ext_texture_tex_coord[] = "texCoord";

// KHR_lights_punctual
constexpr char ext_light[] = "light";
[[maybe_unused]] constexpr char ext_light_point[] = "point";
constexpr char ext_light_spot[] = "spot";
constexpr char ext_light_directional[] = "directional";

struct node_helper_t
{
    size_t index;
    vierkant::transform_t world_transform;
    vierkant::nodes::NodePtr node;
};

using node_map_t = std::unordered_map<uint32_t, vierkant::nodes::NodePtr>;

using joint_map_t = std::unordered_map<uint32_t, uint32_t>;

vierkant::transform_t node_transform(const tinygltf::Node &tiny_node)
{
    if(tiny_node.matrix.size() == 16)
    {
        return vierkant::transform_cast(*reinterpret_cast<const glm::dmat4 *>(tiny_node.matrix.data()));
    }
    vierkant::transform_t ret;

    if(tiny_node.translation.size() == 3)
    {
        ret.translation = glm::dvec3(tiny_node.translation[0], tiny_node.translation[1], tiny_node.translation[2]);
    }

    if(tiny_node.scale.size() == 3)
    {
        ret.scale = glm::dvec3(tiny_node.scale[0], tiny_node.scale[1], tiny_node.scale[2]);
    }

    if(tiny_node.rotation.size() == 4)
    {
        ret.rotation =
                glm::dquat(tiny_node.rotation[3], tiny_node.rotation[0], tiny_node.rotation[1], tiny_node.rotation[2]);
    }
    return ret;
}

glm::mat4 texture_transform(const tinygltf::TextureInfo &texture_info)
{
    glm::mat4 ret(1);
    auto ext_transform_it = texture_info.extensions.find(KHR_texture_transform);

    if(ext_transform_it != texture_info.extensions.end())
    {
        // extract offset, rotation, scale
        const auto &value = ext_transform_it->second;

        glm::vec2 offset = {0.f, 0.f};
        float rotation = 0.f;
        glm::vec2 scale = {1.f, 1.f};

        if(value.Has(ext_texture_offset))
        {
            const auto &offset_value = value.Get(ext_texture_offset);
            offset = glm::dvec2(offset_value.Get(0).GetNumberAsDouble(), offset_value.Get(1).GetNumberAsDouble());
        }
        if(value.Has(ext_texture_rotation))
        {
            const auto &rotation_value = value.Get(ext_texture_rotation);
            rotation = static_cast<float>(rotation_value.GetNumberAsDouble());
        }
        if(value.Has(ext_texture_scale))
        {
            const auto &scale_value = value.Get(ext_texture_scale);
            scale = glm::dvec2(scale_value.Get(0).GetNumberAsDouble(), scale_value.Get(1).GetNumberAsDouble());
        }
        ret = glm::translate(glm::mat4(1), glm::vec3(offset, 0.f)) *
              glm::rotate(glm::mat4(1), rotation, glm::vec3(0.f, 0.f, -1.f)) *
              glm::scale(glm::mat4(1), glm::vec3(scale, 1.f));
    }
    return ret;
}

vierkant::GeometryPtr create_geometry(const tinygltf::Primitive &primitive, const tinygltf::Model &model,
                                      const std::map<std::string, int> &attributes, bool morph_target)
{
    auto geometry = vierkant::Geometry::create();

    // extract indices
    if(!morph_target && primitive.indices >= 0 && static_cast<size_t>(primitive.indices) < model.accessors.size())
    {
        tinygltf::Accessor index_accessor = model.accessors[primitive.indices];
        const auto &buffer_view = model.bufferViews[index_accessor.bufferView];
        const auto &buffer = model.buffers[buffer_view.buffer];

        if(buffer_view.target == 0) { spdlog::warn("bufferView.target is zero"); }

        assert(index_accessor.type == TINYGLTF_TYPE_SCALAR);

        auto data =
                static_cast<const uint8_t *>(buffer.data.data() + index_accessor.byteOffset + buffer_view.byteOffset);


        if(index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
        {
            const auto *ptr = reinterpret_cast<const uint16_t *>(data);
            geometry->indices = {ptr, ptr + index_accessor.count};
        }
        else if(index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
        {
            const auto *ptr = reinterpret_cast<const uint8_t *>(data);
            geometry->indices = {ptr, ptr + index_accessor.count};
        }
        else if(index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
        {
            const auto *ptr = reinterpret_cast<const uint32_t *>(data);
            geometry->indices = {ptr, ptr + index_accessor.count};
        }
        else { spdlog::error("unsupported index-type: {}", index_accessor.componentType); }
    }

    for(const auto &[attrib, accessor_idx]: attributes)
    {
        tinygltf::Accessor accessor = model.accessors[accessor_idx];
        const auto &buffer_view = model.bufferViews[accessor.bufferView];
        const auto &buffer = model.buffers[buffer_view.buffer];

        if(accessor.sparse.isSparse) { assert(false); }

        auto insert = [&accessor, &buffer_view](const tinygltf::Buffer &input, auto &array) {
            using elem_t = typename std::decay<decltype(array)>::type::value_type;
            constexpr size_t elem_size = sizeof(elem_t);

            // data with offset
            const uint8_t *data = input.data.data() + buffer_view.byteOffset + accessor.byteOffset;
            uint32_t stride = accessor.ByteStride(buffer_view);

            if(stride == elem_size)
            {
                const auto *ptr = reinterpret_cast<const elem_t *>(data);
                auto end = ptr + accessor.count;
                array = {ptr, end};
            }
            else
            {
                auto ptr = data;
                auto end = data + accessor.count * stride;

                // prealloc
                array.resize(accessor.count);
                auto dst = reinterpret_cast<uint8_t *>(array.data());

                // stride copy
                for(; ptr < end; ptr += stride, dst += elem_size) { std::memcpy(dst, ptr, elem_size); }
            }
        };

        if(attrib == attrib_position) { insert(buffer, geometry->positions); }
        else if(attrib == attrib_normal) { insert(buffer, geometry->normals); }
        else if(attrib == attrib_tangent) { insert(buffer, geometry->tangents); }
        else if(attrib == attrib_color) { insert(buffer, geometry->colors); }
        else if(attrib == attrib_texcoord) { insert(buffer, geometry->tex_coords); }
        else if(attrib == attrib_joints)
        {
            assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
            assert(accessor.type == TINYGLTF_TYPE_VEC4);
            insert(buffer, geometry->bone_indices);
        }
        else if(attrib == attrib_weights)
        {
            assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
            assert(accessor.type == TINYGLTF_TYPE_VEC4);
            insert(buffer, geometry->bone_weights);
        }
    }// for all attributes

    if(!morph_target)
    {
        if(geometry->indices.empty())
        {
            geometry->indices.resize(geometry->positions.size());
            std::iota(geometry->indices.begin(), geometry->indices.end(), 0);
        }
        if(geometry->normals.empty()) { geometry->compute_vertex_normals(); }
        if(geometry->tangents.empty() && !geometry->tex_coords.empty()) { geometry->compute_tangents(); }
    }

    // last resort is to fill with zeros here
    geometry->tex_coords.resize(geometry->positions.size(), glm::vec2(0.f));
    geometry->normals.resize(geometry->positions.size(), glm::vec3(0.f));
    geometry->tangents.resize(geometry->positions.size(), glm::vec3(0.f));

    return geometry;
}

vierkant::material_t convert_material(const tinygltf::Material &tiny_mat, const tinygltf::Model &model,
                                      const std::map<uint32_t, crocore::ImagePtr> &image_cache,
                                      const std::unordered_map<uint32_t, TextureSourceId> &tex_id_cache,
                                      const std::unordered_map<uint32_t, SamplerId> &sampler_id_cache)
{
    vierkant::material_t ret;
    ret.name = tiny_mat.name;
    ret.base_color = *reinterpret_cast<const glm::dvec4 *>(tiny_mat.pbrMetallicRoughness.baseColorFactor.data());
    ret.emission = *reinterpret_cast<const glm::dvec3 *>(tiny_mat.emissiveFactor.data());

    // blend_mode defaults to opaque
    if(tiny_mat.alphaMode == blend_mode_blend) { ret.blend_mode = vierkant::BlendMode::Blend; }
    else if(tiny_mat.alphaMode == blend_mode_mask) { ret.blend_mode = vierkant::BlendMode::Mask; }
    ret.alpha_cutoff = static_cast<float>(tiny_mat.alphaCutoff);

    ret.metalness = static_cast<float>(tiny_mat.pbrMetallicRoughness.metallicFactor);
    ret.roughness = static_cast<float>(tiny_mat.pbrMetallicRoughness.roughnessFactor);
    ret.twosided = tiny_mat.doubleSided;

    auto insert_texture = [&](int tex_index, vierkant::TextureType tex_type) -> bool {
        if(tex_index >= 0)
        {
            int img_index = model.textures[tex_index].source;
            assert(tex_id_cache.contains(img_index));
            ret.textures[tex_type] = tex_id_cache.at(img_index);

            int sampler_index = model.textures[tex_index].sampler;
            auto sampler_id_it = sampler_id_cache.find(sampler_index);

            if(sampler_index >= 0 && sampler_id_it != sampler_id_cache.end())
            {
                ret.samplers[tex_type] = sampler_id_it->second;
            }
            return true;
        }
        return false;
    };

    // albedo
    if(insert_texture(tiny_mat.pbrMetallicRoughness.baseColorTexture.index, TextureType::Color))
    {
        ret.texture_transform = texture_transform(tiny_mat.pbrMetallicRoughness.baseColorTexture);
    }

    // ao / rough / metal
    insert_texture(tiny_mat.pbrMetallicRoughness.metallicRoughnessTexture.index, TextureType::Ao_rough_metal);

    // normals
    insert_texture(tiny_mat.normalTexture.index, TextureType::Normal);

    // emission
    if(insert_texture(tiny_mat.emissiveTexture.index, TextureType::Emission)) { ret.emission = glm::vec3(0.f); }

    // occlusion only supported alongside rough/metal
    if(ret.textures.contains(TextureType::Ao_rough_metal) && tiny_mat.occlusionTexture.index >= 0)
    {
        if(tiny_mat.occlusionTexture.index != tiny_mat.pbrMetallicRoughness.metallicRoughnessTexture.index)
        {
            // occlusion is provided as separate image -> combine with rough/metal
            auto occlusion_image = image_cache.at(model.textures[tiny_mat.occlusionTexture.index].source);
            auto ao_roughness_metal_image =
                    image_cache.at(model.textures[tiny_mat.pbrMetallicRoughness.metallicRoughnessTexture.index].source);

            // there was texture data for AO in a separate map -> combine

            if(occlusion_image->width() != ao_roughness_metal_image->width() ||
               occlusion_image->height() != ao_roughness_metal_image->height())
            {
                occlusion_image =
                        occlusion_image->resize(ao_roughness_metal_image->width(), ao_roughness_metal_image->height());
            }

            auto *src = static_cast<uint8_t *>(occlusion_image->data());

            constexpr size_t ao_offset = 0;
            auto dst = (uint8_t *) ao_roughness_metal_image->data();
            auto end = dst + ao_roughness_metal_image->num_bytes();

            for(; dst < end;)
            {
                dst[ao_offset] = src[ao_offset];
                dst += ao_roughness_metal_image->num_components();
                src += occlusion_image->num_components();
            }
        }
    }
    else if(ret.textures.contains(TextureType::Ao_rough_metal))
    {
        auto ao_roughness_metal_image =
                image_cache.at(model.textures[tiny_mat.pbrMetallicRoughness.metallicRoughnessTexture.index].source);

        // rough/metal was provided but no occlusion -> pad with 1.0
        constexpr size_t ao_offset = 0;
        auto dst = (uint8_t *) ao_roughness_metal_image->data(), end = dst + ao_roughness_metal_image->num_bytes();
        for(; dst < end; dst += ao_roughness_metal_image->num_components()) { dst[ao_offset] = 255; }
    }

    for(const auto &[ext, value]: tiny_mat.extensions)
    {
        spdlog::trace("ext-properties: {}", value.Keys());

        if(ext == KHR_materials_emissive_strength)
        {
            const auto &emissive_strength_value = value.Get(ext_emissive_strength);
            ret.emissive_strength = static_cast<float>(emissive_strength_value.GetNumberAsDouble());
        }
        else if(ext == KHR_materials_specular)
        {
            if(value.Has(ext_specular_factor))
            {
                const auto &specular_factor_value = value.Get(ext_specular_factor);
                ret.specular_factor = static_cast<float>(specular_factor_value.GetNumberAsDouble());
            }

            if(value.Has(ext_specular_color_factor))
            {
                const auto &specular_color_value = value.Get(ext_specular_color_factor);
                ret.specular_color = glm::dvec3(specular_color_value.Get(0).GetNumberAsDouble(),
                                                specular_color_value.Get(1).GetNumberAsDouble(),
                                                specular_color_value.Get(2).GetNumberAsDouble());
            }

            if(value.Has(ext_specular_texture))
            {
                const auto &specular_texture_value = value.Get(ext_specular_texture);
                insert_texture(specular_texture_value.Get("index").GetNumberAsInt(), TextureType::Specular);
            }

            if(value.Has(ext_specular_color_texture))
            {
                const auto &specular_color_texture_value = value.Get(ext_specular_color_texture);
                insert_texture(specular_color_texture_value.Get("index").GetNumberAsInt(), TextureType::SpecularColor);
            }
        }
        else if(ext == KHR_materials_transmission)
        {
            if(value.Has(ext_transmission_factor))
            {
                ret.transmission = static_cast<float>(value.Get(ext_transmission_factor).GetNumberAsDouble());
            }

            if(value.Has(ext_transmission_texture))
            {
                const auto &transmission_texture_value = value.Get(ext_transmission_texture);
                insert_texture(transmission_texture_value.Get("index").GetNumberAsInt(), TextureType::Transmission);
            }
        }
        else if(ext == KHR_materials_volume)
        {
            if(value.Has(ext_volume_thickness_factor))
            {
                const auto &thickness_value = value.Get(ext_volume_thickness_factor);
                ret.thickness = static_cast<float>(thickness_value.GetNumberAsDouble());
            }

            if(value.Has(ext_volume_attenuation_color))
            {
                const auto &attenuation_value = value.Get(ext_volume_attenuation_color);
                ret.attenuation_color = glm::dvec3(attenuation_value.Get(0).GetNumberAsDouble(),
                                                   attenuation_value.Get(1).GetNumberAsDouble(),
                                                   attenuation_value.Get(2).GetNumberAsDouble());
            }

            if(value.Has(ext_volume_attenuation_distance))
            {
                const auto &attenuation_distance_value = value.Get(ext_volume_attenuation_distance);
                ret.attenuation_distance = static_cast<float>(attenuation_distance_value.GetNumberAsDouble());
            }
            if(value.Has(ext_volume_thickness_texture))
            {
                const auto &thickness_texture_value = value.Get(ext_volume_thickness_texture);
                insert_texture(thickness_texture_value.Get("index").GetNumberAsInt(), TextureType::VolumeThickness);
            }
        }
        else if(ext == KHR_materials_ior)
        {
            if(value.Has("ior")) { ret.ior = static_cast<float>(value.Get("ior").GetNumberAsDouble()); }
        }
        else if(ext == KHR_materials_clearcoat)
        {
            if(value.Has(ext_clearcoat_factor))
            {
                ret.clearcoat_factor = static_cast<float>(value.Get(ext_clearcoat_factor).GetNumberAsDouble());
            }
            if(value.Has(ext_clearcoat_roughness_factor))
            {
                ret.clearcoat_roughness_factor =
                        static_cast<float>(value.Get(ext_clearcoat_roughness_factor).GetNumberAsDouble());
            }
        }
        else if(ext == KHR_materials_sheen)
        {
            if(value.Has(ext_sheen_color_factor))
            {
                const auto &sheen_value = value.Get(ext_sheen_color_factor);
                ret.sheen_color =
                        glm::dvec3(sheen_value.Get(0).GetNumberAsDouble(), sheen_value.Get(1).GetNumberAsDouble(),
                                   sheen_value.Get(2).GetNumberAsDouble());
            }
            if(value.Has(ext_sheen_roughness_factor))
            {
                ret.sheen_roughness = static_cast<float>(value.Get(ext_sheen_roughness_factor).GetNumberAsDouble());
            }
            if(value.Has(ext_sheen_color_texture))
            {
                const auto &sheen_color_texture_value = value.Get(ext_sheen_color_texture);
                insert_texture(sheen_color_texture_value.Get("index").GetNumberAsInt(), TextureType::SheenColor);
            }
            if(value.Has(ext_sheen_roughness_texture))
            {
                const auto &sheen_roughness_texture_value = value.Get(ext_sheen_roughness_texture);
                insert_texture(sheen_roughness_texture_value.Get("index").GetNumberAsInt(),
                               TextureType::SheenRoughness);
            }
        }
        else if(ext == KHR_materials_iridescence)
        {
            if(value.Has(ext_iridescence_factor))
            {
                ret.iridescence_factor = static_cast<float>(value.Get(ext_iridescence_factor).GetNumberAsDouble());
            }
            if(value.Has(ext_iridescence_texture))
            {
                const auto &iridescence_texture_value = value.Get(ext_iridescence_texture);
                auto img_index = model.textures[iridescence_texture_value.Get("index").GetNumberAsInt()].source;
                assert(tex_id_cache.contains(img_index));
                ret.textures[TextureType::Iridescence] = tex_id_cache.at(img_index);
            }
            if(value.Has(ext_iridescence_ior))
            {
                ret.iridescence_ior = static_cast<float>(value.Get(ext_iridescence_ior).GetNumberAsDouble());
            }
            if(value.Has(ext_iridescence_thickness_min))
            {
                ret.iridescence_thickness_range.x =
                        static_cast<float>(value.Get(ext_iridescence_thickness_min).GetNumberAsDouble());
            }
            if(value.Has(ext_iridescence_thickness_max))
            {
                ret.iridescence_thickness_range.y =
                        static_cast<float>(value.Get(ext_iridescence_thickness_max).GetNumberAsDouble());
            }
            if(value.Has(ext_iridescence_thickness_texture))
            {
                const auto &iridescence_thickness_texture_value = value.Get(ext_iridescence_thickness_texture);
                auto img_index =
                        model.textures[iridescence_thickness_texture_value.Get("index").GetNumberAsInt()].source;
                assert(tex_id_cache.contains(img_index));
                ret.textures[TextureType::IridescenceThickness] = tex_id_cache.at(img_index);
                assert(image_cache.contains(img_index));
                auto img_iridescence_thickness = image_cache.at(img_index);

                if(!ret.textures.contains(TextureType::Iridescence))
                {
                    ret.textures[TextureType::Iridescence] = tex_id_cache.at(img_index);

                    if(auto img = std::dynamic_pointer_cast<crocore::Image_<uint8_t>>(img_iridescence_thickness))
                    {
                        auto ptr = img->at(0, 0);
                        auto end = ptr + img->num_bytes();
                        uint32_t c = img->num_components();

                        // iridescence-factor -> 1.0
                        for(; ptr < end; ptr += c) { ptr[0] = 255; }
                    }
                }//} || img_iridescence_thickness == ret.img_iridescence);
            }
        }
    }
    return ret;
}

vierkant::texture_sampler_t convert_sampler(const tinygltf::Sampler &tiny_sampler)
{
    vierkant::texture_sampler_t ret = {};

    using Filter = texture_sampler_t::Filter;
    auto convert_tiny_filter = [](int tf) -> Filter {
        switch(tf)
        {
            case TINYGLTF_TEXTURE_FILTER_NEAREST: return Filter::NEAREST;
            case TINYGLTF_TEXTURE_FILTER_LINEAR:
            default: return Filter::LINEAR;
        }
    };
    ret.min_filter = convert_tiny_filter(tiny_sampler.minFilter);
    ret.mag_filter = convert_tiny_filter(tiny_sampler.magFilter);

    using AddressMode = texture_sampler_t::AddressMode;
    auto convert_tiny_wrap = [](int wrap) -> AddressMode {
        switch(wrap)
        {
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE: return AddressMode::CLAMP_TO_EDGE;
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT: return AddressMode::MIRRORED_REPEAT;
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
            default: return AddressMode::REPEAT;
        }
    };
    ret.address_mode_u = convert_tiny_wrap(tiny_sampler.wrapS);
    ret.address_mode_v = convert_tiny_wrap(tiny_sampler.wrapT);
    return ret;
}

vierkant::nodes::NodePtr create_bone_hierarchy_bfs(const tinygltf::Skin &skin, const tinygltf::Model &model,
                                                   node_map_t &node_map)
{
    vierkant::nodes::NodePtr root_bone;

    // optional vertex skinning
    std::vector<glm::mat4> inverse_binding_matrices;

    joint_map_t joint_map;
    for(uint32_t i = 0; i < skin.joints.size(); ++i) { joint_map[skin.joints[i]] = i; }

    if(skin.inverseBindMatrices >= 0 && static_cast<uint32_t>(skin.inverseBindMatrices) < model.accessors.size())
    {
        tinygltf::Accessor bind_matrix_accessor = model.accessors[skin.inverseBindMatrices];

        assert(bind_matrix_accessor.type == TINYGLTF_TYPE_MAT4);
        assert(bind_matrix_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

        const auto &buffer_view = model.bufferViews[bind_matrix_accessor.bufferView];
        const auto &buffer = model.buffers[buffer_view.buffer];
        assert(bind_matrix_accessor.ByteStride(buffer_view) == sizeof(glm::mat4));

        auto data = reinterpret_cast<const glm::mat4 *>(buffer.data.data() + bind_matrix_accessor.byteOffset +
                                                        buffer_view.byteOffset);
        inverse_binding_matrices = {data, data + bind_matrix_accessor.count};
    }

    if(skin.skeleton >= 0 && static_cast<uint32_t>(skin.skeleton) < model.nodes.size() &&
       !inverse_binding_matrices.empty())
    {
        std::deque<node_helper_t> node_queue;
        node_queue.push_back({static_cast<size_t>(skin.skeleton), {}, nullptr});

        while(!node_queue.empty())
        {
            auto [current_index, world_transform, parent_node] = std::move(node_queue.front());
            node_queue.pop_front();

            const tinygltf::Node &skeleton_node = model.nodes[current_index];
            auto local_joint_transform = node_transform(skeleton_node);
            world_transform = world_transform * local_joint_transform;

            auto bone_node = std::make_shared<vierkant::nodes::node_t>();
            bone_node->parent = parent_node;
            bone_node->name = skeleton_node.name;
            bone_node->index = joint_map[current_index];
            bone_node->offset = vierkant::transform_cast(inverse_binding_matrices[joint_map[current_index]]);
            bone_node->transform = local_joint_transform;

            node_map[current_index] = bone_node;

            if(!root_bone) { root_bone = bone_node; }
            if(parent_node) { parent_node->children.push_back(bone_node); }

            for(auto child_index: skeleton_node.children)
            {
                node_queue.push_back({static_cast<size_t>(child_index), world_transform, bone_node});
            }
        }
    }
    return root_bone;
}

vierkant::nodes::node_animation_t create_node_animation(const tinygltf::Animation &tiny_animation,
                                                        const tinygltf::Model &model, const node_map_t &node_map)
{
    spdlog::debug("animation: {}", tiny_animation.name);

    vierkant::nodes::node_animation_t animation;
    animation.name = tiny_animation.name;

    for(const auto &channel: tiny_animation.channels)
    {
        auto it = node_map.find(channel.target_node);

        std::unordered_map<uint32_t, std::vector<float>> input_samplers;

        if(it != node_map.end())
        {
            const auto &node_ptr = it->second;
            animation_keys_t &animation_keys = animation.keys[node_ptr];

            const auto &sampler = tiny_animation.samplers[channel.sampler];

            // create or retrieve input times
            auto &input_times = input_samplers[sampler.input];

            if(input_times.empty())
            {
                const auto &accessor = model.accessors[sampler.input];
                const auto &buffer_view = model.bufferViews[accessor.bufferView];
                const auto &buffer = model.buffers[buffer_view.buffer];
                assert(accessor.type == TINYGLTF_TYPE_SCALAR);
                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                auto data = buffer.data.data() + accessor.byteOffset + buffer_view.byteOffset;
                auto ptr = reinterpret_cast<const float *>(data);
                input_times = {ptr, ptr + accessor.count};
                animation.duration =
                        std::max(animation.duration, *std::max_element(input_times.begin(), input_times.end()));

                animation.interpolation_mode = vierkant::InterpolationMode::Linear;

                if(sampler.interpolation == interpolation_step)
                {
                    animation.interpolation_mode = vierkant::InterpolationMode::Step;
                }
                else if(sampler.interpolation == interpolation_cubic_spline)
                {
                    animation.interpolation_mode = vierkant::InterpolationMode::CubicSpline;
                }
            }

            const auto &accessor = model.accessors[sampler.output];
            const auto &buffer_view = model.bufferViews[accessor.bufferView];
            const auto &buffer = model.buffers[buffer_view.buffer];
            auto data = buffer.data.data() + accessor.byteOffset + buffer_view.byteOffset;

            // number of elements per time-point
            bool is_cubic_spline = animation.interpolation_mode == vierkant::InterpolationMode::CubicSpline;
            size_t num_elems = is_cubic_spline ? 3 : 1;

            if(channel.target_path == animation_target_translation)
            {
                assert(accessor.type == TINYGLTF_TYPE_VEC3);
                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                auto ptr = reinterpret_cast<const glm::vec3 *>(data);

                for(float t: input_times)
                {
                    vierkant::animation_value_t<glm::vec3> animation_value = {};
                    animation_value.value = ptr[0];
                    if(is_cubic_spline) { animation_value = {ptr[1], ptr[0], ptr[2]}; }
                    animation_keys.positions.insert({t, animation_value});
                    ptr += num_elems;
                }
            }
            else if(channel.target_path == animation_target_rotation)
            {
                assert(accessor.type == TINYGLTF_TYPE_VEC4);
                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                auto ptr = reinterpret_cast<const float *>(data);

                for(float t: input_times)
                {
                    auto q0 = glm::quat(ptr[3], ptr[0], ptr[1], ptr[2]);
                    vierkant::animation_value_t<glm::quat> animation_value = {};
                    animation_value.value = q0;

                    if(is_cubic_spline)
                    {
                        auto q1 = glm::quat(ptr[7], ptr[4], ptr[5], ptr[6]);
                        auto q2 = glm::quat(ptr[11], ptr[8], ptr[9], ptr[10]);
                        animation_value = {q1, q0, q2};
                    }
                    animation_keys.rotations.insert({t, animation_value});
                    ptr += 4 * num_elems;
                }
            }
            else if(channel.target_path == animation_target_scale)
            {
                assert(accessor.type == TINYGLTF_TYPE_VEC3);
                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                auto ptr = reinterpret_cast<const glm::vec3 *>(data);

                for(float t: input_times)
                {
                    vierkant::animation_value_t<glm::vec3> animation_value = {};
                    animation_value.value = ptr[0];
                    if(is_cubic_spline) { animation_value = {ptr[1], ptr[0], ptr[2]}; }
                    animation_keys.scales.insert({t, animation_value});
                    ptr += num_elems;
                }
            }
            else if(channel.target_path == animation_target_weights)
            {
                assert(accessor.type == TINYGLTF_TYPE_SCALAR);
                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                auto ptr = reinterpret_cast<const float *>(data);
                const uint32_t num_weights = accessor.count / input_times.size();

                for(float t: input_times)
                {
                    vierkant::animation_value_t<std::vector<double>> animation_value = {};
                    animation_value.value = {ptr, ptr + num_weights};

                    if(is_cubic_spline)
                    {
                        animation_value.in_tangent = {ptr, ptr + num_weights};
                        animation_value.value = {ptr + num_weights, ptr + 2 * num_weights};
                        animation_value.out_tangent = {ptr + 2 * num_weights, ptr + 3 * num_weights};
                    }
                    else
                    {
                        animation_value.in_tangent.resize(animation_value.value.size(), 0.);
                        animation_value.out_tangent.resize(animation_value.value.size(), 0.);
                    }
                    animation_keys.morph_weights.insert({t, animation_value});
                    ptr += num_elems * num_weights;
                }
            }
        }
    }
    return animation;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct load_image_context_t
{
    // cache image-futures
    std::map<uint32_t, std::future<crocore::ImagePtr>> image_cache;

    crocore::ThreadPool *pool = nullptr;
};

bool LoadImageDataFunction(tinygltf::Image * /*tiny_image*/, const int image_idx, std::string * /*err*/,
                           std::string * /*warn*/, int /*req_width*/, int /*req_height*/, const unsigned char *bytes,
                           int size, void *user_data)
{
    assert(user_data && bytes && size);
    auto &img_context = *reinterpret_cast<load_image_context_t *>(user_data);
    assert(img_context.pool);

    // create and cache image
    std::vector<unsigned char> data(bytes, bytes + size);
    img_context.image_cache[image_idx] = img_context.pool->post([data = std::move(data)]() -> crocore::ImagePtr {
        try
        {
            return crocore::create_image_from_data(data, 4);
        } catch(std::exception &e)
        {
            spdlog::error(e.what());
            return nullptr;
        }
    });
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::optional<mesh_assets_t> gltf(const std::filesystem::path &path, crocore::ThreadPool *const pool)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    load_image_context_t img_context;
    img_context.pool = pool;

    if(pool) { loader.SetImageLoader(&LoadImageDataFunction, &img_context); }

    bool ret = false;
    auto ext_str = path.extension().string();
    std::transform(ext_str.begin(), ext_str.end(), ext_str.begin(), ::tolower);
    if(ext_str == ".gltf") { ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string()); }
    else if(ext_str == ".glb") { ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.string()); }
    if(!warn.empty()) { spdlog::warn(warn); }
    if(!err.empty()) { spdlog::error(err); }
    if(!ret) { return {}; }

    if(!model.extensionsUsed.empty()) { spdlog::debug("model using extensions: {}", model.extensionsUsed); }

    const tinygltf::Scene &scene = model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];

    // create vierkant::node root
    vierkant::model::mesh_assets_t out_assets = {};
    out_assets.root_node = std::make_shared<vierkant::nodes::node_t>();
    out_assets.root_node->name = model.nodes[scene.nodes[0]].name;

    auto &entry_create_infos = std::get<std::vector<vierkant::Mesh::entry_create_info_t>>(out_assets.geometry_data);
    //    std::vector<vierkant::Mesh::entry_create_info_t> entry_create_infos;

    // map tiny-indices to created assets
    std::map<uint32_t, crocore::ImagePtr> image_cache;
    std::map<uint32_t, vierkant::texture_sampler_t> sampler_cache;

    if(pool)
    {
        // wait for image-futures, check for nullptr
        for(auto &[idx, img_future]: img_context.image_cache)
        {
            auto img = img_future.get();

            // fail to load image, bail out
            if(!img) { return {}; }
            image_cache[idx] = std::move(img);
        }
    }
    else
    {
        for(const auto &t: model.textures)
        {
            auto &tiny_image = model.images[t.source];
            if(!image_cache.contains(t.source))
            {
                image_cache[t.source] = crocore::Image_<uint8_t>::create(tiny_image.image.data(), tiny_image.width,
                                                                         tiny_image.height, tiny_image.component);
            }
        }
    }

    // cache sampler under image-index (not correct way, but let's see)
    for(const auto &t: model.textures)
    {
        if(t.sampler >= 0 && static_cast<uint32_t>(t.sampler) < model.samplers.size())
        {
            const auto &sampler = model.samplers[t.sampler];
            if(!sampler_cache.contains(t.sampler)) { sampler_cache[t.sampler] = convert_sampler(sampler); }
        }
    }

    // populate out-textures, generate UUIDs-handles
    std::unordered_map<uint32_t, TextureSourceId> tex_id_cache;
    for(const auto &[index, img]: image_cache)
    {
        auto texture_id = TextureSourceId::random();
        tex_id_cache[index] = texture_id;
        out_assets.textures[texture_id] = img;
    }

    // populate out-samplers, generate UUIDs-handles
    std::unordered_map<uint32_t, SamplerId> sampler_id_cache;
    for(const auto &[index, sampler]: sampler_cache)
    {
        auto sampler_id = SamplerId ::random();
        sampler_id_cache[index] = sampler_id;
        out_assets.texture_samplers[sampler_id] = sampler;
    }

    // create materials
    for(const auto &tiny_mat: model.materials)
    {
        try
        {
            auto m = convert_material(tiny_mat, model, image_cache, tex_id_cache, sampler_id_cache);
            out_assets.material_ids.push_back(m.id);
            out_assets.materials[m.id] = m;
        } catch(std::exception &e)
        {
            spdlog::warn("could not convert material '{}' for: '{}' ({})", tiny_mat.name, path.string(), e.what());
//            out_assets.materials[new_id] = {};
//            out_assets.material_ids.push_back(new_id);
        }
    }

    // create lights
    for(const auto &tiny_light: model.lights)
    {
        lightsource_t l = {};

        if(tiny_light.type == ext_light_spot) { l.type = LightType::Spot; }
        else if(tiny_light.type == ext_light_directional) { l.type = LightType::Directional; }

        if(tiny_light.color.size() == 3) { l.color = {tiny_light.color[0], tiny_light.color[1], tiny_light.color[2]}; }
        l.intensity = static_cast<float>(tiny_light.intensity);
        l.range = static_cast<float>(tiny_light.range);
        l.inner_cone_angle = static_cast<float>(tiny_light.spot.innerConeAngle);
        l.outer_cone_angle = static_cast<float>(tiny_light.spot.outerConeAngle);

        out_assets.lights.push_back(l);
    }

    std::deque<node_helper_t> node_queue;

    for(int node_index: scene.nodes)
    {
        node_queue.push_back({static_cast<size_t>(node_index), {}, out_assets.root_node});
    }

    node_map_t node_map;

    // cache geometries (index_accessor, attributes) -> geometry
    using geometry_key = std::tuple<int, const std::map<std::string, int> &>;
    std::map<geometry_key, vierkant::GeometryPtr> geometry_cache;

    auto get_geometry = [&geometry_cache, &model](const tinygltf::Primitive &primitive,
                                                  const std::map<std::string, int> &attributes,
                                                  bool morph_target = false) {
        vierkant::GeometryPtr geometry;
        geometry_key geom_key = {primitive.indices, attributes};
        auto it = geometry_cache.find(geom_key);
        if(!morph_target && it != geometry_cache.end()) { geometry = it->second; }
        else
        {
            geometry = create_geometry(primitive, model, attributes, morph_target);
            geometry_cache[geom_key] = geometry;
        }
        return geometry;
    };

    while(!node_queue.empty())
    {
        auto [current_index, world_transform, parent_node] = node_queue.front();
        node_queue.pop_front();

        const tinygltf::Node &tiny_node = model.nodes[current_index];
        auto local_transform = node_transform(tiny_node);

        // create vierkant::node
        auto current_node = std::make_shared<vierkant::nodes::node_t>();
        current_node->name = tiny_node.name;
        current_node->index = current_index;
        current_node->parent = parent_node;
        current_node->transform = local_transform;

        assert(parent_node);
        parent_node->children.push_back(current_node);

        world_transform = world_transform * local_transform;

        if(tiny_node.mesh >= 0 && static_cast<uint32_t>(tiny_node.mesh) < model.meshes.size())
        {
            tinygltf::Mesh &mesh = model.meshes[tiny_node.mesh];

            joint_map_t joint_map;

            if(tiny_node.skin >= 0 && static_cast<uint32_t>(tiny_node.skin) < model.skins.size())
            {
                const tinygltf::Skin &skin = model.skins[tiny_node.skin];
                out_assets.root_bone = create_bone_hierarchy_bfs(skin, model, node_map);
            }

            for(const auto &primitive: mesh.primitives)
            {
                vierkant::Mesh::entry_create_info_t create_info = {};
                create_info.name = current_node->name;
                create_info.geometry = get_geometry(primitive, primitive.attributes);
                create_info.transform = world_transform;
                create_info.node_index = current_node->index;
                create_info.morph_weights = {mesh.weights.begin(), mesh.weights.end()};

                if(primitive.material >= 0 && static_cast<size_t>(primitive.material) < model.materials.size())
                {
                    create_info.material_index = primitive.material;
                }

                for(const auto &morph_target: primitive.targets)
                {
                    create_info.morph_targets.push_back(get_geometry(primitive, morph_target, true));
                }

                // pushback new entry
                entry_create_infos.push_back(std::move(create_info));

            }// for all primitives
        }    // mesh

        // node references camera
        if(tiny_node.camera >= 0 && static_cast<uint32_t>(tiny_node.camera) < model.cameras.size())
        {
            const auto &tiny_camera = model.cameras[tiny_node.camera];
            spdlog::debug("scene contains camera of type '{}'", tiny_camera.type);

            if(tiny_camera.type == camera_type_perspective)
            {
                vierkant::model::camera_t model_camera = {};
                model_camera.transform = world_transform;
                model_camera.params.aspect = static_cast<float>(tiny_camera.perspective.aspectRatio);
                model_camera.params.set_fovx(
                        static_cast<float>(tiny_camera.perspective.yfov * tiny_camera.perspective.aspectRatio));
                model_camera.params.clipping_distances =
                        glm::vec2(tiny_camera.perspective.znear, tiny_camera.perspective.zfar);
                out_assets.cameras.push_back(model_camera);
            }
            else if(tiny_camera.type == camera_type_orthographic)
            {
                //            tiny_camera.orthographic.znear
                //            tiny_camera.orthographic.zfar
                //            tiny_camera.orthographic.xmag
                //            tiny_camera.orthographic.ymag
                spdlog::warn("camera-type '{}' currently not supported", tiny_camera.type);
            }
        }

        // node references light-source
        if(tiny_node.extensions.count(KHR_lights_punctual))
        {
            const auto &value = tiny_node.extensions.at(KHR_lights_punctual);
            size_t light_index = value.Get(ext_light).GetNumberAsInt();

            if(light_index < out_assets.lights.size())
            {
                auto &l = out_assets.lights[light_index];
                l.position = world_transform.translation;

                auto m = glm::mat3_cast(world_transform.rotation);
                l.direction = -m[2];
            }
        }

        if(!node_map.count(current_index))
        {
            // cache node
            node_map[current_index] = current_node;
        }

        for(auto child_index: tiny_node.children)
        {
            if(child_index >= 0 && static_cast<size_t>(child_index) < model.nodes.size())
            {
                // enqueue child
                node_queue.push_back({static_cast<size_t>(child_index), world_transform, current_node});
            }
        }
    }

    // animations
    for(const auto &tiny_animation: model.animations)
    {
        auto node_animation = create_node_animation(tiny_animation, model, node_map);
        out_assets.node_animations.push_back(std::move(node_animation));
    }
    return out_assets;
}

}// namespace vierkant::model
