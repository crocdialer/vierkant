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

    VkCommandBufferInheritanceInfo inheritance = {};
    inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritance.framebuffer = framebuffer.handle();
    inheritance.renderPass = framebuffer.renderpass().get();

    // invoke function-object to stage drawables
    stage_fn();

    // create a commandbuffer
    VkCommandBuffer cmd_buffer = renderer.render(&inheritance);

    // submit rendering commands to queue
    auto fence = framebuffer.submit({cmd_buffer}, queue ? queue : renderer.device()->queue());

    if(sync){ vkWaitForFences(renderer.device()->handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max()); }

    // check for resolve-attachment, fallback to color-attachment
    auto attach_it = framebuffer.attachments().find(vierkant::Framebuffer::AttachmentType::Resolve);

    if(attach_it == framebuffer.attachments().end())
    {
        attach_it = framebuffer.attachments().find(vierkant::Framebuffer::AttachmentType::Color);
    }

    // return color-attachment
    if(attach_it != framebuffer.attachments().end()){ return attach_it->second.front(); }
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr cubemap_from_panorama(const vierkant::ImagePtr &panorama_img, const glm::vec2 &size)
{
    if(!panorama_img){ return nullptr; }

    auto device = panorama_img->device();

    // framebuffer image-format
    vierkant::Image::Format img_fmt = {};
    img_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    img_fmt.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    img_fmt.num_layers = 6;
    img_fmt.format = VK_FORMAT_R16G16B16A16_SFLOAT;

    // create cube framebuffer
    vierkant::Framebuffer::create_info_t fb_create_info = {};
    fb_create_info.size = {static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y), 1};
    fb_create_info.color_attachment_format = img_fmt;

    auto cube_fb = vierkant::Framebuffer(device, fb_create_info);

    // create cube pipeline with geometry shader

    // render
    vierkant::Renderer::create_info_t cuber_render_create_info = {};
    cuber_render_create_info.renderpass = cube_fb.renderpass();
    cuber_render_create_info.num_frames_in_flight = 1;
    cuber_render_create_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    cuber_render_create_info.viewport.width = cube_fb.extent().width;
    cuber_render_create_info.viewport.height = cube_fb.extent().height;
    cuber_render_create_info.viewport.maxDepth = cube_fb.extent().depth;
    auto cube_render = vierkant::Renderer(device, cuber_render_create_info);

    // create a drawable
    vierkant::Renderer::drawable_t drawable = {};
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::cube_vert);
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_GEOMETRY_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::cube_layers_geom);
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::unlit_panorama_frag);

    drawable.mesh = vierkant::Mesh::create_from_geometries(device, {vierkant::Geometry::Box()});
    const auto &mesh_entry = drawable.mesh->entries.front();

    drawable.base_index = mesh_entry.base_index;
    drawable.num_indices = mesh_entry.num_indices;
    drawable.base_vertex = mesh_entry.base_vertex;
    drawable.num_vertices = mesh_entry.num_vertices;

    drawable.pipeline_format.binding_descriptions = drawable.mesh->binding_descriptions();
    drawable.pipeline_format.attribute_descriptions = drawable.mesh->attribute_descriptions();
    drawable.pipeline_format.primitive_topology = mesh_entry.primitive_type;
    drawable.pipeline_format.blend_state.blendEnable = false;
    drawable.pipeline_format.depth_test = false;
    drawable.pipeline_format.depth_write = false;
    drawable.pipeline_format.cull_mode = VK_CULL_MODE_FRONT_BIT;
    drawable.use_own_buffers = true;

    auto cube_cam = vierkant::CubeCamera::create(.1f, 10.f);

    struct geom_shader_ubo_t
    {
        glm::mat4 view_matrix[6];
        glm::mat4 model_matrix = glm::mat4(1);
        glm::mat4 projection_matrix = glm::mat4(1);
    };
    geom_shader_ubo_t ubo_data = {};
    memcpy(ubo_data.view_matrix, cube_cam->view_matrices().data(), sizeof(ubo_data.view_matrix));
    ubo_data.projection_matrix = cube_cam->projection_matrix();

    vierkant::descriptor_t desc_matrices = {};
    desc_matrices.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_matrices.stage_flags = VK_SHADER_STAGE_GEOMETRY_BIT;
    desc_matrices.buffer = vierkant::Buffer::create(device, &ubo_data, sizeof(ubo_data),
                                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    drawable.descriptors[0] = desc_matrices;

    vierkant::descriptor_t desc_image = {};
    desc_image.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_image.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_image.image_samplers = {panorama_img};
    drawable.descriptors[1] = desc_image;

    // stage cube-drawable
    cube_render.stage_drawable(drawable);

    VkCommandBufferInheritanceInfo inheritance = {};
    inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritance.framebuffer = cube_fb.handle();
    inheritance.renderPass = cube_fb.renderpass().get();

    auto cmd_buf = cube_render.render(&inheritance);
    auto fence = cube_fb.submit({cmd_buf}, device->queue());

    // mandatory to sync here
    vkWaitForFences(device->handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());

    auto attach_it = cube_fb.attachments().find(vierkant::Framebuffer::AttachmentType::Color);

    // return color-attachment
    if(attach_it != cube_fb.attachments().end() && !attach_it->second.empty()){ return attach_it->second.front(); }

    return vierkant::ImagePtr();
}

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

DrawContext::DrawContext(vierkant::DevicePtr device) : m_device(std::move(device))
{
    // create a pipline cache
    m_pipeline_cache = vierkant::PipelineCache::create(m_device);

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

    // fonts
    {
        // pipeline format
        vierkant::Pipeline::Format pipeline_fmt = {};
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
        auto mesh = vierkant::Mesh::create_from_geometries(m_device, {geom});
        m_drawable_aabb = vierkant::Renderer::create_drawables(mesh).front();
        m_drawable_aabb.pipeline_format.shader_stages = m_pipeline_cache->shader_stages(
                vierkant::ShaderType::UNLIT_COLOR);
    }

    // grid
    {
        // unit grid
        auto geom = vierkant::Geometry::Grid();
        auto mesh = vierkant::Mesh::create_from_geometries(m_device, {geom});
        m_drawable_grid = vierkant::Renderer::create_drawables(mesh).front();
        m_drawable_grid.pipeline_format.shader_stages = m_pipeline_cache->shader_stages(
                vierkant::ShaderType::UNLIT_COLOR);
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
    drawable.matrices.modelview[3] = glm::vec4(area.x / renderer.viewport.width, -area.y / renderer.viewport.height, 0,
                                               1);

    // set image
    drawable.descriptors[vierkant::Renderer::BINDING_TEXTURES].image_samplers = {image};

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

}