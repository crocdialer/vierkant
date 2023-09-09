#pragma once

#include "Object3D.hpp"
#include "physical_camera_params.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(Camera)

DEFINE_CLASS_PTR(OrthoCamera)

DEFINE_CLASS_PTR(PerspectiveCamera)

DEFINE_CLASS_PTR(CubeCamera)

/**
 * @brief   Extract the near- and far-clipping distances from a projection matrix.
 *
 * @param   projection  a provided 4x4 projection matrix.
 * @return  a glm::vec2 containing (near, far) distances
 */
glm::vec2 clipping_distances(const glm::mat4 &projection);

class Camera : virtual public Object3D
{
public:
    vierkant::transform_t view_transform() const;

    virtual glm::mat4 projection_matrix() const = 0;

    virtual vierkant::Frustum frustum() const = 0;

    virtual float near() const = 0;

    virtual float far() const = 0;

    virtual vierkant::Ray calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const = 0;
};

class OrthoCamera : public Camera
{
public:
    float left, right, bottom, top, near_, far_;

    static OrthoCameraPtr create(float left, float right, float bottom, float top, float near, float far);

    glm::mat4 projection_matrix() const override;

    vierkant::Frustum frustum() const override;

    float near() const override { return near_; };

    float far() const override { return far_; };

    vierkant::Ray calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const override;


private:
    OrthoCamera(float left, float right, float bottom, float top, float near, float far);
};

class PerspectiveCamera : public Camera
{

public:
    static PerspectiveCameraPtr create(const std::shared_ptr<entt::registry> &registry,
                                       const physical_camera_params_t params = {})
    {
        auto ret = PerspectiveCameraPtr(new PerspectiveCamera(registry));
        ret->add_component(params);
        return ret;
    }

    glm::mat4 projection_matrix() const override;

    vierkant::Frustum frustum() const override;

    float near() const override { return get_component<physical_camera_params_t>().clipping_distances.x; };

    float far() const override { return get_component<physical_camera_params_t>().clipping_distances.y; };

    vierkant::Ray calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const override;

private:
    explicit PerspectiveCamera(const std::shared_ptr<entt::registry> &registry);
};

class CubeCamera : public Camera
{
public:
    static CubeCameraPtr create(float the_near, float the_far)
    {
        return CubeCameraPtr(new CubeCamera(the_near, the_far));
    };

    glm::mat4 projection_matrix() const override;

    vierkant::Frustum frustum() const override;

    float near() const override { return m_near; };

    float far() const override { return m_far; };

    vierkant::Ray calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const override;

    glm::mat4 view_matrix(uint32_t the_face) const;

    std::vector<glm::mat4> view_matrices() const;

private:
    CubeCamera(float the_near, float the_far);

    float m_near, m_far;
};

}// namespace vierkant
