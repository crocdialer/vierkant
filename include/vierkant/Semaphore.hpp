//
// Created by crocdialer on 1/25/21.
//

#pragma once

#include <vierkant/Device.hpp>

namespace vierkant
{

DEFINE_CLASS_PTR(Semaphore)

/**
 * @brief   Semaphore provides a timeline semaphore.
 */
class Semaphore
{
public:

    static SemaphorePtr create(const vierkant::DevicePtr& device, uint64_t initial_value = 0);

    ~Semaphore();

    Semaphore(const Semaphore &) = delete;

    Semaphore(Semaphore &&) = delete;

    Semaphore &operator=(Semaphore) = delete;

    VkSemaphore handle() const{ return m_semaphore; }

    void signal(uint64_t value);

    void wait(uint64_t value);

private:

    Semaphore(const vierkant::DevicePtr& device, uint64_t initial_value);

    vierkant::DevicePtr m_device;

    VkSemaphore m_semaphore = VK_NULL_HANDLE;
};
}// namespace vierkant