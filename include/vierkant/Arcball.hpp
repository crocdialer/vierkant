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

    bool enabled = true;

    glm::vec2 screen_size;

    Arcball() = default;

    Arcball(vierkant::PerspectiveCameraPtr camera, const glm::vec2& screen_size);

    virtual ~Arcball() = default;

    void update();

    void mouse_press(const MouseEvent &e);

    void mouse_release(const MouseEvent &e);

    void mouse_move(const MouseEvent &e);

    vierkant::mouse_delegate_t mouse_delegate();

private:

    glm::vec3 get_arcball_vector(int x, int y);

    vierkant::PerspectiveCameraPtr m_camera;

    int m_last_mx = 0, m_last_my = 0, m_cur_mx = 0, m_cur_my = 0;
    bool m_arcball_on = false;
};

};