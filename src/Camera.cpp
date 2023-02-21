#include <spdlog/spdlog.h>
#include <vierkant/Camera.hpp>
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


///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::transform_t Camera::view_transform() const { return vierkant::inverse(global_transform()); }

///////////////////////////////////////////////////////////////////////////////////////////////////

OrthoCameraPtr OrthoCamera::create(float left, float right, float bottom, float top, float near, float far)
{
    return OrthoCameraPtr(new OrthoCamera(left, right, bottom, top, near, far));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

OrthoCamera::OrthoCamera(float left, float right, float bottom, float top, float near, float far)
    : Object3D({}, "OrthoCamera"), left(left), right(right), bottom(bottom), top(top), near_(near), far_(far)
{}

///////////////////////////////////////////////////////////////////////////////////////////////////

glm::mat4 OrthoCamera::projection_matrix() const
{
    auto m = glm::orthoRH(left, right, bottom, top, near_, far_);
    m[1][1] *= -1;
    return m;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::Frustum OrthoCamera::frustum() const { return {left, right, bottom, top, near_, far_}; }

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::Ray OrthoCamera::calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const
{
    glm::vec3 click_world_pos, ray_dir;

    glm::vec2 coord(crocore::map_value<float>(pos.x, 0, extent.x, left, right),
                    crocore::map_value<float>(pos.y, extent.y, 0, bottom, top));

    glm::mat3 m = glm::mat3_cast(transform.rotation);
    click_world_pos = glm::vec3(transform.translation) - m[2] * near() + m[0] * coord.x + m[1] * coord.y;
    ray_dir = -m[2];
    spdlog::trace("clicked_world: ({}, {}, {})", click_world_pos.x, click_world_pos.y, click_world_pos.z);
    return {click_world_pos, ray_dir};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

PerspectiveCamera::PerspectiveCamera(const std::shared_ptr<entt::registry> &registry)
    : Object3D(registry, "PerspectiveCamera")
{}

///////////////////////////////////////////////////////////////////////////////////////////////////

glm::mat4 PerspectiveCamera::projection_matrix() const
{
    const auto &params = get_component<physical_camera_params_t>();
    return perspective_infinite_reverse_RH_ZO(params.fovy(), params.aspect, params.clipping_distances.x);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::Frustum PerspectiveCamera::frustum() const
{
    const auto &params = get_component<physical_camera_params_t>();
    return {params.aspect, params.fovx(), params.clipping_distances.x, params.clipping_distances.y};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::Ray PerspectiveCamera::calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const
{
    glm::vec3 ray_origin, ray_dir;

    // bring click_pos to range -1, 1
    glm::vec2 click_2D(pos);
    glm::vec2 offset(extent / 2.0f);
    click_2D -= offset;
    click_2D /= offset;
    click_2D.y = -click_2D.y;

    // convert fovy to radians
    const auto &params = get_component<physical_camera_params_t>();
    float rad = params.fovx();
    float near = params.clipping_distances.x;
    float hLength = std::tan(rad / 2) * near;
    float vLength = hLength / params.aspect;

    glm::mat3 m = glm::mat3_cast(transform.rotation);
    ray_origin =
            glm::vec3(transform.translation) - m[2] * near + m[0] * hLength * click_2D.x + m[1] * vLength * click_2D.y;
    ray_dir = ray_origin - glm::vec3(transform.translation);
    return {ray_origin, ray_dir};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

CubeCamera::CubeCamera(float the_near, float the_far) : Object3D({}, "CubeCamera"), m_near(the_near), m_far(the_far) {}

///////////////////////////////////////////////////////////////////////////////////////////////////

glm::mat4 CubeCamera::projection_matrix() const
{
    return perspective_infinite_reverse_RH_ZO(glm::radians(90.f), 1.f, m_near);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::Frustum CubeCamera::frustum() const
{
    glm::vec3 p = transform.translation;
    return {p.x - far(), p.x + far(), p.y - far(), p.y + far(), p.z - far(), p.z + far()};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

glm::mat4 CubeCamera::view_matrix(uint32_t the_face) const
{
    const glm::vec3 X_AXIS(1.f, 0.f, 0.f), Y_AXIS(0.f, 1.f, 0.f), Z_AXIS(0.f, 0.f, 1.f);

    const glm::vec3 vals[12] = {X_AXIS, -Y_AXIS, -X_AXIS, -Y_AXIS, -Y_AXIS, -Z_AXIS,
                                Y_AXIS, Z_AXIS,  Z_AXIS,  -Y_AXIS, -Z_AXIS, -Y_AXIS};
    glm::vec3 p = transform.translation;
    the_face = crocore::clamp<uint32_t>(the_face, 0, 5);
    return glm::lookAt(p, p + vals[2 * the_face], vals[2 * the_face + 1]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::Ray CubeCamera::calculate_ray(const glm::vec2 & /*pos*/, const glm::vec2 & /*extent*/) const
{
    return {transform.translation, glm::vec3(0, 0, 1)};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<glm::mat4> CubeCamera::view_matrices() const
{
    std::vector<glm::mat4> out_matrices(6);

    for(uint32_t i = 0; i < 6; ++i) { out_matrices[i] = view_matrix(i); }
    return out_matrices;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

}// namespace vierkant
