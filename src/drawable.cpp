//
// Created by crocdialer on 04.10.22.
//

#include <vierkant/Rasterizer.hpp>
#include <vierkant/drawable.hpp>

namespace vierkant
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<vierkant::drawable_t> create_drawables(const vierkant::mesh_component_t &mesh_component,
                                                   const create_drawables_params_t &params)
{
    const auto &mesh = mesh_component.mesh;
    if(!mesh) { return {}; }

    // reserve space for one drawable per mesh-entry
    std::vector<vierkant::drawable_t> ret;
    ret.reserve(mesh->entries.size());

    // same for all entries
    auto binding_descriptions = vierkant::create_binding_descriptions(mesh->vertex_attribs);
    auto attribute_descriptions = vierkant::create_attribute_descriptions(mesh->vertex_attribs);

    // entry animation transforms
    std::vector<vierkant::transform_t> node_transforms;

    // morph-target weights
    std::vector<std::vector<double>> node_morph_weights;

    if(!mesh_component.library && !mesh->root_bone && params.animation_index < mesh->node_animations.size())
    {
        const auto &animation = mesh->node_animations[params.animation_index];
        vierkant::nodes::build_node_matrices_bfs(mesh->root_node, animation, params.animation_time, node_transforms);
        vierkant::nodes::build_morph_weights_bfs(mesh->root_node, animation, params.animation_time, node_morph_weights);
    }

    bool use_meshlets = mesh->meshlets && mesh->meshlet_vertices && mesh->meshlet_triangles;

    for(uint32_t i = 0; i < mesh->entries.size(); ++i)
    {
        if(mesh_component.entry_indices && !mesh_component.entry_indices->contains(i)) { continue; }
        const auto &entry = mesh->entries[i];
        const auto &lod_0 = mesh->entries[i].lods.front();

        // sanity check material-index
        if(entry.material_index >= mesh->materials.size()) { continue; }

        const auto &mesh_material = mesh->materials[entry.material_index];
        const auto &material = mesh_material->m;

        // acquire ref for mesh-drawable
        vierkant::drawable_t drawable = {};
        drawable.mesh = mesh;
        drawable.entry_index = i;

        // combine mesh- with entry-transform
        if(mesh_component.library) { drawable.matrices.transform = params.transform; }
        else
        {
            drawable.matrices.transform =
                    params.transform * (node_transforms.empty() ? entry.transform : node_transforms[entry.node_index]);
        }

        auto col_img_it = material.texture_data.find(TextureType::Color);
        if(col_img_it != material.texture_data.end() && col_img_it->second.texture_transform)
        {
            drawable.matrices.texture = *col_img_it->second.texture_transform;
        }

        // material params
        drawable.material.color = material.base_color;
        drawable.material.emission = {material.emission, material.emissive_strength};
        drawable.material.ambient = material.occlusion;
        drawable.material.roughness = material.roughness;
        drawable.material.metalness = material.metalness;
        drawable.material.blend_mode = static_cast<uint32_t>(material.blend_mode);
        drawable.material.alpha_cutoff = material.alpha_cutoff;
        drawable.material.two_sided = material.twosided;
        drawable.material.transmission = material.transmission;

        drawable.base_index = lod_0.base_index;
        drawable.num_indices = lod_0.num_indices;
        drawable.vertex_offset = entry.vertex_offset;
        drawable.num_vertices = entry.num_vertices;
        drawable.morph_vertex_offset = entry.morph_vertex_offset;
        drawable.morph_weights =
                (node_morph_weights.empty() ? entry.morph_weights : node_morph_weights[entry.node_index]);
        drawable.base_meshlet = lod_0.base_meshlet;
        drawable.num_meshlets = lod_0.num_meshlets;

        drawable.pipeline_format.primitive_topology = entry.primitive_type;
        drawable.pipeline_format.blend_state.blendEnable = material.blend_mode == vierkant::BlendMode::Blend;
        drawable.pipeline_format.cull_mode = material.twosided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

        if(!drawable.use_own_buffers)
        {
            if(use_meshlets)
            {
                auto &desc_meshlets = drawable.descriptors[Rasterizer::BINDING_MESHLETS];
                desc_meshlets.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                desc_meshlets.stage_flags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;
                desc_meshlets.buffers = {mesh->meshlets};

                auto &desc_meshlet_vertices = drawable.descriptors[Rasterizer::BINDING_MESHLET_VERTICES];
                desc_meshlet_vertices.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                desc_meshlet_vertices.stage_flags = VK_SHADER_STAGE_MESH_BIT_EXT;
                desc_meshlet_vertices.buffers = {mesh->meshlet_vertices};

                auto &desc_meshlet_triangles = drawable.descriptors[Rasterizer::BINDING_MESHLET_TRIANGLES];
                desc_meshlet_triangles.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                desc_meshlet_triangles.stage_flags = VK_SHADER_STAGE_MESH_BIT_EXT;
                desc_meshlet_triangles.buffers = {mesh->meshlet_triangles};
            }
        }

        // NOTE: not used anymore by most pipelines
        drawable.pipeline_format.binding_descriptions = binding_descriptions;
        drawable.pipeline_format.attribute_descriptions = attribute_descriptions;

        // textures
        if(!mesh_material->textures.empty())
        {
            vierkant::descriptor_t &desc_texture = drawable.descriptors[Rasterizer::BINDING_TEXTURES];
            desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

            for(auto &[type_flag, tex]: mesh_material->textures)
            {
                if(tex)
                {
                    drawable.material.texture_type_flags |= static_cast<uint32_t>(type_flag);
                    desc_texture.images.push_back(tex);
                }
            }
        }

        // push drawable to vector
        ret.push_back(std::move(drawable));
    }
    return ret;
}

}// namespace vierkant