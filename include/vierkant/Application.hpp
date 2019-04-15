#pragma once

#include <unordered_map>
#include <chrono>

#include <crocore/Component.hpp>
#include "crocore/ThreadPool.hpp"

#include "vierkant/vierkant.hpp"
#include "vierkant/Input.hpp"

namespace vierkant {

DEFINE_CLASS_PTR(Application);

class Application : public crocore::Component
{
public:

    explicit Application(int argc = 0, char *argv[] = nullptr);

    // you are supposed to implement these in a subclass
    virtual void setup() = 0;

    virtual void update(double timeDelta) = 0;

    virtual void teardown() = 0;

    int run();

    double get_application_time();

    virtual std::vector<JoystickState> get_joystick_states() const { return {}; };

    inline bool running() const { return m_running; };

    inline void set_running(bool b) { m_running = b; }

    /**
    * return current frames per second
    */
    float fps() const { return m_current_fps; };

    /**
    * the commandline arguments provided at application start
    */
    const std::vector<std::string> &args() const { return m_args; };

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

    // 1 double per second
    using double_sec_t = std::chrono::duration<double, std::chrono::seconds::period>;

    void timing();

    virtual bool is_running() { return m_running; };

    // timing
    size_t m_num_loop_iterations = 0;
    std::chrono::high_resolution_clock::time_point m_start_time, m_last_timestamp, m_last_measure;
    double m_timingInterval = 1;

    float m_current_fps;

    bool m_running;

    std::vector<std::string> m_args;

    crocore::ThreadPool m_main_queue, m_background_queue;
};


}
