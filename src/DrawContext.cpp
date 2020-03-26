//
// Created by crocdialer on 3/2/19.
//

#include "vierkant/DrawContext.hpp"

namespace vierkant
{

void DrawContext::draw_mesh(vierkant::Renderer &renderer, const vierkant::MeshPtr &mesh, const glm::mat4 &model_view,
                            const glm::mat4 &projection)
{
    auto drawables = vierkant::Renderer::create_drawables(renderer.device(), mesh, m_pipeline_cache);

    for(auto &drawable : drawables)
    {
        drawable.matrices.modelview = model_view * drawable.matrices.modelview;
        drawable.matrices.projection = projection;
        renderer.stage_drawable(std::move(drawable));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_scene(vierkant::Renderer &renderer, const vierkant::Object3DConstPtr &root,
                             const vierkant::CameraConstPtr &camera)
{
    // TODO: create drawables with a CullVisitor
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DrawContext::DrawContext(vierkant::DevicePtr device) : m_device(std::move(device))
{
    // images
    {
        // create plane-geometry
        auto plane = Geometry::Plane();
        plane->normals.clear();
        plane->tangents.clear();
        for(auto &v : plane->vertices){ v.xy += glm::vec2(.5f, -.5f); }

        auto mesh = Mesh::create_from_geometries(m_device, {plane});
        auto entry = mesh->entries.front();

        Pipeline::Format fmt = {};
        fmt.blend_state.blendEnable = true;
        fmt.depth_test = false;
        fmt.depth_write = false;
        fmt.shader_stages = shader_stages(vierkant::ShaderType::UNLIT_TEXTURE);
        fmt.binding_descriptions = mesh->binding_descriptions();
        fmt.attribute_descriptions = mesh->attribute_descriptions();
        fmt.primitive_topology = entry.primitive_type;

        // descriptors
        vierkant::descriptor_t desc_matrix = {};
        desc_matrix.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_matrix.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        m_drawable_image.descriptors[vierkant::Renderer::BINDING_MATRIX] = desc_matrix;

        vierkant::descriptor_t desc_material = {};
        desc_material.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_material.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        m_drawable_image.descriptors[vierkant::Renderer::BINDING_MATERIAL] = desc_material;

        vierkant::descriptor_t desc_texture = {};
        desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        m_drawable_image.descriptors[vierkant::Renderer::BINDING_TEXTURES] = desc_texture;

        m_drawable_image.mesh = mesh;
        m_drawable_image.num_indices = entry.num_indices;
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
        vierkant::descriptor_t desc_matrix = {};
        desc_matrix.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_matrix.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        m_drawable_text.descriptors[vierkant::Renderer::BINDING_MATRIX] = desc_matrix;

        vierkant::descriptor_t desc_material = {};
        desc_material.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_material.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        m_drawable_text.descriptors[vierkant::Renderer::BINDING_MATERIAL] = desc_material;

        vierkant::descriptor_t desc_texture = {};
        desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        m_drawable_text.descriptors[vierkant::Renderer::BINDING_TEXTURES] = desc_texture;

        m_drawable_text.pipeline_format = std::move(pipeline_fmt);

        m_drawable_text.descriptor_set_layout = vierkant::create_descriptor_set_layout(m_device,
                                                                                       m_drawable_text.descriptors);
        m_drawable_text.pipeline_format.descriptor_set_layouts = {m_drawable_text.descriptor_set_layout.get()};
    }

    // aabb
    {
        // unit cube
        auto geom = vierkant::Geometry::BoxOutline();
        auto mesh = vierkant::Mesh::create_from_geometries(m_device, {geom});
        mesh->materials.front()->shader_type = vierkant::ShaderType::UNLIT_COLOR;
        m_drawable_aabb = vierkant::Renderer::create_drawables(m_device, mesh).front();
    }

    // grid
    {
        // unit grid
        auto geom = vierkant::Geometry::Grid();
        auto mesh = vierkant::Mesh::create_from_geometries(m_device, {geom});
        auto material = vierkant::Material::create();
        mesh->materials.front()->shader_type = vierkant::ShaderType::UNLIT_COLOR;
        m_drawable_grid = vierkant::Renderer::create_drawables(m_device, mesh).front();
    }

    // create a pipline cache
    m_pipeline_cache = vierkant::PipelineCache::create(m_device);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_text(vierkant::Renderer &renderer, const std::string &text, const FontPtr &font,
                            const glm::vec2 &pos, const glm::vec4 &color)
{
    auto mesh = font->create_mesh(text, color);
    auto entry = mesh->entries.front();

    if(m_drawable_text.pipeline_format.attribute_descriptions.empty())
    {
        m_drawable_text.pipeline_format.attribute_descriptions = mesh->attribute_descriptions();
        m_drawable_text.pipeline_format.binding_descriptions = mesh->binding_descriptions();
    }

    auto drawable = m_drawable_text;
    drawable.mesh = mesh;
    drawable.matrices.projection = glm::orthoRH(0.f, renderer.viewport.width, 0.f, renderer.viewport.height, 0.0f,
                                                1.0f);
    drawable.matrices.modelview[3] = glm::vec4(pos.x, pos.y, 0, 1);
    drawable.descriptors[vierkant::Renderer::BINDING_TEXTURES].image_samplers = {font->glyph_texture()};
    drawable.num_indices = entry.num_indices;
    drawable.num_vertices = entry.num_vertices;
    renderer.stage_drawable(std::move(drawable));
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
    drawable.matrices.modelview = glm::scale(glm::mat4(1), glm::vec3(scale, 1));
    drawable.matrices.modelview[3] = glm::vec4(area.x / renderer.viewport.width, -area.y / renderer.viewport.height, 0,
                                               1);

    // set image
    drawable.descriptors[vierkant::Renderer::BINDING_TEXTURES].image_samplers = {image};

    // stage image drawable
    renderer.stage_drawable(std::move(drawable));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_grid(vierkant::Renderer &renderer, float scale, uint32_t num_subs, const glm::mat4 &model_view,
                            const glm::mat4 &projection)
{
    auto drawable = m_drawable_grid;
    drawable.matrices.modelview = glm::scale(model_view, glm::vec3(scale));
    drawable.matrices.projection = projection;
    renderer.stage_drawable(std::move(drawable));
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
    drawable.matrices.modelview = model_view * center_mat * scale_mat;
    drawable.matrices.projection = projection;
    renderer.stage_drawable(std::move(drawable));
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