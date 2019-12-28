//
// Created by crocdialer on 12/20/19.
//

#pragma once

#include <crocore/CircularBuffer.hpp>
#include "vierkant/Input.hpp"
#include "vierkant/Camera.hpp"

namespace vierkant
{

class Arcball
{
public:

    bool enabled = false;

    float multiplier = 1.f;

    glm::vec2 screen_size;

    Arcball() = default;

    Arcball(vierkant::Object3DPtr object, const glm::vec2 &screen_size);

    void update(double time_delta);

    void mouse_press(const MouseEvent &e);

    void mouse_release(const MouseEvent &e);

    void mouse_move(const MouseEvent &e);

    vierkant::mouse_delegate_t mouse_delegate();

    const glm::quat &rotation() const{ return m_current_rotation; }

private:

    glm::vec3 get_arcball_vector(const glm::vec2 &screen_pos);

//    glm::quat m_rotation;
    vierkant::Object3DPtr m_object;

    glm::ivec2 m_last_pos = {};
    glm::ivec2 m_current_pos = {};

    // mouse rotation control
    glm::vec2 m_inertia = {};

    glm::quat m_last_rotation = {}, m_current_rotation = {1.0f, 0.0f, 0.0f, 0.0f};
    crocore::CircularBuffer<glm::vec2> m_drag_buffer;

    bool m_mouse_down = false;
};

};