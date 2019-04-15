//
// Created by crocdialer on 4/13/19.
//

#include "crocore/filesystem.hpp"
#include "vierkant/Application.hpp"

namespace vierkant {

Application::Application(int argc, char *argv[]) :
        Component(argc ? crocore::fs::get_filename_part(argv[0]) : "vierkant_app"),
        m_start_time(std::chrono::high_resolution_clock::now()),
        m_last_timestamp(std::chrono::high_resolution_clock::now()),
        m_timingInterval(1.0),
        m_current_fps(0.f),
        m_running(false),
        m_main_queue(0),
        m_background_queue(2)
{
    srand(clock());
    for(int i = 0; i < argc; i++){ m_args.emplace_back(argv[i]); }
}

int Application::run()
{
    m_running = true;

    // user setup-hook
    setup();

    std::chrono::high_resolution_clock::time_point time_stamp;

    try
    {
        // main loop
        while(m_running)
        {
            // get currennt time
            time_stamp = std::chrono::high_resolution_clock::now();

            // poll io_service if no seperate worker-threads exist
            if(!m_main_queue.get_num_threads()) m_main_queue.poll();

            // poll input events
            glfwPollEvents();

            // time elapsed since last frame
            double time_delta = double_sec_t(time_stamp - m_last_timestamp).count();

            // call update callback
            update(time_delta);

            m_last_timestamp = time_stamp;

            //TODO: draw all windows

            // perform fps-timing
            timing();

            // Check if ESC key was pressed or window was closed or whatever
            m_running = is_running();
        }

        // manage teardown, save stuff etc.
        teardown();

    }catch(std::exception &e)
    {
        LOG_ERROR << e.what();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

double Application::get_application_time()
{
    auto current_time = std::chrono::high_resolution_clock::now();
    return double_sec_t(current_time - m_start_time).count();
}

void Application::timing()
{
    m_num_loop_iterations++;

    double diff = double_sec_t(m_last_timestamp - m_last_measure).count();

    if(diff > m_timingInterval)
    {
        m_current_fps = m_num_loop_iterations / diff;
        m_num_loop_iterations = 0;
        m_last_measure = m_last_timestamp;
    }
}

}
