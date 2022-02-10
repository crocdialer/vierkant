//
// Created by crocdialer on 12/20/19.
//

#pragma once

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

    virtual vierkant::joystick_delegate_t joystick_delegate() = 0;

    [[nodiscard]] virtual glm::mat4 transform() const = 0;
};

DEFINE_CLASS_PTR(OrbitCamera)

class OrbitCamera : public CameraControl
{
public:

    glm::vec3 look_at = glm::vec3(0.f);

    // (theta, phi)
    glm::vec2 spherical_coords = {glm::half_pi<float>(), 0.f};

    float distance = 1.f;

    void update(double time_delta) override;

    vierkant::key_delegate_t key_delegate() override{ return {}; };

    vierkant::mouse_delegate_t mouse_delegate() override;

    vierkant::joystick_delegate_t joystick_delegate() override;

    [[nodiscard]] glm::mat4 transform() const override;

    static OrbitCameraUPtr create(){ return std::make_unique<OrbitCamera>(); }

private:

    void pan(const glm::vec2 &diff);

    void orbit(const glm::vec2 &diff);

    void mouse_press(const MouseEvent &e);

    void mouse_drag(const MouseEvent &e);

    [[nodiscard]] inline glm::quat rotation() const{ return {glm::vec3(spherical_coords.yx(), 0.f)}; }

    glm::ivec2 m_last_pos{};

    bool m_mouse_down = false;

    std::vector<JoystickState> m_last_joystick_states;
};

DEFINE_CLASS_PTR(FlyCamera)

class FlyCamera : public CameraControl
{
public:

    glm::vec3 position = {0.0f, 0.0f, 0.0f};

    // (theta, phi)
    glm::vec2 spherical_coords = {glm::half_pi<float>(), 0.f};

    float move_speed = 1.f;

    void update(double time_delta) override;

    vierkant::mouse_delegate_t mouse_delegate() override;

    vierkant::key_delegate_t key_delegate() override;

    vierkant::joystick_delegate_t joystick_delegate() override;

    glm::mat4 transform() const override;

    static FlyCameraUPtr create(){ return std::make_unique<FlyCamera>(); }

private:

    void orbit(const glm::vec2 &diff);

    [[nodiscard]] inline glm::quat rotation() const{ return {glm::vec3(spherical_coords.yx(), 0.f)}; }

    std::unordered_map<int, bool> m_keys;

    std::vector<JoystickState> m_last_joystick_states;
    glm::ivec2 m_last_cursor_pos{};
};

}// namespace vierkant