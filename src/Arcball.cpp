//
// Created by crocdialer on 12/20/19.
//

#include "vierkant/Arcball.hpp"

namespace vierkant
{

Arcball::Arcball(vierkant::Object3DPtr object, const glm::vec2 &screen_size) :
        enabled(true),
        screen_size(screen_size),
        m_object(std::move(object))
{

}

void Arcball::update()
{
    if(!enabled){ return; }

    if(m_last_pos != m_current_pos)
    {
        glm::vec3 va = get_arcball_vector(m_last_pos);
        glm::vec3 vb = get_arcball_vector(m_current_pos);
        float angle = acosf(std::min(1.0f, glm::dot(va, vb)));
        glm::vec3 axis_in_camera_coord = glm::cross(va, vb);

        auto obj_rotation = glm::rotate(m_object->rotation(), angle, axis_in_camera_coord);
//        m_object->set_rotation(obj_rotation);

        m_object->set_transform(glm::mat4(obj_rotation));
        m_last_pos = m_current_pos;
    }

}

void Arcball::mouse_press(const MouseEvent &e)
{
    if(!enabled){ return; }

    if(e.is_left())
    {
        m_arcball_on = true;
        m_last_pos = m_current_pos = e.position();
    }
}

void Arcball::mouse_release(const MouseEvent &e)
{
    if(enabled && e.is_left()){ m_arcball_on = false; }
}

void Arcball::mouse_move(const MouseEvent &e)
{
    if(enabled && m_arcball_on){ m_current_pos = e.position(); }
}

/**
 * Get a normalized vector from the center of the virtual ball O to a
 * point P on the virtual ball surface, such that P is aligned on
 * screen's (X,Y) coordinates.  If (X,Y) is too far away from the
 * sphere, return the nearest point on the virtual ball surface.
 */
glm::vec3 Arcball::get_arcball_vector(const glm::vec2 &screen_pos)
{
    // screenpos in range [-1 .. 1]
    glm::vec3 surface_point = glm::vec3(multiplier * (screen_pos / screen_size * 2.f - 1.f), 0.f);

    surface_point.y *= -1.f;
    float OP_squared = glm::length2(surface_point); //P.x * P.x + P.y * P.y;

    if(OP_squared <= 1.f)
    {
        // pythagoras
        surface_point.z = sqrtf(1 * 1 - OP_squared);
    }
    else
    {
        // nearest point
        surface_point = glm::normalize(surface_point);
    }
    return surface_point;
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