//
// Created by crocdialer on 11/20/20.
//

#pragma once

//! serializers for the asset-bundle on-disk format. the serializers for engine components other
//! documents legitimately embed live next door in components.hpp.

// GCC 13 false-positive -Wdangling-reference (gcc.gnu.org/bugzilla/show_bug.cgi?id=107488):
// cereal's polymorphic_impl.hpp lookup() returns a stored ref, not a ref-to-temporary;
// the declaration-only heuristic in do_warn_dangling_reference() misfires here, fixed in GCC 14
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ == 13)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-reference"
#endif

#include <cereal/cereal.hpp>

#include <cereal/types/memory.hpp>       // nodes::NodePtr
#include <cereal/types/optional.hpp>
#include <cereal/types/polymorphic.hpp>   // CEREAL_REGISTER_TYPE / _POLYMORPHIC_RELATION below
#include <cereal/types/unordered_map.hpp> // model_assets_t::textures, ::texture_samplers
#include <cereal/types/variant.hpp>       // geometry_variant_t
#include <cereal/types/vector.hpp>

#include "animation_cereal.hpp"
#include "collision_cereal.hpp"
#include "components.hpp"
#include "glm_cereal.hpp"
#include "optional_nvp_cereal.hpp"

#include "vierkant/model/model_loading.hpp"
#include <vierkant/Mesh.hpp>
#include <vierkant/texture_block_compression.hpp>

namespace cereal
{

template<class Archive>
void serialize(Archive &archive, vierkant::bcn::block_t &block)
{ archive(cereal::make_nvp("value", block.value)); }

template<class Archive>
void serialize(Archive &archive, VkMicromapTriangleEXT &triangle)
{
    archive(cereal::make_nvp("dataOffset", triangle.dataOffset),
            cereal::make_nvp("subdivisionLevel", triangle.subdivisionLevel),
            cereal::make_nvp("format", triangle.format));
}

template<class Archive>
void serialize(Archive &archive, vierkant::bcn::compress_result_t &compress_result)
{
    archive(cereal::make_nvp("mode", compress_result.mode), cereal::make_nvp("base_width", compress_result.base_width),
            cereal::make_nvp("base_height", compress_result.base_height),
            cereal::make_nvp("levels", compress_result.levels));
}

}// namespace cereal

namespace crocore
{

template<class Archive>
void save(Archive &archive, const crocore::Image_<unsigned char> &img)
{ archive(crocore::encode_png(img)); }

template<class Archive>
void load(Archive &archive, crocore::Image_<unsigned char> &img)
{
    std::vector<uint8_t> array;
    archive(array);
    img = std::move(*std::dynamic_pointer_cast<crocore::Image_<unsigned char>>(crocore::create_image_from_data(array)));
}
}// namespace crocore

CEREAL_REGISTER_TYPE(crocore::Image_<unsigned char>);
CEREAL_REGISTER_POLYMORPHIC_RELATION(crocore::Image, crocore::Image_<unsigned char>);

