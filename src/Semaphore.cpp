//
// Created by crocdialer on 1/25/21.
//

#include <vierkant/Semaphore.hpp>

namespace vierkant
{

SemaphorePtr Semaphore::create(const DevicePtr &device, uint64_t initial_value)
{
    return vierkant::SemaphorePtr(new Semaphore(device, initial_value));
}

Semaphore::Semaphore(const vierkant::DevicePtr &device, uint64_t initial_value) :
        m_device(device)
{
    VkSemaphoreTypeCreateInfo timeline_create_info{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    timeline_create_info.pNext = nullptr;
    timeline_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timeline_create_info.initialValue = initial_value;

    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create_info.pNext = &timeline_create_info;

    vkCreateSemaphore(device->handle(), &semaphore_create_info, nullptr, &m_semaphore);
}

Semaphore::~Semaphore()
{
    if(m_semaphore){ vkDestroySemaphore(m_device->handle(), m_semaphore, nullptr); }
}

void Semaphore::signal(uint64_t value)
{
    VkSemaphoreSignalInfo signal_info;
    signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signal_info.pNext = nullptr;
    signal_info.semaphore = m_semaphore;
    signal_info.value = value;

    vkSignalSemaphore(m_device->handle(), &signal_info);
}

void Semaphore::wait(uint64_t value)
{
    VkSemaphoreWaitInfo wait_info;
    wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    wait_info.pNext = nullptr;
    wait_info.flags = 0;
    wait_info.semaphoreCount = 1;
    wait_info.pSemaphores = &m_semaphore;
    wait_info.pValues = &value;
    vkWaitSemaphores(m_device->handle(), &wait_info, std::numeric_limits<uint64_t>::max());
}

}// namespace vierkant
