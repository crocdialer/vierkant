#pragma once

#include "Object3D.hpp"

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

struct alignas(16) projective_camera_params_t
{
    //! focal length in mm
    float focal_length = 50.f;

    //! horizontal sensor-size in mm
    float sensor_width = 36.f;

    //! sensor aspect-ratio (w/h)
    float aspect = 16.f / 9.f;

    //! camera near/far clipping distances in meter
    glm::vec2 clipping_distances = {0.1f, 100.f};

    //! focal distance in meter
    float focal_distance = 10.f;

    //! f-stop value
    float fstop = 2.8f;

    //! aperture/lens size in m
    [[nodiscard]] inline double aperture_size() const
    {
        return 0.001 * focal_length / fstop;
    }

    //! horizontal field-of-view (fov) in radians
    [[nodiscard]] inline float fovx() const
    {
        return 2 * std::atan(0.5f * sensor_width / focal_length);
    }

    //! horizontal field-of-view (fov) in radians
    [[nodiscard]] inline float fovy() const
    {
        return fovx() / aspect;
    }

    //! will adjust focal_length to match provided field-of-view (fov) in radians
    inline void set_fov(float fov)
    {
        focal_length = 0.5f * sensor_width / std::tan(fov * 0.5f);
    }
};

class Camera : virtual public Object3D
{
public:

    glm::mat4 view_matrix() const;

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

    float near() const override{ return near_; };

    float far() const override{ return far_; };

    vierkant::Ray calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const override;


private:

    OrthoCamera(float left, float right, float bottom, float top,
                float near, float far);
};

class PerspectiveCamera : public Camera
{

public:

    static PerspectiveCameraPtr create(const std::shared_ptr<entt::registry> &registry,
                                       const projective_camera_params_t params = {})
    {
        auto ret = PerspectiveCameraPtr(new PerspectiveCamera(registry));
        ret->add_component(params);
        return ret;
    }

    glm::mat4 projection_matrix() const override;

    vierkant::Frustum frustum() const override;

    float near() const override{ return get_component<projective_camera_params_t>().clipping_distances.x; };

    float far() const override{ return get_component<projective_camera_params_t>().clipping_distances.y; };

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

    float near() const override{ return m_near; };

    float far() const override{ return m_far; };

    vierkant::Ray calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const override;

    glm::mat4 view_matrix(uint32_t the_face) const;

    std::vector<glm::mat4> view_matrices() const;

private:

    CubeCamera(float the_near, float the_far);

    float m_near, m_far;
};

}//namespace
