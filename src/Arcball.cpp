//
// Created by crocdialer on 12/20/19.
//

#include "vierkant/Arcball.hpp"

namespace vierkant
{

Arcball::Arcball(const glm::vec2 &screen_size) :
        enabled(true),
        screen_size(screen_size)
{

}

void Arcball::update(double time_delta)
{
    if(!enabled){ return; }
}

void Arcball::mouse_press(const MouseEvent &e)
{
    if(!enabled){ return; }

    m_last_pos = m_clicked_pos = e.position();

    if(e.is_left())
    {
        m_last_rotation = rotation;
    }
    else if(e.is_right()){ m_last_look_at = look_at; }
}

void Arcball::mouse_drag(const MouseEvent &e)
{
    if(enabled && e.is_left())
    {
        glm::vec2 diff = m_last_pos - e.position();
        rotation = m_last_rotation * glm::quat(glm::vec3(glm::radians(diff.y), glm::radians(diff.x), 0));

//        if(m_last_pos != m_current_pos)
//        {
//            glm::vec3 va = get_arcball_vector(m_last_pos);
//            glm::vec3 vb = get_arcball_vector(m_current_pos);
//            float angle = multiplier * acosf(std::min(1.0f, glm::dot(va, vb)));
//            glm::vec3 axis_in_camera_coord = glm::cross(va, vb);
//            m_current_rotation = glm::rotate(m_last_rotation, angle, axis_in_camera_coord);
//        }
        m_last_pos = e.position();
        m_last_rotation = rotation;
    }
    else if(enabled && e.is_right())
    {
        glm::vec2 mouse_diff = e.position() - m_clicked_pos;
        mouse_diff *= distance / screen_size;

        glm::mat3 rotation_mat = glm::mat3_cast(rotation);
        look_at = m_last_look_at - glm::normalize(rotation_mat * glm::vec3(1, 0, 0)) * mouse_diff.x +
                  glm::normalize(rotation * glm::vec3(0, 1, 0)) * mouse_diff.y;
    }
    if(enabled && transform_cb){ transform_cb(transform()); }
}

/**
 * Get a normalized vector from the center of the virtual ball O to a
 * point P on the virtual ball surface, such that P is aligned on
 * screen's (X,Y) coordinates.  If (X,Y) is too far away from the
 * sphere, return the nearest point on the virtual ball surface.
 */
glm::vec3 Arcball::get_arcball_vector(const glm::vec2 &screen_pos) const
{
    // screenpos in range [-1 .. 1]
    glm::vec3 surface_point = glm::vec3(screen_pos / screen_size * 2.f - 1.f, 0.f);

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
    ret.mouse_drag = [this](const MouseEvent &e){ mouse_drag(e); };
    ret.mouse_wheel = [this](const vierkant::MouseEvent &e)
    {
        if(enabled){ distance = std::max(.1f, distance - e.wheel_increment().y); }
        if(enabled && transform_cb){ transform_cb(transform()); }
    };
    return ret;
}

glm::mat4 Arcball::transform() const
{
    glm::mat4 ret = glm::mat4_cast(rotation);
    ret[3] = glm::vec4(look_at + (ret * glm::vec4(0, 0, distance, 1.f)).xyz(), 1.f);
    return ret;
}

}