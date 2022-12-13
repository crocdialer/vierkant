//
// Created by crocdialer on 3/2/19.
//

#include <deque>

#include "vierkant/DrawContext.hpp"
#include <vierkant/shaders.hpp>

namespace vierkant
{

DrawContext::DrawContext(vierkant::DevicePtr device) : m_device(std::move(device))
{
    // create a pipeline cache
    m_pipeline_cache = vierkant::PipelineCache::create(m_device);

    // images
    {
        // create plane-geometry
        auto plane = Geometry::Plane(2.f, 2.f);
        plane->normals.clear();
        plane->tangents.clear();

        auto mesh = Mesh::create_from_geometry(m_device, plane, {});
        const auto &entry = mesh->entries.front();
        const auto &lod = entry.lods.front();

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
        desc_matrix.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_matrix.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        m_drawable_image.descriptors[vierkant::Renderer::BINDING_MESH_DRAWS] = desc_matrix;

        vierkant::descriptor_t desc_material = {};
        desc_material.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_material.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        m_drawable_image.descriptors[vierkant::Renderer::BINDING_MATERIAL] = desc_material;

        vierkant::descriptor_t desc_texture = {};
        desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        m_drawable_image.descriptors[vierkant::Renderer::BINDING_TEXTURES] = desc_texture;

        m_drawable_image.mesh = mesh;
        m_drawable_image.num_indices = lod.num_indices;
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
        fmt.dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        // descriptors
        vierkant::descriptor_t desc_texture = {};
        desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        m_drawable_image_fullscreen.descriptors[0] = desc_texture;
        m_drawable_image_fullscreen.num_vertices = 3;
        m_drawable_image_fullscreen.pipeline_format = fmt;
        m_drawable_image_fullscreen.use_own_buffers = true;

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
        desc_matrix.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_matrix.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        m_drawable_text.descriptors[vierkant::Renderer::BINDING_MESH_DRAWS] = desc_matrix;

        vierkant::descriptor_t desc_material = {};
        desc_material.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
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

    vierkant::create_drawables_params_t drawable_params = {};

    // aabb
    {
        // unit cube
        auto geom = vierkant::Geometry::BoxOutline();
        drawable_params.mesh = vierkant::Mesh::create_from_geometry(m_device, geom, {});
        m_drawable_aabb = vierkant::create_drawables(drawable_params).front();
        m_drawable_aabb.pipeline_format.shader_stages = m_pipeline_cache->shader_stages(
                vierkant::ShaderType::UNLIT_COLOR);
    }

    // grid
    {
        // unit grid
        auto geom = vierkant::Geometry::Grid();
        geom->tex_coords.clear();
        drawable_params.mesh = vierkant::Mesh::create_from_geometry(m_device, geom, {});
        m_drawable_grid = vierkant::create_drawables(drawable_params).front();
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
        drawable_params.mesh = vierkant::Mesh::create_from_geometry(m_device, box, {});
        auto &mat = drawable_params.mesh->materials.front();
        mat->depth_write = false;
        mat->depth_test = true;
        mat->cull_mode = VK_CULL_MODE_FRONT_BIT;
        mat->textures[vierkant::Material::TextureType::Environment] = {};
        m_drawable_skybox = vierkant::create_drawables(drawable_params).front();
    }

//    // memorypool with 128MB blocks
//    constexpr size_t block_size = 1U << 27U;
//    constexpr size_t min_num_blocks = 0, max_num_blocks = 0;
//    m_memory_pool = vierkant::Buffer::create_pool(m_device,
//                                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
//                                                  VMA_MEMORY_USAGE_CPU_TO_GPU,
//                                                  block_size, min_num_blocks, max_num_blocks,
//                                                  VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_mesh(vierkant::Renderer &renderer, const vierkant::MeshPtr &mesh, const glm::mat4 &model_view,
                            const glm::mat4 &projection, vierkant::ShaderType shader_type)
{
    vierkant::create_drawables_params_t drawable_params = {};
    drawable_params.mesh = mesh;
    auto drawables = vierkant::create_drawables(drawable_params);

    for(auto &drawable : drawables)
    {
        drawable.pipeline_format.shader_stages = m_pipeline_cache->shader_stages(shader_type);
        drawable.matrices.modelview = model_view * drawable.matrices.modelview;
        drawable.matrices.projection = projection;
        renderer.stage_drawable(std::move(drawable));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_node_hierarchy(vierkant::Renderer &renderer,
                                      const vierkant::nodes::NodeConstPtr &root_node,
                                      const vierkant::nodes::node_animation_t &animation,
                                      float animation_time,
                                      const glm::mat4 &model_view,
                                      const glm::mat4 &projection)
{
    if(!root_node){ return; }

    // create line-list
    std::vector<glm::vec3> lines;
    lines.reserve(256);

    std::deque<std::pair<vierkant::nodes::NodeConstPtr, glm::mat4>> node_queue;
    node_queue.emplace_back(root_node, glm::mat4(1));

    while(!node_queue.empty())
    {
        auto[node, joint_transform] = node_queue.front();
        node_queue.pop_front();

        glm::mat4 node_transform = node->transform;

        auto it = animation.keys.find(node);

        if(it != animation.keys.end())
        {
            const auto &animation_keys = it->second;
            create_animation_transform(animation_keys, animation_time, animation.interpolation_mode,
                                       node_transform);
        }
        joint_transform = joint_transform * node_transform;

        // queue all children
        for(auto &child_node : node->children)
        {
            // draw line from current to child
            auto child_transform = joint_transform * child_node->transform;
            lines.push_back(joint_transform[3].xyz());
            lines.push_back(child_transform[3].xyz());

            node_queue.emplace_back(child_node, joint_transform);
        }
    }

    draw_lines(renderer, lines, glm::vec4(1, 0, 0, 1), model_view, projection);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_text(vierkant::Renderer &renderer, const std::string &text, const FontPtr &font,
                            const glm::vec2 &pos, const glm::vec4 &color)
{
    auto mesh = font->create_mesh(text, color);
    const auto &entry = mesh->entries.front();
    const auto &lod_0 = entry.lods.front();

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
    drawable.descriptors[vierkant::Renderer::BINDING_TEXTURES].images = {font->glyph_texture()};
    drawable.num_indices = lod_0.num_indices;
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
    drawable.matrices.projection = glm::orthoRH(-1.f, 1.0f, -1.f, 1.0f, 0.0f, 1.0f);
    drawable.matrices.projection[1][1] *= -1;
    drawable.matrices.modelview = glm::scale(glm::mat4(1), glm::vec3(scale, 1));
    drawable.matrices.modelview[3] = glm::vec4(area.x / renderer.viewport.width, -area.y / renderer.viewport.height, 0,
                                               1);

    // set image
    drawable.descriptors[vierkant::Renderer::BINDING_TEXTURES].images = {image};

    // stage image drawable
    renderer.stage_drawable(std::move(drawable));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_lines(vierkant::Renderer &renderer,
                             const std::vector<glm::vec3> &lines,
                             const glm::vec4 &color,
                             const glm::mat4 &model_view,
                             const glm::mat4 &projection)
{
    // search drawable
    auto drawable_it = m_drawables.find(DrawableType::Lines);

    if(drawable_it == m_drawables.end())
    {
        vierkant::drawable_t drawable = {};

        auto &fmt = drawable.pipeline_format;
        fmt.blend_state.blendEnable = true;
        fmt.depth_test = false;
        fmt.depth_write = false;
        fmt.shader_stages = m_pipeline_cache->shader_stages(vierkant::ShaderType::UNLIT);
        fmt.primitive_topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

        // descriptors
        vierkant::descriptor_t desc_matrix = {};
        desc_matrix.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_matrix.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        drawable.descriptors[vierkant::Renderer::BINDING_MESH_DRAWS] = desc_matrix;

        vierkant::descriptor_t desc_material = {};
        desc_material.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_material.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        drawable.descriptors[vierkant::Renderer::BINDING_MATERIAL] = desc_material;

        auto mesh = vierkant::Mesh::create();

        // vertex attrib -> position
        vierkant::vertex_attrib_t position_attrib;
        position_attrib.offset = 0;
        position_attrib.stride = sizeof(glm::vec3);
        position_attrib.buffer = nullptr;
        position_attrib.buffer_offset = 0;
        position_attrib.format = vierkant::format<glm::vec3>();
        mesh->vertex_attribs[vierkant::Mesh::ATTRIB_POSITION] = position_attrib;

        drawable.mesh = mesh;

        drawable_it = m_drawables.insert({DrawableType::Lines, std::move(drawable)}).first;
    }

    auto drawable = drawable_it->second;

    auto mesh = vierkant::Mesh::create();
    mesh->vertex_attribs = drawable.mesh->vertex_attribs;

    auto &position_attrib = mesh->vertex_attribs.at(vierkant::Mesh::ATTRIB_POSITION);
    position_attrib.buffer = vierkant::Buffer::create(renderer.device(), lines,
                                                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                      VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                      m_memory_pool);

    drawable.mesh = mesh;
    drawable.pipeline_format.attribute_descriptions = mesh->attribute_descriptions();
    drawable.pipeline_format.binding_descriptions = mesh->binding_descriptions();
    drawable.num_vertices = lines.size();

    // line color via material
    drawable.material.color = color;

    drawable.matrices.modelview = model_view;
    drawable.matrices.projection = projection;
    renderer.stage_drawable(std::move(drawable));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_image_fullscreen(Renderer &renderer, const ImagePtr &image, const vierkant::ImagePtr &depth,
                                        bool depth_test)
{
    // create image-drawable
    vierkant::drawable_t drawable;

    if(image && depth)
    {
        // set image + depth
        drawable = m_drawable_color_depth_fullscreen;
        drawable.pipeline_format.depth_test = depth_test;
        drawable.descriptors[0].images = {image, depth};
    }
    else if(image)
    {
        // set image
        drawable = m_drawable_image_fullscreen;
        drawable.pipeline_format.depth_test = depth_test;
        drawable.descriptors[0].images = {image};
    }
    drawable.pipeline_format.scissor.extent.width = static_cast<uint32_t>(renderer.viewport.width);
    drawable.pipeline_format.scissor.extent.height = static_cast<uint32_t>(renderer.viewport.height);

    // stage image drawable
    renderer.stage_drawable(std::move(drawable));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_grid(vierkant::Renderer &renderer,
                            float scale,
                            uint32_t /*num_subs*/,
                            const glm::mat4 &model_view,
                            const glm::mat4 &projection)
{
    // TODO: map-lookup for requested num-subdivisions
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
    drawable.descriptors[vierkant::Renderer::BINDING_TEXTURES].images = {environment};
    drawable.pipeline_format.shader_stages = m_pipeline_cache->shader_stages(vierkant::ShaderType::UNLIT_CUBE);

    renderer.stage_drawable(drawable);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

}