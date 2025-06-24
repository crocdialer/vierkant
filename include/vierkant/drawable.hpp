//
// Created by crocdialer on 04.10.22.
//

#pragma once

#include <crocore/NamedId.hpp>
#include <optional>

#include <vierkant/descriptor.hpp>
#include <vierkant/mesh_component.hpp>

namespace vierkant
{

struct matrix_struct_t
{
    glm::mat4 projection = glm::mat4(1);
    glm::mat4 texture = glm::mat4(1);
    vierkant::transform_t transform = {};
    uint32_t pad[2] = {};
};

struct alignas(16) material_struct_t
{
    glm::vec4 color = glm::vec4(1);

    glm::vec4 emission = glm::vec4(0, 0, 0, 1);

    float metalness = 0.f;

    float roughness = 1.f;

    float ambient = 1.f;

    uint32_t blend_mode = static_cast<uint32_t>(vierkant::BlendMode::Opaque);

    float alpha_cutoff = 0.5f;

    float transmission = 0.f;

    float ior = 1.5f;

    float attenuation_distance = std::numeric_limits<float>::infinity();

    glm::vec4 attenuation_color = glm::vec4(1.f);

    float clearcoat_factor = 0.f;

    float clearcoat_roughness_factor = 0.f;

    float iridescence_factor = 0.f;

    float iridescence_ior = 1.3f;

    // range of thin-film thickness in nanometers (nm)
    glm::vec2 iridescence_thickness_range = {100.f, 400.f};

    uint32_t base_texture_index = 0;

    uint32_t texture_type_flags = 0;

    uint32_t two_sided = false;
};

//! define a strong id-type for drawables
DEFINE_NAMED_ID(DrawableId);

/**
 * @brief   drawable_t groups all necessary information for a drawable object.
 */
struct drawable_t
{
    DrawableId id;

    MeshConstPtr mesh;

    //! optional override for vertex-buffer provided by mesh, if any
    VkDeviceAddress vertex_buffer = 0;

    uint32_t entry_index = 0;

    graphics_pipeline_info_t pipeline_format = {};

    matrix_struct_t matrices = {};

    std::optional<matrix_struct_t> last_matrices;

    material_struct_t material = {};

    //! a descriptormap
    descriptor_map_t descriptors;

    //! optional descriptor-set-layout
    DescriptorSetLayoutPtr descriptor_set_layout;

    //! binary blob for push-constants
    std::vector<uint8_t> push_constants;

    uint32_t base_index = 0;
    uint32_t num_indices = 0;

    int32_t vertex_offset = 0;
    uint32_t num_vertices = 0;

    uint32_t morph_vertex_offset = 0;
    std::vector<double> morph_weights;

    uint32_t base_meshlet = 0;
    uint32_t num_meshlets = 0;

    uint32_t num_instances = 1;

    bool use_own_buffers = false;
    bool share_material = true;
};

struct create_drawables_params_t
{
    vierkant::transform_t transform = {};
    uint32_t animation_index = 0;
    float animation_time = 0.f;
};

/**
 * @brief   Factory to create drawables from a provided mesh.
 *
 * @param   params  a struct containing a mesh and other params for drawable-creation.
 * @return  an array of drawables for the mesh-entries.
 */
std::vector<vierkant::drawable_t> create_drawables(const vierkant::mesh_component_t &mesh_component,
                                                   const create_drawables_params_t &params);

}// namespace vierkant
