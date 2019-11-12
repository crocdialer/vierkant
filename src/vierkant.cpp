//
// Created by crocdialer on 3/2/19.
//

#include "vierkant/vierkant.hpp"

namespace vierkant {

///////////////////////////////////////////////////////////////////////////////////////////////////

DrawContext::DrawContext(vierkant::DevicePtr device) : m_device(std::move(device))
{
    // images
    {
        // create plane-geometry
        auto plane = Geometry::Plane();
        plane->normals.clear();
        plane->tangents.clear();
        for(auto &v : plane->vertices){ v.xy += glm::vec2(.5f, -.5f); }

        auto mesh = create_mesh_from_geometry(m_device, plane);

        Pipeline::Format fmt = {};
        fmt.blend_state.blendEnable = true;
        fmt.depth_test = false;
        fmt.depth_write = false;
        fmt.shader_stages = shader_stages(vierkant::ShaderType::UNLIT_TEXTURE);
        fmt.binding_descriptions = vierkant::binding_descriptions(mesh);
        fmt.attribute_descriptions = vierkant::attribute_descriptions(mesh);
        fmt.primitive_topology = mesh->topology;

        // descriptors
        vierkant::descriptor_t desc_ubo = {};
        desc_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_ubo.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        desc_ubo.binding = 0;

        vierkant::descriptor_t desc_texture = {};
        desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        desc_texture.binding = 1;

        m_drawable_image.mesh = mesh;
        m_drawable_image.num_indices = mesh->num_elements;
        m_drawable_image.descriptors = {desc_ubo, desc_texture};
        m_drawable_image.descriptor_set_layout = vierkant::create_descriptor_set_layout(m_device,
                                                                                        m_drawable_image.descriptors);
        fmt.descriptor_set_layouts = {m_drawable_image.descriptor_set_layout.get()};
        m_drawable_image.pipeline_format = fmt;
    }

    // fonts
    {
        // pipeline format
        vierkant::Pipeline::Format pipeline_fmt = {};
        pipeline_fmt.shader_stages = shader_stages(vierkant::ShaderType::UNLIT_TEXTURE);
        pipeline_fmt.depth_write = false;
        pipeline_fmt.depth_test = false;
        pipeline_fmt.blend_state.blendEnable = true;
        pipeline_fmt.cull_mode = VK_CULL_MODE_BACK_BIT;
        pipeline_fmt.dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT};

        // descriptors
        vierkant::descriptor_t desc_ubo = {};
        desc_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_ubo.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        desc_ubo.binding = 0;

        vierkant::descriptor_t desc_texture = {};
        desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        desc_texture.binding = 1;

        m_drawable_text.pipeline_format = std::move(pipeline_fmt);
        m_drawable_text.descriptors = {desc_ubo, desc_texture};
        m_drawable_text.descriptor_set_layout = vierkant::create_descriptor_set_layout(m_device,
                                                                                       m_drawable_text.descriptors);
        m_drawable_text.pipeline_format.descriptor_set_layouts = {m_drawable_text.descriptor_set_layout.get()};
    }

    // aabb
    {
        // unit cube
        auto geom = vierkant::Geometry::BoxOutline();
        auto mesh = vierkant::create_mesh_from_geometry(m_device, geom);
        auto material = vierkant::Material::create();
        material->shader_type = vierkant::ShaderType::UNLIT_COLOR;
        m_drawable_aabb = vierkant::Renderer::create_drawable(m_device, mesh, material);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_text(vierkant::Renderer &renderer, const std::string &text, const FontPtr &font,
                            const glm::vec2 &pos, const glm::vec4 &color)
{
    auto mesh = font->create_mesh(text, color);

    if(m_drawable_text.pipeline_format.attribute_descriptions.empty())
    {
        m_drawable_text.pipeline_format.attribute_descriptions = vierkant::attribute_descriptions(mesh);
        m_drawable_text.pipeline_format.binding_descriptions = vierkant::binding_descriptions(mesh);
    }

    auto drawable = m_drawable_text;
    drawable.mesh = mesh;
    drawable.matrices.projection = glm::orthoRH(0.f, renderer.viewport.width, 0.f, renderer.viewport.height, 0.0f,
                                                1.0f);
    drawable.matrices.model[3] = glm::vec4(pos.x, pos.y, 0, 1);
    drawable.descriptors[1].image_samplers = {font->glyph_texture()};
    drawable.num_indices = mesh->num_elements;
    renderer.stage_drawable(drawable);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_image(vierkant::Renderer &renderer, const vierkant::ImagePtr &image,
                             const crocore::Area_<int> &area)
{
    float w = area.width ? area.width : renderer.viewport.width;
    float h = area.height ? area.height : renderer.viewport.height;
    glm::vec2 scale = glm::vec2(w, h) / glm::vec2(renderer.viewport.width, renderer.viewport.height);

    // copy image-drawable
    auto drawable = m_drawable_image;
    drawable.matrices.projection = glm::orthoRH(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    drawable.matrices.projection[1][1] *= -1;
    drawable.matrices.model = glm::scale(glm::mat4(1), glm::vec3(scale, 1));
    drawable.matrices.model[3] = glm::vec4(area.x / renderer.viewport.width, -area.y / renderer.viewport.height, 0, 1);

    // set image
    drawable.descriptors[vierkant::Renderer::SLOT_TEXTURES].image_samplers = {image};

    // stage image drawable
    renderer.stage_drawable(drawable);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_boundingbox(vierkant::Renderer &renderer, const vierkant::AABB &aabb,
                                   const glm::mat4 &model_view, const glm::mat4 &projection)
{
    auto drawable = m_drawable_aabb;

    glm::mat4 center_mat = glm::translate(glm::mat4(1), aabb.center());
    glm::mat4 scale_mat = glm::scale(glm::mat4(1), glm::vec3(aabb.width(),
                                                            aabb.height(),
                                                            aabb.depth()));
    drawable.matrices.model = scale_mat * center_mat * model_view;
    drawable.matrices.projection = projection;
    renderer.stage_drawable(drawable);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const shader_stage_map_t &DrawContext::shader_stages(vierkant::ShaderType type)
{
    auto it = m_shader_stage_cache.find(type);

    if(it != m_shader_stage_cache.end()){ return it->second; }
    else
    {
        it = m_shader_stage_cache.insert(std::make_pair(type, vierkant::create_shader_stages(m_device, type))).first;
        return it->second;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

}