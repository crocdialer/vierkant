//
// Created by crocdialer on 3/2/19.
//

#pragma once

#include "vierkant/Renderer.hpp"
#include "vierkant/Scene.hpp"
#include "vierkant/Camera.hpp"
#include "vierkant/Font.hpp"

namespace vierkant
{

class DrawContext
{
public:

    DrawContext() = default;

    DrawContext(const DrawContext &) = delete;

    explicit DrawContext(vierkant::DevicePtr device);

    /**
     * @brief   Draws text in a 2D context.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   text        a provided text. can contain '\n' line-breaks.
     * @param   font        a provided vierkant::Font.
     * @param   pos         the desired position for the text's origin (top-left corner).
     * @param   color       the desired color for the text.
     */
    void draw_text(vierkant::Renderer &renderer, const std::string &text, const FontPtr &font, const glm::vec2 &pos,
                   const glm::vec4 &color = glm::vec4(1));

    /**
     * @brief   Draws an image in a 2D context.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   image       a provided vierkant::Image, assumed to contain a sampler2D.
     * @param   area        the desired area to cover.
     */
    void draw_image(vierkant::Renderer &renderer, const vierkant::ImagePtr &image,
                    const crocore::Area_<int> &area = {});

    /**
     * @brief   Draws a set of lines.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   lines       a set of lines.
     */
    void draw_lines(vierkant::Renderer &renderer,
                    const std::vector<glm::vec3> &lines,
                    const glm::vec4 &color,
                    const glm::mat4 &model_view,
                    const glm::mat4 &projection);

    /**
     * @brief   Draws an image in a 2D context.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   image       a provided vierkant::Image, assumed to contain a sampler2D with VK_ASPECT_COLOR.
     * @param   depth       an optional vierkant::Image, assumed to contain a sampler2D with VK_ASPECT_DEPTH.
     * @param   depth_test  an optional flag to enable depth-testing. only used when a deph-buffer is provided.
     */
    void draw_image_fullscreen(vierkant::Renderer &renderer,
                               const vierkant::ImagePtr &image,
                               const vierkant::ImagePtr &depth = nullptr,
                               bool depth_test = false);

    /**
     * @brief   Draws an axis-aligned bounding box.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   aabb        a provided vierkant::AABB.
     * @param   model_view  the modelview matrix to use for drawing.
     * @param   projection  the projection matrix to use for drawing.
     */
    void draw_boundingbox(vierkant::Renderer &renderer, const vierkant::AABB &aabb, const glm::mat4 &model_view,
                          const glm::mat4 &projection);

    /**
     * @brief   Draws a grid of lines in the xz-plane.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   scale       the desired scaling-value.
     * @param   num_subs    the number of subdivisions.
     * @param   model_view  the modelview matrix to use for drawing.
     * @param   projection  the projection matrix to use for drawing.
     */
    void draw_grid(vierkant::Renderer &renderer, float scale, uint32_t num_subs, const glm::mat4 &model_view,
                   const glm::mat4 &projection);

    /**
     * @brief   Draws a mesh.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   mesh        a provided vierkant::MeshPtr.
     * @param   model_view  the modelview matrix to use for drawing.
     * @param   projection  the projection matrix to use for drawing.
     * @param   shader_type the desired vierkant::ShaderType.
     */
    void draw_mesh(vierkant::Renderer &renderer, const vierkant::MeshPtr &mesh, const glm::mat4 &model_view,
                   const glm::mat4 &projection, vierkant::ShaderType shader_type);

    /**
     * @brief   Draws a node hierarchy as set of lines.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   node        a provided vierkant::nodes::NodeConstPtr.
     * @param   animation   an optional vierkant::nodes::node_animation_t
     * @param   model_view  the modelview matrix to use for drawing.
     * @param   projection  the projection matrix to use for drawing.
     */
    void draw_node_hierarchy(vierkant::Renderer &renderer,
                             const vierkant::nodes::NodeConstPtr &node,
                             const vierkant::nodes::node_animation_t &animation,
                             const glm::mat4 &model_view,
                             const glm::mat4 &projection);

    /**
     * @brief   Render a skybox.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   environment a provided vierkant::Image, assumed to contain a samplerCube.
     * @param   cam         a vierkant::Camera
     */
    void draw_skybox(vierkant::Renderer &renderer, const vierkant::ImagePtr &environment,
                     const vierkant::CameraPtr &cam);

private:

    vierkant::DevicePtr m_device;

    enum class DrawableType
    {
        Points,
        Lines,
        Text,
        Image,
        ImageFullscreen,
        ImageFullscreenDepth,
        AABB,
        Grid,
        Skybox
    };

    std::unordered_map<DrawableType, Renderer::drawable_t> m_drawables;

    Renderer::drawable_t m_drawable_text = {};

    Renderer::drawable_t m_drawable_image = {};

    Renderer::drawable_t m_drawable_image_fullscreen = {};

    Renderer::drawable_t m_drawable_color_depth_fullscreen = {};

    Renderer::drawable_t m_drawable_aabb = {};

    Renderer::drawable_t m_drawable_grid = {};

    Renderer::drawable_t m_drawable_skybox = {};

    vierkant::PipelineCachePtr m_pipeline_cache;

    vierkant::VmaPoolPtr m_memory_pool;
};

}