//
// Created by crocdialer on 12/20/19.
//

#include "vierkant/CameraControl.hpp"

namespace vierkant
{

void OrbitCamera::update(double time_delta)
{
    if(!enabled){ return; }

    bool needs_update = false;

    // quick and dirty joystick-controls
    auto joystick_states = get_joystick_states();

    if(!joystick_states.empty())
    {
        constexpr float js_sensitivity = 250.f;
        constexpr float zoom_sensitivity = 0.1f;
        glm::vec2 pan_sensitivity = js_sensitivity * spherical_coords.x / screen_size;

        const auto &state = joystick_states[0];
        auto trigger = (state.trigger() + 1.f) / 2.f;

        constexpr float deadzone_thresh = 0.008;
        bool above_thresh = glm::length2(state.analog_right()) > deadzone_thresh ||
                            glm::length2(state.analog_left()) > deadzone_thresh ||
                            glm::length2(trigger) > deadzone_thresh;

        glm::vec2 pan_diff = pan_sensitivity * glm::vec2(1, -1) * state.analog_left() * static_cast<float>(time_delta);

        glm::vec2 orbit_diff = js_sensitivity * -state.analog_right() * static_cast<float>(time_delta);

        float zoom = state.trigger().y - state.trigger().x;
        zoom *= zoom_sensitivity;

        if(above_thresh)
        {
            spherical_coords.x = std::max(.1f, spherical_coords.x - zoom);
            pan(pan_diff);
            orbit(orbit_diff);

            needs_update = true;
        }
    }
    if(needs_update && transform_cb){ transform_cb(transform()); }
}

void OrbitCamera::pan(const glm::vec2 &diff)
{
    auto rot = rotation();

    look_at -= glm::normalize(rot * glm::vec3(1, 0, 0)) * diff.x +
               glm::normalize(rot * glm::vec3(0, 1, 0)) * diff.y;
}

void OrbitCamera::orbit(const glm::vec2 &diff)
{
    spherical_coords += glm::vec3(0.f, glm::radians(diff.x), glm::radians(diff.y));
    spherical_coords.z = std::clamp(spherical_coords.z, -glm::half_pi<float>(), glm::half_pi<float>());
    spherical_coords.y = std::fmod(spherical_coords.y, glm::two_pi<float>());
}

void OrbitCamera::mouse_press(const MouseEvent &e)
{
    if(!enabled){ return; }
    m_last_pos = m_clicked_pos = e.position();
}

void OrbitCamera::mouse_drag(const MouseEvent &e)
{
    glm::vec2 diff = m_last_pos - e.position();
    m_last_pos = e.position();

    if(enabled && e.is_left()){ orbit(diff); }
    else if(enabled && e.is_right())
    {
        diff *= glm::vec2(-1, 1) * spherical_coords.x / screen_size;
        pan(diff);
    }
    if(enabled && transform_cb){ transform_cb(transform()); }
}

vierkant::mouse_delegate_t OrbitCamera::mouse_delegate()
{
    vierkant::mouse_delegate_t ret = {};
    ret.mouse_press = [this](const MouseEvent &e){ mouse_press(e); };
    ret.mouse_drag = [this](const MouseEvent &e){ mouse_drag(e); };
    ret.mouse_wheel = [this](const vierkant::MouseEvent &e)
    {
        float scroll_gain = e.is_control_down() ? .1f : 1.f;
        if(enabled){ spherical_coords.x = std::max(.1f, spherical_coords.x - scroll_gain * e.wheel_increment().y); }
        if(enabled && transform_cb){ transform_cb(transform()); }
    };
    return ret;
}

glm::mat4 OrbitCamera::transform() const
{
    glm::mat4 ret = glm::mat4_cast(rotation());
    ret[3] = glm::vec4(look_at + (ret * glm::vec4(0, 0, spherical_coords.x, 1.f)).xyz(), 1.f);
    return ret;
}

void FlyCamera::update(double time_delta)
{
    if(enabled)
    {
        glm::vec3 move_mask(0.f);
        bool needs_update = false;

        for(const auto &[key, state] : m_keys)
        {
            if(state)
            {
                needs_update = true;

                switch(key)
                {
                    case vierkant::Key::_PAGE_UP:
                        move_mask.y += 1.f;
                        break;
                    case vierkant::Key::_PAGE_DOWN:
                        move_mask.y -= 1.f;
                        break;
                    case vierkant::Key::_RIGHT:
                        move_mask.x += 1.f;
                        break;
                    case vierkant::Key::_LEFT:
                        move_mask.x -= 1.f;
                        break;
                    case vierkant::Key::_UP:
                        move_mask.z -= 1.f;
                        break;
                    case vierkant::Key::_DOWN:
                        move_mask.z += 1.f;
                        break;
                    default:
                        break;
                }
            }
        }

        // quick and dirty joystick-controls
        auto joystick_states = get_joystick_states();

        if(!joystick_states.empty())
        {
            const auto &state = joystick_states[0];

            move_mask.x += state.analog_left().x;
            move_mask.z += state.analog_left().y;

            glm::vec2 diff = -state.analog_right() * static_cast<float>(time_delta);

            bool above_thresh = glm::length2(move_mask) > 0.f || glm::length2(state.analog_right()) > 0.01;

            if(above_thresh)
            {
                constexpr float controller_sensitivity = 250.f;
                diff *= controller_sensitivity;

                m_last_rotation *= glm::quat(glm::vec3(0, glm::radians(diff.x), 0));

                pitch = std::clamp(pitch + diff.y, -90.f, 90.f);
                rotation = m_last_rotation * glm::quat(glm::vec3(glm::radians(pitch), 0, 0));

                needs_update = true;
            }
        }

        if(needs_update)
        {
            position += static_cast<float>(time_delta) * move_speed * (rotation * move_mask);
            if(transform_cb){ transform_cb(transform()); }
        }
    }
}

vierkant::mouse_delegate_t FlyCamera::mouse_delegate()
{
    vierkant::mouse_delegate_t ret = {};
    ret.mouse_press = [this](const MouseEvent &e)
    {
        if(!enabled){ return; }

        m_last_cursor_pos = e.position();
    };
    ret.mouse_drag = [this](const MouseEvent &e)
    {
        if(enabled && e.is_left())
        {
            glm::vec2 diff = m_last_cursor_pos - e.position();

            diff *= mouse_sensitivity;

            m_last_rotation *= glm::quat(glm::vec3(0, glm::radians(diff.x), 0));

            pitch = std::clamp(pitch + diff.y, -90.f, 90.f);
            rotation = m_last_rotation * glm::quat(glm::vec3(glm::radians(pitch), 0, 0));

            m_last_cursor_pos = e.position();

            if(enabled && transform_cb){ transform_cb(transform()); }
        }
    };
    return ret;
}

vierkant::key_delegate_t FlyCamera::key_delegate()
{
    vierkant::key_delegate_t ret = {};
    ret.key_press = [this](const vierkant::KeyEvent &e)
    {
        switch(e.code())
        {
            case vierkant::Key::_PAGE_UP:
            case vierkant::Key::_PAGE_DOWN:
            case vierkant::Key::_RIGHT:
            case vierkant::Key::_LEFT:
            case vierkant::Key::_UP:
            case vierkant::Key::_DOWN:
                m_keys[e.code()] = true;
                if(enabled && transform_cb){ transform_cb(transform()); }
            default:
                break;
        }
    };
    ret.key_release = [this](const vierkant::KeyEvent &e)
    {
        switch(e.code())
        {
            case vierkant::Key::_PAGE_UP:
            case vierkant::Key::_PAGE_DOWN:
            case vierkant::Key::_RIGHT:
            case vierkant::Key::_LEFT:
            case vierkant::Key::_UP:
            case vierkant::Key::_DOWN:
                m_keys[e.code()] = false;
                if(enabled && transform_cb){ transform_cb(transform()); }
            default:
                break;
        }
    };
    return ret;
}

}