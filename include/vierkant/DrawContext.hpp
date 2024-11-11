//
// Created by crocdialer on 3/2/19.
//

#pragma once

#include "vierkant/Camera.hpp"
#include "vierkant/Font.hpp"
#include "vierkant/Rasterizer.hpp"
#include "vierkant/Scene.hpp"

namespace vierkant
{

class DrawContext
{
public:
    DrawContext() = default;

    DrawContext(const DrawContext &) = delete;

    DrawContext(DrawContext &&) = default;

    explicit DrawContext(vierkant::DevicePtr device);

    DrawContext &operator=(DrawContext &&) = default;

    /**
     * @brief   Draws text in a 2D context.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   text        a provided text. can contain '\n' line-breaks.
     * @param   font        a provided vierkant::Font.
     * @param   pos         the desired position for the text's origin (top-left corner).
     * @param   color       the desired color for the text.
     */
    void draw_text(vierkant::Rasterizer &renderer, const std::string &text, const FontPtr &font, const glm::vec2 &pos,
                   const glm::vec4 &color = glm::vec4(1));

    /**
     * @brief   Draws an image in a 2D context.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   image       a provided vierkant::Image, assumed to contain a sampler2D.
     * @param   area        the desired area to cover.
     */
    void draw_image(vierkant::Rasterizer &renderer, const vierkant::ImagePtr &image,
                    const crocore::Area_<int> &area = {}, const glm::vec4 &color = glm::vec4(1.f));

    /**
     * @brief   Draws a set of lines.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   lines       a set of lines.
     */
    void draw_lines(vierkant::Rasterizer &renderer, const std::vector<glm::vec3> &lines, const glm::vec4 &color,
                    const vierkant::transform_t &transform, const glm::mat4 &projection);

    void draw_lines(vierkant::Rasterizer &renderer, const std::vector<glm::vec3> &lines,
                    const std::vector<glm::vec4> &colors, const vierkant::transform_t &transform,
                    const glm::mat4 &projection);

    void draw_geometry(vierkant::Rasterizer &renderer, const vierkant::GeometryConstPtr &geom, const glm::vec4 &color,
                       const vierkant::transform_t &transform, const glm::mat4 &projection);
    /**
     * @brief   Draws an image in a 2D context.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   image       a provided vierkant::Image, assumed to contain a sampler2D with VK_ASPECT_COLOR.
     * @param   depth       an optional vierkant::Image, assumed to contain a sampler2D with VK_ASPECT_DEPTH.
     * @param   depth_test  an optional flag to enable depth-testing. only used when a deph-buffer is provided.
     * @param   blend       an optional flag to enable alpha-blending.
     */
    void draw_image_fullscreen(vierkant::Rasterizer &renderer, const vierkant::ImagePtr &image,
                               const vierkant::ImagePtr &depth = nullptr, bool depth_test = false, bool blend = true);

    /**
     * @brief   Draws an axis-aligned bounding box.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   aabb        a provided vierkant::AABB.
     * @param   transform   a modelview transform
     * @param   projection  the projection matrix to use for drawing.
     */
    void draw_boundingbox(vierkant::Rasterizer &renderer, const vierkant::AABB &aabb,
                          const vierkant::transform_t &transform, const glm::mat4 &projection);

    /**
     * @brief   Draws a grid of lines in the xz-plane.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   scale       the desired scaling-value.
     * @param   num_subs    the number of subdivisions.
     * @param   transform   a modelview transform
     * @param   projection  the projection matrix to use for drawing.
     */
    void draw_grid(vierkant::Rasterizer &renderer, float scale, uint32_t num_subs,
                   const vierkant::transform_t &transform, const glm::mat4 &projection);

    /**
     * @brief   Draws a mesh.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   mesh        a provided vierkant::MeshPtr.
     * @param   transform   a modelview transform
     * @param   projection  the projection matrix to use for drawing.
     * @param   shader_type the desired vierkant::ShaderType.
     */
    void draw_mesh(vierkant::Rasterizer &renderer, const vierkant::MeshPtr &mesh,
                   const vierkant::transform_t &transform, const glm::mat4 &projection,
                   vierkant::ShaderType shader_type);

    /**
     * @brief   Draws a node hierarchy as set of lines.
     *
     * @param   renderer        a provided vierkant::Renderer.
     * @param   node            a provided vierkant::nodes::NodeConstPtr.
     * @param   animation       an optional vierkant::nodes::node_animation_t
     * @param   animation_time  current animation-time
     * @param   transform   a modelview transform
     * @param   projection      the projection matrix to use for drawing.
     */
    void draw_node_hierarchy(vierkant::Rasterizer &renderer, const vierkant::nodes::NodeConstPtr &node,
                             const vierkant::nodes::node_animation_t &animation, float animation_time,
                             const vierkant::transform_t &transform, const glm::mat4 &projection);

    /**
     * @brief   Render a skybox.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   environment a provided vierkant::Image, assumed to contain a samplerCube.
     * @param   cam         a vierkant::Camera
     */
    void draw_skybox(vierkant::Rasterizer &renderer, const vierkant::ImagePtr &environment,
                     const vierkant::CameraPtr &cam);

private:
    vierkant::DevicePtr m_device;

    enum class DrawableType
    {
        Points,
        Lines,
        LinesColor,
        TrianglesColor,
        Text,
        Image,
        ImageFullscreen,
        ImageFullscreenDepth,
        AABB,
        Grid,
        Skybox
    };

    std::unordered_map<DrawableType, vierkant::drawable_t> m_drawables;

    vierkant::drawable_t m_drawable_image = {};

    vierkant::drawable_t m_drawable_image_fullscreen = {};

    vierkant::drawable_t m_drawable_color_depth_fullscreen = {};

    vierkant::drawable_t m_drawable_aabb = {};

    vierkant::drawable_t m_drawable_grid = {};

    vierkant::drawable_t m_drawable_skybox = {};

    vierkant::PipelineCachePtr m_pipeline_cache;

    vierkant::VmaPoolPtr m_memory_pool;
};

}// namespace vierkant