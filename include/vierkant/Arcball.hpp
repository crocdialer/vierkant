//
// Created by crocdialer on 12/20/19.
//

#pragma once

#include <crocore/CircularBuffer.hpp>
#include "vierkant/Input.hpp"

namespace vierkant
{

class Arcball
{
public:

    using transform_cb_t = std::function<void(const glm::mat4&)>;

    bool enabled = false;

    float multiplier = 1.f;

    glm::vec2 screen_size = {};

    glm::quat rotation = {1.0f, 0.0f, 0.0f, 0.0f};

    glm::vec3 look_at = glm::vec3(0.f);

    float distance = 1.f;

    transform_cb_t transform_cb = {};

    Arcball() = default;

    explicit Arcball(const glm::vec2 &screen_size);

    void update(double time_delta);

    void mouse_press(const MouseEvent &e);

    void mouse_drag(const MouseEvent &e);

    vierkant::mouse_delegate_t mouse_delegate();

    glm::mat4 transform() const;

private:

    glm::vec3 get_arcball_vector(const glm::vec2 &screen_pos) const;

    glm::ivec2 m_clicked_pos{}, m_last_pos{};

    glm::vec3 m_last_look_at{};

    // mouse rotation control
    glm::vec2 m_inertia = {};

    glm::quat m_last_rotation = {};
    crocore::CircularBuffer<glm::vec2> m_drag_buffer;

    bool m_mouse_down = false;
};

class FlyCamera
{
public:

    glm::vec3 position = {0.0f, 0.0f, 0.0f};

    glm::quat rotation = {1.0f, 0.0f, 0.0f, 0.0f};

    float move_speed = 1.f;

    glm::vec2 mouse_sensitivity = {1.f, 1.f};

    void update(double time_delta);

    vierkant::mouse_delegate_t mouse_delegate();

    vierkant::key_delegate_t key_delegate();

    glm::mat4 transform() const
    {
        glm::mat4 ret = glm::mat4_cast(rotation);
        ret[3] = glm::vec4(position, 1.f);
        return ret;
    }
};

}// namespace vierkant