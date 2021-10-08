//
// Created by crocdialer on 12/20/19.
//

#pragma once

#include <crocore/CircularBuffer.hpp>
#include "vierkant/Input.hpp"

namespace vierkant
{

using transform_cb_t = std::function<void(const glm::mat4 &)>;

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

class Arcball : public CameraControl
{
public:

    float multiplier = 1.f;

    glm::quat rotation = {1.0f, 0.0f, 0.0f, 0.0f};

    glm::vec3 look_at = glm::vec3(0.f);

    float distance = 1.f;

    void update(double time_delta) override;

    vierkant::key_delegate_t key_delegate() override{ return {}; };

    vierkant::mouse_delegate_t mouse_delegate() override;

    [[nodiscard]] glm::mat4 transform() const override;

private:

    void mouse_press(const MouseEvent &e);

    void mouse_drag(const MouseEvent &e);

    glm::vec3 get_arcball_vector(const glm::vec2 &screen_pos) const;

    glm::ivec2 m_clicked_pos{}, m_last_pos{};

    glm::vec3 m_last_look_at{};

    // mouse rotation control
    glm::vec2 m_inertia = {};

    glm::quat m_last_rotation = {1.0f, 0.0f, 0.0f, 0.0f};
    crocore::CircularBuffer<glm::vec2> m_drag_buffer;

    bool m_mouse_down = false;
};

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

private:

    std::unordered_map<int, bool> m_keys;

    glm::ivec2 m_last_pos{};
    glm::quat m_last_rotation = {1.0f, 0.0f, 0.0f, 0.0f};
};

}// namespace vierkant