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

    bool enabled = false;

    float multiplier = 1.f;

    glm::vec2 screen_size{};

    glm::vec3 look_at = glm::vec3(0.f);

    float distance = 1.f;

    Arcball() = default;

    explicit Arcball(const glm::vec2 &screen_size);

    void update(double time_delta);

    void mouse_press(const MouseEvent &e);

    void mouse_release(const MouseEvent &e);

    void mouse_drag(const MouseEvent &e);

    vierkant::mouse_delegate_t mouse_delegate();

    const glm::quat &rotation() const{ return m_current_rotation; }

    glm::mat4 transform() const;

private:

    glm::vec3 get_arcball_vector(const glm::vec2 &screen_pos) const;

    glm::ivec2 m_clicked_pos{}, m_last_pos{};

    glm::vec3 m_last_look_at{};

    // mouse rotation control
    glm::vec2 m_inertia = {};

    glm::quat m_last_rotation = {}, m_current_rotation = {1.0f, 0.0f, 0.0f, 0.0f};
    crocore::CircularBuffer<glm::vec2> m_drag_buffer;

    bool m_mouse_down = false;
};

};