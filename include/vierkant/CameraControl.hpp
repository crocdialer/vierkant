//
// Created by crocdialer on 12/20/19.
//

#pragma once

#include <crocore/CircularBuffer.hpp>
#include <memory>
#include "vierkant/Input.hpp"

namespace vierkant
{

using transform_cb_t = std::function<void(const glm::mat4 &)>;

DEFINE_CLASS_PTR(CameraControl)

class CameraControl
{
public:

    bool enabled = true;

    glm::vec2 screen_size = {};

    transform_cb_t transform_cb = {};

    glm::vec2 mouse_sensitivity = {1.f, 1.f};

    virtual void update(double time_delta) = 0;

    virtual vierkant::mouse_delegate_t mouse_delegate() = 0;

    virtual vierkant::key_delegate_t key_delegate() = 0;

    [[nodiscard]] virtual glm::mat4 transform() const = 0;
};

DEFINE_CLASS_PTR(OrbitCamera)

class OrbitCamera : public CameraControl
{
public:

    glm::vec3 look_at = glm::vec3(0.f);

    // (dist, theta, phi)
    glm::vec3 spherical_coords = {1.f, glm::half_pi<float>(), 0.f};

    void update(double time_delta) override;

    vierkant::key_delegate_t key_delegate() override{ return {}; };

    vierkant::mouse_delegate_t mouse_delegate() override;

    [[nodiscard]] glm::mat4 transform() const override;

    static OrbitCameraUPtr create(){ return std::make_unique<OrbitCamera>(); }

private:

//    OrbitCamera() = default;

    void pan(const glm::vec2 &diff);

    void orbit(const glm::vec2 &diff);

    void mouse_press(const MouseEvent &e);

    void mouse_drag(const MouseEvent &e);

    [[nodiscard]] inline glm::quat rotation() const{ return {glm::vec3(spherical_coords.zy(), 0.f)}; }

    glm::ivec2 m_clicked_pos{}, m_last_pos{};

    bool m_mouse_down = false;
};

DEFINE_CLASS_PTR(FlyCamera)

class FlyCamera : public CameraControl
{
public:

    glm::vec3 position = {0.0f, 0.0f, 0.0f};

    glm::quat rotation = {1.0f, 0.0f, 0.0f, 0.0f};

    float move_speed = 1.f;

    void update(double time_delta) override;

    vierkant::mouse_delegate_t mouse_delegate() override;

    vierkant::key_delegate_t key_delegate() override;

    glm::mat4 transform() const override
    {
        glm::mat4 ret = glm::mat4_cast(rotation);
        ret[3] = glm::vec4(position, 1.f);
        return ret;
    }

    static FlyCameraUPtr create(){ return std::make_unique<FlyCamera>(); }

private:

//    FlyCamera() = default;

    std::unordered_map<int, bool> m_keys;

    glm::ivec2 m_last_cursor_pos{};
    glm::quat m_last_rotation = {1.0f, 0.0f, 0.0f, 0.0f};

    float pitch = 0.f;
};

}// namespace vierkant