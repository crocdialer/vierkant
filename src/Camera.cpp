#include "vierkant/Camera.hpp"

namespace vierkant
{

///////////////////////////////////////////////////////////////////////////////////////////////////

Camera::Camera(const std::string &name) : Object3D(name){}

///////////////////////////////////////////////////////////////////////////////////////////////////

glm::mat4 Camera::view_matrix() const
{
    return glm::inverse(global_transform());
}

///////////////////////////////////////////////////////////////////////////////////////////////////

AABB Camera::boundingbox() const
{
    return {glm::vec3(-0.5f), glm::vec3(0.5f)};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

OrthoCameraPtr OrthoCamera::create(float left, float right,
                                   float bottom, float top,
                                   float near, float far)
{
    return OrthoCameraPtr(new OrthoCamera(left, right, bottom, top, near, far));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

OrthoCamera::OrthoCamera(float left, float right,
                         float bottom, float top,
                         float near, float far) :
        Camera("OrthoCamera"),
        m_left(left),
        m_right(right),
        m_bottom(bottom),
        m_top(top),
        m_near(near),
        m_far(far)
{
    update_projection_matrix();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void OrthoCamera::update_projection_matrix()
{
    auto m = glm::orthoRH(m_left, m_right, m_bottom, m_top, m_near, m_far);
    m[1][1] *= -1;
    m_projection = m;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::Frustum OrthoCamera::frustum() const
{
    return {left(), right(), bottom(), top(), near(), far()};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::Ray OrthoCamera::calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const
{
    glm::vec3 click_world_pos, ray_dir;

    glm::vec2 coord(crocore::map_value<float>(pos.x, 0, extent.x, left(), right()),
                    crocore::map_value<float>(pos.y, extent.y, 0, bottom(), top()));
    click_world_pos = position() + lookAt() * near() + side() * coord.x + up() * coord.y;
    ray_dir = lookAt();
    LOG_TRACE_2 << "clicked_world: (" << click_world_pos.x << ",  " << click_world_pos.y
                << ",  " << click_world_pos.z << ")";
    return Ray(click_world_pos, ray_dir);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void OrthoCamera::set_size(const glm::vec2 &the_sz)
{
    m_left = 0.f;
    m_right = the_sz.x;
    m_bottom = 0.f;
    m_top = the_sz.y;
    m_near = 0.f;
    m_far = 1.f;
    update_projection_matrix();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

PerspectiveCamera::PerspectiveCamera(float ascpect, float fov, float near, float far) :
        Camera("PerspectiveCamera"),
        m_near(near),
        m_far(far),
        m_fov(fov),
        m_aspect(ascpect)
{
    update_projection_matrix();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void PerspectiveCamera::update_projection_matrix()
{
    auto m = glm::perspectiveRH(glm::radians(m_fov), m_aspect, m_near, m_far);
    m[1][1] *= -1;
    m_projection = m;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::Frustum PerspectiveCamera::frustum() const
{
    return {aspect(), fov(), near(), far()};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void PerspectiveCamera::set_fov(float theFov)
{
    m_fov = theFov;
    update_projection_matrix();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void PerspectiveCamera::set_aspect(float theAspect)
{
    if(std::isnan(theAspect)){ return; }
    m_aspect = theAspect;
    update_projection_matrix();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void PerspectiveCamera::set_clipping(float near, float far)
{
    m_near = near;
    m_far = far;
    update_projection_matrix();
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
    float rad = glm::radians(fov());
    float vLength = std::tan(rad / 2) * near();
    float hLength = vLength * aspect();

    ray_origin = position() + lookAt() * near() + side() * hLength * click_2D.x
                 + up() * vLength * click_2D.y;
    ray_dir = ray_origin - position();

    LOG_TRACE_2 << "ray-origin: " << glm::to_string(ray_origin) << " -- dir: "
                << glm::to_string(glm::normalize(ray_dir));
    return Ray(ray_origin, ray_dir);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

CubeCamera::CubeCamera(float the_near, float the_far) :
        Camera("CubeCamera"),
        m_near(the_near),
        m_far(the_far)
{
    update_projection_matrix();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CubeCamera::update_projection_matrix()
{
    auto m = glm::perspectiveRH(glm::radians(90.f), 1.f, m_near, m_far);
    m[1][1] *= -1;
    m_projection = m;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::Frustum CubeCamera::frustum() const
{
    auto p = global_position();
    return {p.x - far(), p.x + far(), p.y - far(), p.y + far(), p.z - far(), p.z + far()};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

glm::mat4 CubeCamera::view_matrix(uint32_t the_face) const
{
    const glm::vec3 X_AXIS(1.f, 0.f, 0.f), Y_AXIS(0.f, 1.f, 0.f), Z_AXIS(0.f, 0.f, 1.f);

    const glm::vec3 vals[12] =
            {
                    X_AXIS, -Y_AXIS,
                    -X_AXIS, -Y_AXIS,
                    -Y_AXIS, -Z_AXIS,
                    Y_AXIS, Z_AXIS,
                    Z_AXIS, -Y_AXIS,
                    -Z_AXIS, -Y_AXIS
            };
    auto p = global_position();
    the_face = crocore::clamp<uint32_t>(the_face, 0, 5);
    return glm::lookAt(p, p + vals[2 * the_face], vals[2 * the_face + 1]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::Ray CubeCamera::calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const
{
    return {position(), glm::vec3(0, 0, 1)};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<glm::mat4> CubeCamera::view_matrices() const
{
    std::vector<glm::mat4> out_matrices(6);

    for(uint32_t i = 0; i < 6; ++i)
    {
        out_matrices[i] = view_matrix(i);
    }
    return out_matrices;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

}//namespace
