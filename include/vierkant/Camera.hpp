#pragma once

#include "Object3D.hpp"
#include "camera_params.hpp"

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

    virtual vierkant::camera_params_variant_t params() const = 0;

    void accept(class Visitor &v) override;
};

class OrthoCamera : public Camera
{
public:
    vierkant::ortho_camera_params_t orth_params;

    static OrthoCameraPtr create(const std::shared_ptr<entt::registry> &registry,
                                 vierkant::ortho_camera_params_t params = {})
    {
        {
            auto ret = OrthoCameraPtr(new OrthoCamera(registry));
            ret->orth_params = params;
            return ret;
        }
    }

    glm::mat4 projection_matrix() const override;

    vierkant::Frustum frustum() const override;

    float near() const override { return orth_params.near_; };

    float far() const override { return orth_params.far_; };

    vierkant::Ray calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const override;

    vierkant::camera_params_variant_t params() const override { return orth_params; }

    void accept(class Visitor &v) override;

private:
    explicit OrthoCamera(const std::shared_ptr<entt::registry> &registry);
};

class PerspectiveCamera : public Camera
{
public:
    physical_camera_params_t perspective_params;

    static PerspectiveCameraPtr create(const std::shared_ptr<entt::registry> &registry,
                                       const physical_camera_params_t params = {})
    {
        auto ret = PerspectiveCameraPtr(new PerspectiveCamera(registry));
        ret->perspective_params = params;
        return ret;
    }

    glm::mat4 projection_matrix() const override;

    vierkant::Frustum frustum() const override;

    float near() const override { return perspective_params.clipping_distances.x; };

    float far() const override { return perspective_params.clipping_distances.y; };

    vierkant::Ray calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const override;

    vierkant::camera_params_variant_t params() const override { return perspective_params; }

    void accept(class Visitor &v) override;

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

    vierkant::camera_params_variant_t params() const override { return vierkant::physical_camera_params_t(); }

    glm::mat4 view_matrix(uint32_t the_face) const;

    std::vector<glm::mat4> view_matrices() const;

private:
    CubeCamera(float the_near, float the_far);

    float m_near, m_far;
};

}// namespace vierkant
