//
// Created by crocdialer on 12/20/19.
//

#include "vierkant/Arcball.hpp"

namespace vierkant
{

Arcball::Arcball(vierkant::PerspectiveCameraPtr camera, const glm::vec2 &screen_size) :
        screen_size(screen_size),
        m_camera(std::move(camera))
{

}

void Arcball::update()
{
    if(!enabled){ return; }

    /* onIdle() */
    if(m_cur_mx != m_last_mx || m_cur_my != m_last_my)
    {
        glm::vec3 va = get_arcball_vector(m_last_mx, m_last_my);
        glm::vec3 vb = get_arcball_vector(m_cur_mx, m_cur_my);
        float angle = acosf(std::min(1.0f, glm::dot(va, vb)));
        glm::vec3 axis_in_camera_coord = glm::cross(va, vb);

        auto cam_transform = glm::rotate(m_camera->transform(), angle, axis_in_camera_coord);
        m_camera->set_transform(cam_transform);

//        glm::mat3 camera2object = glm::inverse(glm::mat3(transforms[MODE_CAMERA]) * glm::mat3(mesh.object2world));
//        glm::vec3 axis_in_object_coord = camera2object * axis_in_camera_coord;
//        mesh.object2world = glm::rotate(mesh.object2world, glm::degrees(angle), axis_in_object_coord);

        m_last_mx = m_cur_mx;
        m_last_my = m_cur_my;
    }

}

void Arcball::mouse_press(const MouseEvent &e)
{
    if(!enabled){ return; }

    if(e.is_left())
    {
        m_arcball_on = true;
        m_last_mx = m_cur_mx = e.get_x();
        m_last_my = m_cur_my = e.get_y();
    }
}

void Arcball::mouse_release(const MouseEvent &e)
{
    if(!enabled){ return; }

    if(e.is_left())
    {
        m_arcball_on = false;
    }
}

void Arcball::mouse_move(const MouseEvent &e)
{
    if(!enabled){ return; }
    
    if(m_arcball_on)
    {
        m_cur_mx = e.get_x();
        m_cur_my = e.get_y();
    }
}

/**
 * Get a normalized vector from the center of the virtual ball O to a
 * point P on the virtual ball surface, such that P is aligned on
 * screen's (X,Y) coordinates.  If (X,Y) is too far away from the
 * sphere, return the nearest point on the virtual ball surface.
 */
glm::vec3 Arcball::get_arcball_vector(int x, int y)
{

    glm::vec3 P = glm::vec3(1.0 * x / screen_size.x * 2 - 1.0,
                            1.0 * y / screen_size.y * 2 - 1.0,
                            0);
    P.y = -P.y;
    float OP_squared = P.x * P.x + P.y * P.y;
    if(OP_squared <= 1 * 1)
        P.z = sqrt(1 * 1 - OP_squared);  // Pythagoras
    else
        P = glm::normalize(P);  // nearest point
    return P;
}

vierkant::mouse_delegate_t Arcball::mouse_delegate()
{
    vierkant::mouse_delegate_t ret = {};
    ret.mouse_press = [this](const MouseEvent &e){ mouse_press(e); };
    ret.mouse_release = [this](const MouseEvent &e){ mouse_release(e); };
    ret.mouse_move = [this](const MouseEvent &e){ mouse_move(e); };
    return ret;
}

}