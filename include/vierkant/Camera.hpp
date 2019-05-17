#pragma once

#include "Object3D.hpp"

namespace vierkant {

DEFINE_CLASS_PTR(Camera)

DEFINE_CLASS_PTR(OrthoCamera)

DEFINE_CLASS_PTR(PerspectiveCamera)

DEFINE_CLASS_PTR(CubeCamera)

class Camera : public Object3D
{
public:

    glm::mat4 projection_matrix() const { return m_projectionMatrix; };

    glm::mat4 view_matrix() const;

    vierkant::AABB boundingbox() const;

    virtual vierkant::Frustum frustum() const = 0;

    virtual float near() const = 0;

    virtual float far() const = 0;

protected:

    Camera(const std::string &name);

    virtual void update_projection_matrix() = 0;

    void set_projection_matrix(const glm::mat4 &theMatrix) { m_projectionMatrix = theMatrix; };

private:

    glm::mat4 m_projectionMatrix;
};

class OrthoCamera : public Camera
{
public:

    static OrthoCameraPtr create_for_window();

    static OrthoCameraPtr create(float left, float right, float bottom, float top, float near, float far);

    virtual vierkant::Frustum frustum() const override;

    float near() const override { return m_near; };

    void near(float val)
    {
        m_near = val;
        update_projection_matrix();
    };

    float far() const override { return m_far; };

    void far(float val)
    {
        m_far = val;
        update_projection_matrix();
    };

    inline float left() const { return m_left; };

    void left(float val)
    {
        m_left = val;
        update_projection_matrix();
    };

    inline float right() const { return m_right; };

    void right(float val)
    {
        m_right = val;
        update_projection_matrix();
    };

    inline float bottom() const { return m_bottom; };

    void bottom(float val)
    {
        m_bottom = val;
        update_projection_matrix();
    };

    inline float top() const { return m_top; };

    void top(float val)
    {
        m_top = val;
        update_projection_matrix();
    };

    void set_size(const glm::vec2 &the_sz);

protected:

    void update_projection_matrix() override;

private:

    OrthoCamera(float left, float right, float bottom, float top,
                float near, float far);

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

    float fov() const { return m_fov; };

    void set_aspect(float theAspect);

    float aspect() const { return m_aspect; };

    void set_clipping(float near, float far);

    float near() const override { return m_near; };

    float far() const override { return m_far; };

protected:

    void update_projection_matrix() override;

private:

    explicit PerspectiveCamera(float ascpect = 4.f / 3.f, float fov = 45, float near = .1, float far = 5000);

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

    float near() const override { return m_near; };

    float far() const override { return m_far; };

    glm::mat4 view_matrix(uint32_t the_face) const;

    std::vector<glm::mat4> view_matrices() const;

private:

    CubeCamera(float the_near, float the_far);

    void update_projection_matrix() override;

    float m_near, m_far;
};

}//namespace