namespace vierkant
{

template<class Archive>
void serialize(Archive &archive, vierkant::Geometry &g)
{
    archive(cereal::make_nvp("topology", g.topology), cereal::make_nvp("positions", g.positions),
            cereal::make_nvp("colors", g.colors), cereal::make_nvp("tex_coords", g.tex_coords),
            cereal::make_nvp("normals", g.normals), cereal::make_nvp("tangents", g.tangents),
            cereal::make_nvp("bone_indices", g.bone_indices), cereal::make_nvp("bone_weights", g.bone_weights),
            cereal::make_nvp("indices", g.indices));
}

template<class Archive>
void serialize(Archive &archive, vierkant::AABB &aabb)
{ archive(cereal::make_nvp("min", aabb.min), cereal::make_nvp("max", aabb.max)); }

template<class Archive>
void serialize(Archive &archive, vierkant::Sphere &sphere)
{ archive(cereal::make_nvp("center", sphere.center), cereal::make_nvp("radius", sphere.radius)); }

template<class Archive>
void serialize(Archive &archive, vierkant::Cone &cone)
{ archive(cereal::make_nvp("axis", cone.axis), cereal::make_nvp("cutoff", cone.cutoff)); }

template<class Archive>
void serialize(Archive &ar, material_data_t &material_data)
{
    ar(cereal::make_nvp("materials", material_data.materials), cereal::make_nvp("textures", material_data.textures),
       cereal::make_nvp("texture_samplers", material_data.texture_samplers));
}

template<class Archive>
void serialize(Archive &archive, vierkant::vertex_attrib_t &vertex_attrib)
{
    archive(cereal::make_nvp("buffer_offset", vertex_attrib.buffer_offset),
            //          cereal::make_nvp("buffer", vertex_attrib.buffer),
            cereal::make_nvp("offset", vertex_attrib.offset), cereal::make_nvp("stride", vertex_attrib.stride),
            cereal::make_nvp("format", vertex_attrib.format), cereal::make_nvp("input_rate", vertex_attrib.input_rate));
}

template<class Archive>
void serialize(Archive &archive, vierkant::Mesh::entry_create_info_t &entry_info)
{
    archive(cereal::make_nvp("name", entry_info.name), cereal::make_nvp("geometry", entry_info.geometry),
            cereal::make_nvp("transform", entry_info.transform), cereal::make_nvp("node_index", entry_info.node_index),
            cereal::make_nvp("material_index", entry_info.material_index),
            cereal::make_nvp("morph_targets", entry_info.morph_targets),
            cereal::make_nvp("morph_weights", entry_info.morph_weights));
}

template<class Archive>
void serialize(Archive &archive, vierkant::Mesh::lod_t &lod)
{
    archive(cereal::make_nvp("base_index", lod.base_index), cereal::make_nvp("num_indices", lod.num_indices),
            cereal::make_nvp("base_meshlet", lod.base_meshlet), cereal::make_nvp("num_meshlets", lod.num_meshlets));
}

template<class Archive>
void serialize(Archive &archive, vierkant::Mesh::entry_t &entry)
{
    archive(cereal::make_nvp("name", entry.name), cereal::make_nvp("transform", entry.transform),
            cereal::make_nvp("bounding_box", entry.bounding_box),
            cereal::make_nvp("bounding_sphere", entry.bounding_sphere),
            cereal::make_nvp("node_index", entry.node_index), cereal::make_nvp("vertex_offset", entry.vertex_offset),
            cereal::make_nvp("num_vertices", entry.num_vertices), cereal::make_nvp("lods", entry.lods),
            cereal::make_nvp("material_index", entry.material_index),
            cereal::make_nvp("primitive_type", entry.primitive_type),
            cereal::make_nvp("morph_vertex_offset", entry.morph_vertex_offset),
            cereal::make_nvp("morph_weights", entry.morph_weights));
}

template<class Archive>
void serialize(Archive &archive, vierkant::Mesh::meshlet_t &meshlet)
{
    archive(cereal::make_nvp("vertex_offset", meshlet.vertex_offset),
            cereal::make_nvp("triangle_offset", meshlet.triangle_offset),
            cereal::make_nvp("vertex_count", meshlet.vertex_count),
            cereal::make_nvp("triangle_count", meshlet.triangle_count),
            cereal::make_nvp("cone_axis", meshlet.cone_axis), cereal::make_nvp("cone_cutoff", meshlet.cone_cutoff),
            cereal::make_nvp("bounding_sphere", meshlet.bounding_sphere));
}

template<class Archive>
void serialize(Archive &archive, vierkant::mesh_buffer_bundle_t &mesh_buffer_bundle)
{
    archive(cereal::make_nvp("vertex_stride", mesh_buffer_bundle.vertex_stride),
            cereal::make_nvp("vertex_attribs", mesh_buffer_bundle.vertex_attribs),
            cereal::make_nvp("entries", mesh_buffer_bundle.entries),
            cereal::make_nvp("num_materials", mesh_buffer_bundle.num_materials),
            cereal::make_nvp("vertex_buffer", mesh_buffer_bundle.vertex_buffer),
            cereal::make_nvp("index_buffer", mesh_buffer_bundle.index_buffer),
            cereal::make_nvp("bone_vertex_buffer", mesh_buffer_bundle.bone_vertex_buffer),
            cereal::make_nvp("morph_buffer", mesh_buffer_bundle.morph_buffer),
            cereal::make_nvp("num_morph_targets", mesh_buffer_bundle.num_morph_targets),
            cereal::make_nvp("meshlets", mesh_buffer_bundle.meshlets),
            cereal::make_nvp("meshlet_vertices", mesh_buffer_bundle.meshlet_vertices),
            cereal::make_nvp("meshlet_triangles", mesh_buffer_bundle.meshlet_triangles));
}
}// namespace vierkant

namespace vierkant::model
{

template<class Archive>
void serialize(Archive &archive, vierkant::model::mesh_omm_entry_t &omm_entry)
{
    archive(cereal::make_nvp("data", omm_entry.data), cereal::make_nvp("triangles", omm_entry.triangles),
            cereal::make_nvp("indices", omm_entry.indices));
}

template<class Archive>
void serialize(Archive &archive, vierkant::model::mesh_omm_data_t &omm_data)
{
    archive(cereal::make_nvp("entry_index", omm_data.entry_index),
            cereal::make_nvp("color_texture_id", omm_data.color_texture_id),
            cereal::make_nvp("entry", omm_data.entry));
}

template<class Archive>
void serialize(Archive &archive, vierkant::model::lightsource_instance_t &light_instance)
{
    archive(cereal::make_nvp("transform", light_instance.transform),
            cereal::make_nvp("light_id", light_instance.light_id));
}

template<class Archive>
void serialize(Archive &archive, vierkant::model::model_assets_t &mesh_assets)
{
    archive(cereal::make_nvp("geometry_data", mesh_assets.geometry_data),
            cereal::make_nvp("materials", mesh_assets.materials), cereal::make_nvp("textures", mesh_assets.textures),
            cereal::make_nvp("texture_samplers", mesh_assets.texture_samplers),
            //            cereal::make_nvp("cameras", mesh_assets.cameras),
            cereal::make_nvp("root_node", mesh_assets.root_node), cereal::make_nvp("root_bone", mesh_assets.root_bone),
            cereal::make_nvp("node_animations", mesh_assets.node_animations),
            // appended fields: bundles are stored via BinaryArchive (positional), so these are read back
            // only for bundles written by this or a newer version; older bundles get a different
            // cache-filename (see model_bundle_filename) and are re-baked rather than mis-read.
            cereal::make_nvp("omm_data", mesh_assets.omm_data), cereal::make_nvp("lights", mesh_assets.lights),
            cereal::make_nvp("light_instances", mesh_assets.light_instances));
}

}// namespace vierkant::model

#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ == 13)
#pragma GCC diagnostic pop
#endif
