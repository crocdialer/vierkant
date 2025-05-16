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

        vierkant::Mesh::create_info_t mesh_info = {};
        mesh_info.mesh_buffer_params.use_vertex_colors = true;
        auto mesh = Mesh::create_from_geometry(m_device, plane, mesh_info);
        const auto &entry = mesh->entries.front();
        const auto &lod = entry.lods.front();

        graphics_pipeline_info_t fmt = {};
        fmt.blend_state.blendEnable = true;
        fmt.depth_test = false;
        fmt.depth_write = false;
        fmt.shader_stages = m_pipeline_cache->shader_stages(vierkant::ShaderType::UNLIT_TEXTURE);
        fmt.binding_descriptions = vierkant::create_binding_descriptions(mesh->vertex_attribs);
        fmt.attribute_descriptions = vierkant::create_attribute_descriptions(mesh->vertex_attribs);
        fmt.primitive_topology = entry.primitive_type;

        // descriptors
        vierkant::descriptor_t desc_matrix = {};
        desc_matrix.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_matrix.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        m_drawable_image.descriptors[vierkant::Rasterizer::BINDING_MESH_DRAWS] = desc_matrix;

        vierkant::descriptor_t desc_material = {};
        desc_material.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_material.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        m_drawable_image.descriptors[vierkant::Rasterizer::BINDING_MATERIAL] = desc_material;

        vierkant::descriptor_t desc_texture = {};
        desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        m_drawable_image.descriptors[vierkant::Rasterizer::BINDING_TEXTURES] = desc_texture;

        m_drawable_image.mesh = mesh;
        m_drawable_image.num_indices = lod.num_indices;
        m_drawable_image.pipeline_format = fmt;
    }

    graphics_pipeline_info_t fmt = {};
    fmt.blend_state.blendEnable = true;
    fmt.depth_test = false;
    fmt.depth_write = false;
    fmt.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    fmt.dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    drawable_t drawable_fullscreen = {};
    drawable_fullscreen.num_vertices = 3;
    drawable_fullscreen.pipeline_format = fmt;
    drawable_fullscreen.use_own_buffers = true;

    // fullscreen
    {
        m_drawable_image_fullscreen = drawable_fullscreen;
        m_drawable_image_fullscreen.pipeline_format.shader_stages =
                m_pipeline_cache->shader_stages(vierkant::ShaderType::FULLSCREEN_TEXTURE);

        // descriptors
        vierkant::descriptor_t &desc_texture = m_drawable_image_fullscreen.descriptors[0];
        desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        vierkant::descriptor_t &desc_color = m_drawable_image_fullscreen.descriptors[1];
        desc_color.type = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
        desc_color.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        m_drawable_color_depth_fullscreen = m_drawable_image_fullscreen;
        m_drawable_color_depth_fullscreen.pipeline_format.depth_test = false;
        m_drawable_color_depth_fullscreen.pipeline_format.depth_write = true;
        m_drawable_color_depth_fullscreen.pipeline_format.shader_stages =
                m_pipeline_cache->shader_stages(vierkant::ShaderType::FULLSCREEN_TEXTURE_DEPTH);
    }

    vierkant::create_drawables_params_t drawable_params = {};

    // request vertex-colors
    vierkant::Mesh::create_info_t mesh_create_info = {};
    mesh_create_info.mesh_buffer_params.use_vertex_colors = true;

    // aabb
    {
        // unit cube
        auto geom = vierkant::Geometry::BoxOutline();
        m_drawable_aabb =
                vierkant::create_drawables({vierkant::Mesh::create_from_geometry(m_device, geom, mesh_create_info)},
                                           drawable_params)
                        .front();
        m_drawable_aabb.pipeline_format.shader_stages =
                m_pipeline_cache->shader_stages(vierkant::ShaderType::UNLIT_COLOR);
    }

    // grid
    {
        m_drawable_grid = drawable_fullscreen;
        m_drawable_grid.pipeline_format.depth_test = true;
        m_drawable_grid.pipeline_format.depth_write = true;
        m_drawable_grid.pipeline_format.shader_stages =
                m_pipeline_cache->shader_stages(vierkant::ShaderType::FULLSCREEN_GRID);

        vierkant::descriptor_t &desc_camera = m_drawable_grid.descriptors[0];
        desc_camera.type = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
        desc_camera.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    // skybox
    {
        auto box = vierkant::Geometry::Box();
        box->colors.clear();
        box->tex_coords.clear();
        box->tangents.clear();
        box->normals.clear();
        vierkant::mesh_component_t mesh_component = {vierkant::Mesh::create_from_geometry(m_device, box, {})};
        m_drawable_skybox = vierkant::create_drawables(mesh_component, drawable_params).front();
        m_drawable_skybox.pipeline_format.depth_write = false;
        m_drawable_skybox.pipeline_format.depth_test = true;
        m_drawable_skybox.pipeline_format.cull_mode = VK_CULL_MODE_FRONT_BIT;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_mesh(vierkant::Rasterizer &renderer, const vierkant::MeshPtr &mesh,
                            const vierkant::transform_t &transform, const glm::mat4 &projection,
                            vierkant::ShaderType shader_type, const std::optional<glm::vec4> &color, bool depth_test,
                            bool depth_write)
{
    vierkant::create_drawables_params_t drawable_params = {};
    auto drawables = vierkant::create_drawables({mesh}, drawable_params);

    for(auto &drawable: drawables)
    {
        if(color) { drawable.material.color = *color; }
        drawable.share_material = false;
        drawable.pipeline_format.depth_test = depth_test;
        drawable.pipeline_format.depth_write = depth_write;
        drawable.pipeline_format.shader_stages = m_pipeline_cache->shader_stages(shader_type);
        drawable.matrices.transform = transform * drawable.matrices.transform;
        drawable.matrices.projection = projection;
        renderer.stage_drawable(std::move(drawable));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_node_hierarchy(vierkant::Rasterizer &renderer, const vierkant::nodes::NodeConstPtr &root_node,
                                      const vierkant::nodes::node_animation_t &animation, float animation_time,
                                      const vierkant::transform_t &transform, const glm::mat4 &projection)
{
    if(!root_node) { return; }

    // create line-list
    std::vector<glm::vec3> lines;
    lines.reserve(256);

    std::deque<std::pair<vierkant::nodes::NodeConstPtr, vierkant::transform_t>> node_queue;
    node_queue.emplace_back(root_node, vierkant::transform_t());

    while(!node_queue.empty())
    {
        auto [node, joint_transform] = node_queue.front();
        node_queue.pop_front();

        auto node_transform = node->transform;
        auto it = animation.keys.find(node);

        if(it != animation.keys.end())
        {
            const auto &animation_keys = it->second;
            create_animation_transform(animation_keys, animation_time, animation.interpolation_mode, node_transform);
        }
        joint_transform = joint_transform * node_transform;

        // queue all children
        for(auto &child_node: node->children)
        {
            // draw line from current to child
            auto child_transform = joint_transform * child_node->transform;
            lines.push_back(joint_transform.translation);
            lines.push_back(child_transform.translation);

            node_queue.emplace_back(child_node, joint_transform);
        }
    }

    draw_lines(renderer, lines, glm::vec4(1, 0, 0, 1), transform, projection);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_text(vierkant::Rasterizer &renderer, const std::string &text, const FontPtr &font,
                            const glm::vec2 &pos, const glm::vec4 &color)
{
    auto mesh = font->create_mesh(text, color);
    const auto &entry = mesh->entries.front();
    const auto &lod_0 = entry.lods.front();

    // search drawable
    auto drawable_it = m_drawables.find(DrawableType::Text);

    if(drawable_it == m_drawables.end())
    {
        vierkant::drawable_t drawable = {};

        // pipeline format
        vierkant::graphics_pipeline_info_t pipeline_fmt = {};
        pipeline_fmt.shader_stages = m_pipeline_cache->shader_stages(vierkant::ShaderType::UNLIT_TEXTURE);
        pipeline_fmt.depth_write = false;
        pipeline_fmt.depth_test = false;
        pipeline_fmt.blend_state.blendEnable = true;
        pipeline_fmt.cull_mode = VK_CULL_MODE_BACK_BIT;
        pipeline_fmt.dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT};
        drawable.pipeline_format = std::move(pipeline_fmt);
        drawable_it = m_drawables.insert({DrawableType::Text, std::move(drawable)}).first;
    }

    auto drawable = drawable_it->second;
    drawable.pipeline_format.attribute_descriptions = vierkant::create_attribute_descriptions(mesh->vertex_attribs);
    drawable.pipeline_format.binding_descriptions = vierkant::create_binding_descriptions(mesh->vertex_attribs);

    drawable.mesh = mesh;
    drawable.matrices.projection =
            glm::orthoRH(0.f, renderer.viewport.width, 0.f, renderer.viewport.height, 0.0f, 1.0f);
    drawable.matrices.transform.translation = {pos.x, pos.y, 0};
    drawable.descriptors[vierkant::Rasterizer::BINDING_TEXTURES].images = {font->glyph_texture()};
    drawable.num_indices = lod_0.num_indices;
    drawable.num_vertices = entry.num_vertices;
    renderer.stage_drawable(std::move(drawable));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_image(vierkant::Rasterizer &renderer, const vierkant::ImagePtr &image,
                             const crocore::Area_<int> &area, const glm::vec4 &color)
{
    if(!image) { return; }
    float w = area.width ? static_cast<float>(area.width) : renderer.viewport.width;
    float h = area.height ? static_cast<float>(area.height) : renderer.viewport.height;
    glm::vec2 scale = glm::vec2(w, h) / glm::vec2(renderer.viewport.width, renderer.viewport.height);

    // copy image-drawable
    auto drawable = m_drawable_image;
    drawable.matrices.projection = glm::orthoRH(-1.f, 1.0f, -1.f, 1.0f, 0.0f, 1.0f);
    drawable.matrices.projection[1][1] *= -1;
    drawable.matrices.transform.scale = glm::vec3(scale, 1);
    drawable.matrices.transform.translation = glm::vec3(static_cast<float>(area.x) / renderer.viewport.width,
                                                        static_cast<float>(-area.y) / renderer.viewport.height, 0);

    // color-tint
    drawable.material.color = color;

    // set image
    drawable.descriptors[vierkant::Rasterizer::BINDING_TEXTURES].images = {image};

    // stage image drawable
    renderer.stage_drawable(std::move(drawable));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_lines(vierkant::Rasterizer &renderer, const std::vector<glm::vec3> &lines,
                             const glm::vec4 &color, const vierkant::transform_t &transform,
                             const glm::mat4 &projection)
{
    if(lines.empty()) { return; }

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
        drawable.descriptors[vierkant::Rasterizer::BINDING_MESH_DRAWS] = desc_matrix;

        vierkant::descriptor_t desc_material = {};
        desc_material.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_material.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        drawable.descriptors[vierkant::Rasterizer::BINDING_MATERIAL] = desc_material;

        auto mesh = vierkant::Mesh::create();

        // vertex attrib -> position
        auto &position_attrib = mesh->vertex_attribs[vierkant::Mesh::ATTRIB_POSITION];
        position_attrib.offset = 0;
        position_attrib.stride = sizeof(glm::vec3);
        position_attrib.buffer = nullptr;
        position_attrib.buffer_offset = 0;
        position_attrib.format = vierkant::format<glm::vec3>();

        drawable.mesh = mesh;

        drawable_it = m_drawables.insert({DrawableType::Lines, std::move(drawable)}).first;
    }

    auto drawable = drawable_it->second;

    auto mesh = vierkant::Mesh::create();
    mesh->vertex_attribs = drawable.mesh->vertex_attribs;

    auto &position_attrib = mesh->vertex_attribs.at(vierkant::Mesh::ATTRIB_POSITION);
    position_attrib.buffer = vierkant::Buffer::create(renderer.device(), lines, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                      VMA_MEMORY_USAGE_CPU_TO_GPU, m_memory_pool);

    drawable.mesh = mesh;
    drawable.pipeline_format.attribute_descriptions = vierkant::create_attribute_descriptions(mesh->vertex_attribs);
    drawable.pipeline_format.binding_descriptions = vierkant::create_binding_descriptions(mesh->vertex_attribs);
    drawable.num_vertices = lines.size();

    // line color via material
    drawable.material.color = color;

    drawable.matrices.transform = transform;
    drawable.matrices.projection = projection;
    renderer.stage_drawable(std::move(drawable));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_lines(vierkant::Rasterizer &renderer, const std::vector<glm::vec3> &lines,
                             const std::vector<glm::vec4> &colors, const vierkant::transform_t &transform,
                             const glm::mat4 &projection)
{
    if(lines.empty() || lines.size() != colors.size()) { return; }

    // search drawable
    auto drawable_it = m_drawables.find(DrawableType::LinesColor);

    if(drawable_it == m_drawables.end())
    {
        vierkant::drawable_t drawable = {};

        auto &fmt = drawable.pipeline_format;
        fmt.blend_state.blendEnable = true;
        fmt.depth_test = false;
        fmt.depth_write = false;
        fmt.shader_stages = m_pipeline_cache->shader_stages(vierkant::ShaderType::UNLIT_COLOR);
        fmt.primitive_topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

        // descriptors
        vierkant::descriptor_t desc_matrix = {};
        desc_matrix.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_matrix.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        drawable.descriptors[vierkant::Rasterizer::BINDING_MESH_DRAWS] = desc_matrix;

        vierkant::descriptor_t desc_material = {};
        desc_material.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_material.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        drawable.descriptors[vierkant::Rasterizer::BINDING_MATERIAL] = desc_material;

        auto mesh = vierkant::Mesh::create();

        // vertex attrib -> position
        auto &position_attrib = mesh->vertex_attribs[vierkant::Mesh::ATTRIB_POSITION];
        position_attrib.offset = 0;
        position_attrib.stride = sizeof(glm::vec3);
        position_attrib.buffer = nullptr;
        position_attrib.buffer_offset = 0;
        position_attrib.format = vierkant::format<glm::vec3>();

        // vertex attrib -> colors
        auto &color_attrib = mesh->vertex_attribs[vierkant::Mesh::ATTRIB_COLOR];
        color_attrib.offset = 0;
        color_attrib.stride = sizeof(glm::vec4);
        color_attrib.buffer = nullptr;
        color_attrib.buffer_offset = 0;
        color_attrib.format = vierkant::format<glm::vec4>();

        drawable.mesh = mesh;
        drawable_it = m_drawables.insert({DrawableType::LinesColor, std::move(drawable)}).first;
    }
    auto drawable = drawable_it->second;
    auto mesh = vierkant::Mesh::create();
    mesh->vertex_attribs = drawable.mesh->vertex_attribs;

    auto &position_attrib = mesh->vertex_attribs.at(vierkant::Mesh::ATTRIB_POSITION);
    position_attrib.buffer = vierkant::Buffer::create(renderer.device(), lines, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                      VMA_MEMORY_USAGE_CPU_TO_GPU, m_memory_pool);
    auto &color_attrib = mesh->vertex_attribs.at(vierkant::Mesh::ATTRIB_COLOR);
    color_attrib.buffer = vierkant::Buffer::create(renderer.device(), colors, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                   VMA_MEMORY_USAGE_CPU_TO_GPU, m_memory_pool);
    drawable.mesh = mesh;
    drawable.share_material = false;
    drawable.pipeline_format.attribute_descriptions = vierkant::create_attribute_descriptions(mesh->vertex_attribs);
    drawable.pipeline_format.binding_descriptions = vierkant::create_binding_descriptions(mesh->vertex_attribs);
    drawable.num_vertices = lines.size();
    drawable.matrices.transform = transform;
    drawable.matrices.projection = projection;
    renderer.stage_drawable(std::move(drawable));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_image_fullscreen(Rasterizer &renderer, const ImagePtr &image, const vierkant::ImagePtr &depth,
                                        bool depth_test, bool blend, const glm::vec4 &color, float depth_bias,
                                        float depth_scale)
{
    if(!image) { return; }

    // create image-drawable
    vierkant::drawable_t drawable;

    if(depth)
    {
        // set image + depth
        drawable = m_drawable_color_depth_fullscreen;
        drawable.descriptors[0].images = {image, depth};
    }
    else
    {
        drawable = m_drawable_image_fullscreen;
        drawable.descriptors[0].images = {image};
    }
    struct params_t
    {
        glm::vec4 color = glm::vec4(1.f);
        float depth_scale = 1.f;
        float depth_bias = 0.f;
    };
    drawable.descriptors[1].inline_uniform_block.resize(sizeof(params_t));
    auto params = reinterpret_cast<params_t *>(drawable.descriptors[1].inline_uniform_block.data());
    *params = {};
    params->color = color;
    params->depth_bias = depth_bias;
    params->depth_scale = depth_scale;

    drawable.pipeline_format.depth_test = depth_test;
    drawable.pipeline_format.blend_state.blendEnable = blend;
    drawable.pipeline_format.scissor.extent.width = static_cast<uint32_t>(renderer.viewport.width);
    drawable.pipeline_format.scissor.extent.height = static_cast<uint32_t>(renderer.viewport.height);

    // stage image drawable
    renderer.stage_drawable(std::move(drawable));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_grid(vierkant::Rasterizer &renderer, const glm::vec4 &color, float spacing,
                            const glm::vec2 &line_width, bool ortho, const vierkant::transform_t &transform,
                            const glm::mat4 &projection)
{
    auto drawable = m_drawable_grid;
    drawable.pipeline_format.scissor.extent.width = static_cast<uint32_t>(renderer.viewport.width);
    drawable.pipeline_format.scissor.extent.height = static_cast<uint32_t>(renderer.viewport.height);

    struct grid_params_t
    {
        glm::mat4 projection_view;
        glm::mat4 projection_inverse;
        glm::mat4 view_inverse;
        glm::vec4 plane = glm::vec4(0, 1, 0, 0);
        glm::vec4 color = glm::vec4(1.f);
        glm::vec2 line_width = glm::vec2(0.05f);
        float spacing = 1.f;
        VkBool32 ortho = false;
    };
    drawable.descriptors[0].inline_uniform_block.resize(sizeof(grid_params_t));
    auto &grid_params = *reinterpret_cast<grid_params_t *>(drawable.descriptors[0].inline_uniform_block.data());
    grid_params = {};
    auto transform_mat = vierkant::mat4_cast(transform);
    grid_params.projection_view = projection * transform_mat;
    grid_params.projection_inverse = glm::inverse(projection);
    grid_params.view_inverse = vierkant::mat4_cast(vierkant::inverse(transform));
    grid_params.color = color;
    grid_params.spacing = spacing;
    grid_params.line_width = glm::clamp(line_width, glm::vec2(0.f), glm::vec2(1.f));
    grid_params.ortho = ortho;

    // adjust plane to cardinal axis
    if(ortho)
    {
        auto eye = transform_mat[2].xyz();
        float max_v = 0.f;
        int32_t max_index = 0;
        for(int32_t i = 0; i < 3; ++i)
        {
            if(std::abs(eye[i]) > max_v)
            {
                max_index = i;
                max_v = std::abs(eye[i]);
            }
        }
        for(int32_t i = 0; i < 3; ++i) { grid_params.plane[i] = i == max_index ? 1.f : 0.f; }
    }
    renderer.stage_drawable(std::move(drawable));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_boundingbox(vierkant::Rasterizer &renderer, const vierkant::AABB &aabb,
                                   const vierkant::transform_t &transform, const glm::mat4 &projection)
{
    auto drawable = m_drawable_aabb;
    vierkant::transform_t t = {};
    t.translation = aabb.center();
    t.scale = {aabb.width(), aabb.height(), aabb.depth()};
    drawable.matrices.transform = transform * t;
    drawable.matrices.projection = projection;
    renderer.stage_drawable(std::move(drawable));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DrawContext::draw_skybox(vierkant::Rasterizer &renderer, const vierkant::ImagePtr &environment,
                              const vierkant::CameraPtr &cam)
{
    vierkant::transform_t t = {};
    t.rotation = cam->view_transform().rotation;
    t.scale = glm::vec3(cam->far() * .99f);

    auto drawable = m_drawable_skybox;
    drawable.matrices.transform = t;
    drawable.matrices.projection = cam->projection_matrix();
    drawable.descriptors[vierkant::Rasterizer::BINDING_TEXTURES].images = {environment};
    drawable.pipeline_format.shader_stages = m_pipeline_cache->shader_stages(vierkant::ShaderType::UNLIT_CUBE);

    renderer.stage_drawable(drawable);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

}// namespace vierkant