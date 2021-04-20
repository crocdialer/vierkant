//
// Created by crocdialer on 1/25/21.
//

#pragma once

#include <vierkant/Device.hpp>

namespace vierkant
{

struct semaphore_submit_info_t
{
    //! shared semaphore object
    VkSemaphore semaphore = VK_NULL_HANDLE;

    //! wait value
    uint64_t wait_value = 0;

    //! the stage to wait at
    VkPipelineStageFlags wait_stage = 0;

    //! signal value
    uint64_t signal_value = 0;
};

/**
 * @brief   Semaphore provides a timeline semaphore.
 */
class Semaphore
{
public:

    Semaphore() = default;

    Semaphore(const vierkant::DevicePtr &device, uint64_t initial_value = 0);

    ~Semaphore();

    Semaphore(Semaphore &&other) noexcept;

    Semaphore(const Semaphore &) = delete;

    Semaphore &operator=(Semaphore other);

    VkSemaphore handle() const{ return m_handle; }

    void signal(uint64_t value);

    void wait(uint64_t value) const;

    uint64_t value() const;

    inline explicit operator bool() const{ return static_cast<bool>(m_handle); };

    friend void swap(Semaphore &lhs, Semaphore &rhs);

private:

    vierkant::DevicePtr m_device;

    VkSemaphore m_handle = VK_NULL_HANDLE;
};

}// namespace vierkant