//
// Created by crocdialer on 3/2/19.
//

#include "vierkant/DrawContext.hpp"
#include <vierkant/shaders.hpp>

namespace vierkant
{

vierkant::ImagePtr render_offscreen(vierkant::Framebuffer &framebuffer,
                                    vierkant::Renderer &renderer,
                                    const std::function<void()> &stage_fn,
                                    VkQueue queue,
                                    bool sync)
{
    // wait for prior frame to finish
    framebuffer.wait_fence();

    // invoke function-object to stage drawables
    stage_fn();

    // create a commandbuffer
    VkCommandBuffer cmd_buffer = renderer.render(framebuffer);

    // submit rendering commands to queue
    auto fence = framebuffer.submit({cmd_buffer}, queue ? queue : renderer.device()->queue());

    if(sync){ vkWaitForFences(renderer.device()->handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max()); }

    // check for resolve-attachment, fallback to color-attachment
    return framebuffer.color_attachment();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


DrawContext::DrawContext(vierkant::DevicePtr device) : m_device(std::move(device))
{
    // create a pipeline cache
    m_pipeline_cache = vierkant::PipelineCache::create(m_device);

    // images
    {
        // create plane-geometry
        auto plane = Geometry::Plane();
        plane->normals.clear();
        plane->tangents.clear();
//        for(auto &v : plane->vertices){ v.xy() += glm::vec2(.5f, -.5f); }

        auto mesh = Mesh::create_from_geometry(m_device, plane, {});
        auto entry = mesh->entries.front();

        graphics_pipeline_info_t fmt = {};
        fmt.blend_state.blendEnable = true;
        fmt.depth_test = false;
        fmt.depth_write = false;
        fmt.shader_stages = m_pipeline_cache->shader_stages(vierkant::ShaderType::UNLIT_TEXTURE);
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

    // fullscreen
    {
        graphics_pipeline_info_t fmt = {};
        fmt.blend_state.blendEnable = true;
        fmt.depth_test = false;
        fmt.depth_write = false;
        fmt.shader_stages = m_pipeline_cache->shader_stages(vierkant::ShaderType::FULLSCREEN_TEXTURE);
        fmt.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // descriptors
        vierkant::descriptor_t desc_texture = {};
        desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        m_drawable_image_fullscreen.descriptors[0] = desc_texture;
        m_drawable_image_fullscreen.num_vertices = 3;
        m_drawable_image_fullscreen.pipeline_format = fmt;
        m_drawable_image_fullscreen.use_own_buffers = true;
        m_drawable_image_fullscreen.pipeline_format.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;

        m_drawable_color_depth_fullscreen = m_drawable_image_fullscreen;
        m_drawable_color_depth_fullscreen.pipeline_format.depth_test = false;
        m_drawable_color_depth_fullscreen.pipeline_format.depth_write = true;
        m_drawable_color_depth_fullscreen.pipeline_format.shader_stages =
                m_pipeline_cache->shader_stages(vierkant::ShaderType::FULLSCREEN_TEXTURE_DEPTH);
    }

    // fonts
    {
        // pipeline format
        vierkant::graphics_pipeline_info_t pipeline_fmt = {};
        pipeline_fmt.shader_stages = m_pipeline_cache->shader_stages(vierkant::ShaderType::UNLIT_TEXTURE);
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
        auto mesh = vierkant::Mesh::create_from_geometry(m_device, geom, {});
        m_drawable_aabb = vierkant::Renderer::create_drawables(mesh).front();
        m_drawable_aabb.pipeline_format.shader_stages = m_pipeline_cache->shader_stages(
                vierkant::ShaderType::UNLIT_COLOR);
    }

    // grid
    {
        // unit grid
        auto geom = vierkant::Geometry::Grid();
        auto mesh = vierkant::Mesh::create_from_geometry(m_device, geom, {});
        m_drawable_grid = vierkant::Renderer::create_drawables(mesh).front();
        m_drawable_grid.pipeline_format.shader_stages = m_pipeline_cache->shader_stages(
                vierkant::ShaderType::UNLIT_COLOR);
    }

    // skybox
    {
        auto box = vierkant::Geometry::Box();
        box->colors.clear();
        box->tex_coords.clear();
        box->tangents.clear();
        box->normals.clear();
        auto mesh = vierkant::Mesh::create_from_geometry(m_device, box, {});
        auto &mat = mesh->materials.front();
        mat->depth_write = false;
        mat->depth_test = true;
        mat->cull_mode = VK_CULL_MODE_FRONT_BIT;
        mat->textures[vierkant::Material::TextureType::Environment] = {};
        m_drawable_skybox = vierkant::Renderer::create_drawables(mesh).front();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_mesh(vierkant::Renderer &renderer, const vierkant::MeshPtr &mesh, const glm::mat4 &model_view,
                            const glm::mat4 &projection, vierkant::ShaderType shader_type)
{
    auto drawables = vierkant::Renderer::create_drawables(mesh);

    for(auto &drawable : drawables)
    {
        drawable.pipeline_format.shader_stages = m_pipeline_cache->shader_stages(shader_type);
        drawable.matrices.modelview = model_view * drawable.matrices.modelview;
        drawable.matrices.projection = projection;
        renderer.stage_drawable(std::move(drawable));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
    drawable.matrices.modelview[3] = glm::vec4(0.5f + area.x / renderer.viewport.width, -0.5f + -area.y / renderer.viewport.height, 0,
                                               1);

    // set image
    drawable.descriptors[vierkant::Renderer::BINDING_TEXTURES].image_samplers = {image};

    // stage image drawable
    renderer.stage_drawable(std::move(drawable));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_image_fullscreen(Renderer &renderer, const ImagePtr &image, const vierkant::ImagePtr &depth,
                                        bool depth_test)
{
    // create image-drawable
    vierkant::Renderer::drawable_t drawable;

    if(image && depth)
    {
        // set image + depth
        drawable = m_drawable_color_depth_fullscreen;
        drawable.pipeline_format.depth_test = depth_test;
        drawable.descriptors[0].image_samplers = {image, depth};
    }
    else if(image)
    {
        // set image
        drawable = m_drawable_image_fullscreen;
        drawable.pipeline_format.depth_test = depth_test;
        drawable.descriptors[0].image_samplers = {image};
    }

    // stage image drawable
    renderer.stage_drawable(std::move(drawable));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_grid(vierkant::Renderer &renderer, float scale, uint32_t num_subs, const glm::mat4 &model_view,
                            const glm::mat4 &projection)
{
    auto drawable = m_drawable_grid;
    drawable.matrices.modelview = glm::scale(model_view, glm::vec3(scale));
    drawable.matrices.projection = projection;
    renderer.stage_drawable(std::move(drawable));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_skybox(vierkant::Renderer &renderer, const vierkant::ImagePtr &environment,
                              const vierkant::CameraPtr &cam)
{
    glm::mat4 m = cam->view_matrix();
    m[3] = glm::vec4(0, 0, 0, 1);
    m = glm::scale(m, glm::vec3(cam->far() * .99f));

    auto drawable = m_drawable_skybox;
    drawable.matrices.modelview = m;
    drawable.matrices.projection = cam->projection_matrix();
    drawable.descriptors[vierkant::Renderer::BINDING_TEXTURES].image_samplers = {environment};
    drawable.pipeline_format.shader_stages = m_pipeline_cache->shader_stages(vierkant::ShaderType::UNLIT_CUBE);

    renderer.stage_drawable(drawable);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

}