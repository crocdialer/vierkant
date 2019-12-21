//
// Created by crocdialer on 12/20/19.
//

#pragma once

#include "vierkant/Input.hpp"
#include "vierkant/Camera.hpp"

namespace vierkant
{

class Arcball
{
public:

    bool enabled = false;

    glm::vec2 multiplier = {1.f, 1.f};

    glm::vec2 screen_size;

    Arcball() = default;

    Arcball(vierkant::Object3DPtr object, const glm::vec2& screen_size);

    virtual ~Arcball() = default;

    void update();

    void mouse_press(const MouseEvent &e);

    void mouse_release(const MouseEvent &e);

    void mouse_move(const MouseEvent &e);

    vierkant::mouse_delegate_t mouse_delegate();

private:

    glm::vec3 get_arcball_vector(const glm::vec2 &screen_pos);

//    glm::quat m_rotation;
    vierkant::Object3DPtr m_object;

    glm::ivec2 m_last_pos = {};
    glm::ivec2 m_current_pos = {};
    bool m_arcball_on = false;
};

};