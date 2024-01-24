#include <vierkant/Semaphore.hpp>

namespace vierkant
{

Semaphore::Semaphore(const vierkant::DevicePtr &device, uint64_t initial_value) :
        m_device(device)
{
    VkSemaphoreTypeCreateInfo timeline_create_info{};
    timeline_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timeline_create_info.pNext = nullptr;
    timeline_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timeline_create_info.initialValue = initial_value;

    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create_info.pNext = &timeline_create_info;

    vkCreateSemaphore(device->handle(), &semaphore_create_info, nullptr, &m_handle);
}

Semaphore::Semaphore(Semaphore &&other) noexcept: Semaphore()
{
    swap(*this, other);
}

Semaphore::~Semaphore()
{
    if(m_handle){ vkDestroySemaphore(m_device->handle(), m_handle, nullptr); }
}

Semaphore &Semaphore::operator=(Semaphore other)
{
    swap(*this, other);
    return *this;
}

void Semaphore::signal(uint64_t value)
{
    if(m_handle)
    {
        VkSemaphoreSignalInfo signal_info;
        signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signal_info.pNext = nullptr;
        signal_info.semaphore = m_handle;
        signal_info.value = value;

        vkSignalSemaphore(m_device->handle(), &signal_info);
    }
}

void Semaphore::wait(uint64_t value) const
{
    if(m_handle)
    {
        VkSemaphoreWaitInfo wait_info;
        wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wait_info.pNext = nullptr;
        wait_info.flags = 0;
        wait_info.semaphoreCount = 1;
        wait_info.pSemaphores = &m_handle;
        wait_info.pValues = &value;
        vkWaitSemaphores(m_device->handle(), &wait_info, std::numeric_limits<uint64_t>::max());
    }
}

uint64_t Semaphore::value() const
{
    if(m_handle)
    {
        uint64_t counter = 0;
        vkGetSemaphoreCounterValue(m_device->handle(), m_handle, &counter);
        return counter;
    }
    return 0;
}

void swap(Semaphore &lhs, Semaphore &rhs)
{
    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_handle, rhs.m_handle);
}

}// namespace vierkant
