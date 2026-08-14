#include <spdlog/spdlog.h>
#include <vierkant/Camera.hpp>
#include <vierkant/Visitor.hpp>
#include <vierkant/projection.hpp>

namespace vierkant
{

glm::vec2 clipping_distances(const glm::mat4 &projection)
{
    glm::vec2 ret;
    auto &c = projection[2][2];// zFar / (zNear - zFar);
    auto &d = projection[3][2];// -(zFar * zNear) / (zFar - zNear);

    // n = near clip plane distance
    ret.x = d / c;

    // f  = far clip plane distance
    ret.y = d / (c + 1.f);

    return ret;
}

namespace camera
{
glm::mat4 projection_matrix(const vierkant::Object3D *camera)
{
    assert(camera);
    if(const auto *camera_cmp = camera->get_component_ptr<camera_component_t>())
    {
        const auto &params = *camera_cmp;

        if(params.projection == camera_component_t::ORTHO)
        {
            return ortho_reverse_RH_ZO(params.ortho.left, params.ortho.right, params.ortho.bottom, params.ortho.top,
                                       params.ortho.near_, params.ortho.far_);
        }
        return perspective_infinite_reverse_RH_ZO(params.physical.fovy(), params.physical.aspect,
                                                  params.physical.clipping_distances.x);
    }
    return {};
}

float near(const vierkant::Object3D *camera)
{
    assert(camera);
    if(const auto *camera_cmp = camera->get_component_ptr<camera_component_t>())
    {
        const auto &params = *camera_cmp;
        return params.projection == camera_component_t::ORTHO ? params.ortho.near_
                                                           : params.physical.clipping_distances.x;
    }
    return 0.f;
}

float far(const vierkant::Object3D *camera)
{
    assert(camera);
    if(const auto *camera_cmp = camera->get_component_ptr<camera_component_t>())
    {
        const auto &params = *camera_cmp;
        return params.projection == camera_component_t::ORTHO ? params.ortho.far_ : params.physical.clipping_distances.y;
    }
    return 0.f;
}

vierkant::Frustum frustum(const vierkant::Object3D *camera)
{
    assert(camera);
    if(const auto *camera_cmp = camera->get_component_ptr<camera_component_t>())
    {
        const auto &params = *camera_cmp;

        if(params.projection == camera_component_t::ORTHO)
        {
            return {params.ortho.left, params.ortho.right, params.ortho.bottom,
                    params.ortho.top,  params.ortho.near_, params.ortho.far_};
        }
        return {params.physical.aspect, params.physical.fovx(), params.physical.clipping_distances.x,
                params.physical.clipping_distances.y};
    }
    return {};
}

vierkant::Ray calculate_ray(const vierkant::Object3D *camera, const glm::vec2 &pos, const glm::vec2 &extent)
{
    assert(camera);
    if(const auto *camera_cmp = camera->get_component_ptr<camera_component_t>())
    {
        const auto &params = *camera_cmp;
        auto t = camera->global_transform();
        glm::mat3 m = glm::mat3_cast(t.rotation);

        if(params.projection == camera_component_t::ORTHO)
        {
            const auto &cam_param = params.ortho;
            const glm::vec2 coord(crocore::map_value<float>(pos.x, 0, extent.x, cam_param.left, cam_param.right),
                                  crocore::map_value<float>(pos.y, extent.y, 0, cam_param.bottom, cam_param.top));

            glm::vec3 click_world_pos =
                    glm::vec3(t.translation) - m[2] * cam_param.near_ + m[0] * coord.x + m[1] * coord.y;
            glm::vec3 ray_dir = -m[2];
            spdlog::trace("clicked_world: ({}, {}, {})", click_world_pos.x, click_world_pos.y, click_world_pos.z);
            return {click_world_pos, ray_dir};
        }
        const auto &cam_param = params.physical;

        // bring click_pos to range -1, 1
        glm::vec2 click_2D(pos);
        glm::vec2 offset(extent / 2.0f);
        click_2D -= offset;
        click_2D /= offset;
        click_2D.y = -click_2D.y;

        // convert fovy to radians
        const float rad = cam_param.fovx();
        const float near = cam_param.clipping_distances.x;
        const float hLength = std::tan(rad / 2) * near;
        const float vLength = hLength / cam_param.aspect;

        glm::vec3 ray_origin = glm::vec3(t.translation) - m[2] * near + m[0] * hLength * click_2D.x +
                               m[1] * vLength * click_2D.y;
        glm::vec3 ray_dir = ray_origin - glm::vec3(t.translation);
        return {ray_origin, ray_dir};
    }
    return {};
}

}// namespace camera

///////////////////////////////////////////////////////////////////////////////////////////////////

glm::mat4 CubeCamera::projection_matrix() const
{ return perspective_infinite_reverse_RH_ZO(glm::radians(90.f), 1.f, m_params.clipping_distances.x); }

///////////////////////////////////////////////////////////////////////////////////////////////////

glm::mat4 CubeCamera::view_matrix(uint32_t the_face) const
{
    constexpr glm::vec3 X_AXIS(1.f, 0.f, 0.f), Y_AXIS(0.f, 1.f, 0.f), Z_AXIS(0.f, 0.f, 1.f);

    constexpr glm::vec3 vals[12] = {X_AXIS, -Y_AXIS, -X_AXIS, -Y_AXIS, -Y_AXIS, -Z_AXIS,
                                    Y_AXIS, Z_AXIS,  Z_AXIS,  -Y_AXIS, -Z_AXIS, -Y_AXIS};
    glm::vec3 p{};// = global_transform().translation;
    the_face = crocore::clamp<uint32_t>(the_face, 0, 5);
    return glm::lookAt(p, p + vals[2 * the_face], vals[2 * the_face + 1]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<glm::mat4> CubeCamera::view_matrices() const
{
    std::vector<glm::mat4> out_matrices(6);

    for(uint32_t i = 0; i < 6; ++i) { out_matrices[i] = view_matrix(i); }
    return out_matrices;
}

}// namespace vierkant
