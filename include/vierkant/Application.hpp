#pragma once

#include <unordered_map>
#include "crocore/Component.hpp"
#include "crocore/ThreadPool.hpp"
#include "vierkant/Input.h"
#include "vierkant/vierkant.hpp"

namespace vierkant {

DEFINE_CLASS_PTR(Application);

//DEFINE_CLASS_PTR(Window);

//class MouseEvent;
//
//class KeyEvent;
//
//class JoystickState;
//
//struct Touch;

//// explicit template instantiation for some vec types
//extern template class Property_<glm::vec2>;
//extern template class Property_<glm::vec3>;
//extern template class Property_<glm::vec4>;


class Application : public crocore::Component
{
public:

    Application(int argc = 0, char *argv[] = nullptr);

    virtual ~Application();

    int run();

// you are supposed to implement these in a subclass
    virtual void setup() = 0;

    virtual void update(float timeDelta) = 0;

    virtual void draw() = 0;

    virtual void post_draw() = 0;

    virtual void teardown() = 0;

    virtual double get_application_time() = 0;

    virtual std::vector<JoystickState> get_joystick_states() const { return {}; };

    inline bool running() const { return m_running; };

    inline void set_running(bool b) { m_running = b; }

    virtual void set_display_gui(bool b) { m_display_gui = b; };

    inline bool display_gui() const { return m_display_gui; };

//virtual bool fullscreen() const {return m_fullscreen;};
//virtual void set_fullscreen(bool b, int monitor_index){ m_fullscreen = b; };
//void set_fullscreen(bool b = true){ set_fullscreen(b, 0); };

//virtual bool v_sync() const { return false; };
    virtual void set_v_sync(bool b) {};

//virtual void set_cursor_position(float x, float y) = 0;
//virtual glm::vec2 cursor_position() const = 0;
//virtual bool cursor_visible() const { return m_cursorVisible;};
//virtual void set_cursor_visible(bool b = true){ m_cursorVisible = b;};

//virtual bool needs_redraw() const { return true; };

/*!
 * return current frames per second
 */
    float fps() const { return m_framesPerSec; };

/*!
 * the commandline arguments provided at application start
 */
    const std::vector<std::string> &args() const { return m_args; };

///*!
// * returns true if some loading operation is in progress,
// * meaning the number of active tasks is greater than 0.
// * tasks can be announced and removed with calls to inc_task() and dec_task() respectively
// */
//bool is_loading() const;

/*!
 * this queue is processed by the main thread
 */
    crocore::ThreadPool &main_queue() { return m_main_queue; }

    const crocore::ThreadPool &main_queue() const { return m_main_queue; }

/*!
 * the background queue is processed by a background threadpool
 */
    crocore::ThreadPool &background_queue() { return m_background_queue; }

    const crocore::ThreadPool &background_queue() const { return m_background_queue; }

private:

    virtual void init() = 0;

    virtual void poll_events() = 0;

    virtual void swap_buffers() = 0;

    void timing(double timeStamp);

    virtual void draw_internal();

    virtual bool is_running() { return m_running; };

    uint32_t m_framesDrawn;
    double m_lastTimeStamp;
    double m_lastMeasurementTimeStamp;
    float m_framesPerSec;
    double m_timingInterval;

    bool m_running;
    bool m_fullscreen;
    bool m_display_gui;
    bool m_cursorVisible;
    float m_max_fps;

    std::vector<std::string> m_args;

    crocore::ThreadPool m_main_queue, m_background_queue;
};


}
