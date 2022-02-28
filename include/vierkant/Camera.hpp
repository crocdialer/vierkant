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

/**
 * @brief   'perspective_infinite_reverse_RH_ZO' returns a perspective projection-matrix with following properties:
 *          - far-clipping plane is at infinity
 *          - depth-range is inverted and falls in range [1..0]
 *
 * @param   fovY    vertical field-of-view in radians.
 * @param   aspect  aspect-ratio (width / height)
 * @param   zNear   near clipping distance
 * @return  a perspective projection-matrix
 */
glm::mat4 perspective_infinite_reverse_RH_ZO(float fovY, float aspect, float zNear);

class Camera : public Object3D
{
public:

    glm::mat4 projection_matrix() const{ return m_projection; };

    glm::mat4 view_matrix() const;

    vierkant::AABB boundingbox() const;

    virtual vierkant::Frustum frustum() const = 0;

    virtual float near() const = 0;

    virtual float far() const = 0;

    virtual float fov() const = 0;

    virtual vierkant::Ray calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const = 0;

protected:

    explicit Camera(const std::string &name);

    glm::mat4 m_projection{};

private:

    virtual void update_projection_matrix() = 0;
};

class OrthoCamera : public Camera
{
public:

    static OrthoCameraPtr create(float left, float right, float bottom, float top, float near, float far);

    vierkant::Frustum frustum() const override;

    float near() const override{ return m_near; };

    void near(float val)
    {
        m_near = val;
        update_projection_matrix();
    };

    vierkant::Ray calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const override;

    float far() const override{ return m_far; };

    void far(float val)
    {
        m_far = val;
        update_projection_matrix();
    };

    float fov() const override{ return glm::degrees(atanf(std::abs(m_right - m_left) / std::abs(m_far - m_near))); };

    inline float left() const{ return m_left; };

    void left(float val)
    {
        m_left = val;
        update_projection_matrix();
    };

    inline float right() const{ return m_right; };

    void right(float val)
    {
        m_right = val;
        update_projection_matrix();
    };

    inline float bottom() const{ return m_bottom; };

    void bottom(float val)
    {
        m_bottom = val;
        update_projection_matrix();
    };

    inline float top() const{ return m_top; };

    void top(float val)
    {
        m_top = val;
        update_projection_matrix();
    };

    void set_size(const glm::vec2 &the_sz);

private:

    OrthoCamera(float left, float right, float bottom, float top,
                float near, float far);

    void update_projection_matrix() override;

    float m_left, m_right, m_bottom, m_top, m_near, m_far;
};

class PerspectiveCamera : public Camera
{

public:

    static PerspectiveCameraPtr create(float ascpect = 4.f / 3.f, float fov = 45, float near = .1, float far = 5000)
    {
        return PerspectiveCameraPtr(new PerspectiveCamera(ascpect, fov, near, far));
    }

    vierkant::Frustum frustum() const override;

    void set_fov(float theFov);

    float fov() const override{ return m_fov; };

    void set_aspect(float theAspect);

    float aspect() const{ return m_aspect; };

    void set_clipping(float near, float far);

    float near() const override{ return m_near; };

    float far() const override{ return m_far; };

    vierkant::Ray calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const override;

private:

    PerspectiveCamera(float ascpect, float fov, float near, float far);

    void update_projection_matrix() override;

    float m_near, m_far;
    float m_fov;
    float m_aspect;
};

class CubeCamera : public Camera
{
public:

    static CubeCameraPtr create(float the_near, float the_far)
    {
        return CubeCameraPtr(new CubeCamera(the_near, the_far));
    };

    vierkant::Frustum frustum() const override;

    float near() const override{ return m_near; };

    float far() const override{ return m_far; };

    float fov() const override{ return 45.f; };

    vierkant::Ray calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const override;

    glm::mat4 view_matrix(uint32_t the_face) const;

    std::vector<glm::mat4> view_matrices() const;

private:

    CubeCamera(float the_near, float the_far);

    void update_projection_matrix() override;

    float m_near, m_far;
};

}//namespace
