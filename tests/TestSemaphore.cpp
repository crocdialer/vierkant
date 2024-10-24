#include "test_context.hpp"

#include <vierkant/Semaphore.hpp>
#include <vierkant/CommandBuffer.hpp>

TEST(Semaphore, DefaultConstructor)
{
    vierkant::Semaphore semaphore;

    // checks operator bool
    EXPECT_TRUE(!semaphore);

    EXPECT_EQ(semaphore.handle(), nullptr);
    EXPECT_EQ(semaphore.value(), 0);
}

TEST(Semaphore, Constructor)
{
    vulkan_test_context_t testContext;

    auto semaphore = vierkant::Semaphore(testContext.device);

    // checks operator bool
    EXPECT_TRUE(semaphore);

    EXPECT_NE(semaphore.handle(), nullptr);
    EXPECT_EQ(semaphore.value(), 0);

    // reassign
    semaphore = vierkant::Semaphore(testContext.device, 42);
    EXPECT_NE(semaphore.handle(), nullptr);
    EXPECT_EQ(semaphore.value(), 42);
}

TEST(Semaphore, Submission)
{
    vulkan_test_context_t testContext;

    auto semaphore = vierkant::Semaphore(testContext.device);

    // signal value on gpu, wait on host
    constexpr uint64_t signalValue = 42;
    vierkant::semaphore_submit_info_t semaphoreSubmitInfo = {};
    semaphoreSubmitInfo.semaphore = semaphore.handle();
    semaphoreSubmitInfo.signal_value = signalValue;

    // submit an empty list of commandbuffers, sync host via fence
    vierkant::submit(testContext.device, testContext.device->queue(), {}, true,
                     VK_NULL_HANDLE, {semaphoreSubmitInfo});

    EXPECT_EQ(semaphore.value(), signalValue);

    // reset semaphore
    semaphore = vierkant::Semaphore(testContext.device);
    semaphoreSubmitInfo.semaphore = semaphore.handle();
    semaphoreSubmitInfo.signal_value = signalValue;

    // submit an empty list of commandbuffers, sync host via semaphore
    vierkant::submit(testContext.device, testContext.device->queue(), {}, false,
                     VK_NULL_HANDLE, {semaphoreSubmitInfo});

    semaphore.wait(signalValue);
}

TEST(Semaphore, WaitBeforeSignal)
{
    vulkan_test_context_t testContext;

    auto semaphore = vierkant::Semaphore(testContext.device, 0);

    // two independent queues
    auto queue1 = testContext.device->queues(vierkant::Device::Queue::COMPUTE).front().queue;
    auto queue2 = testContext.device->queues(vierkant::Device::Queue::GRAPHICS).back().queue;

    constexpr uint64_t signal1 = 42, signal2 = 666;

    // wait for 1st signal on gpu, then issue 2nd signal
    vierkant::semaphore_submit_info_t waitBeforeSignalInfo = {};
    waitBeforeSignalInfo.semaphore = semaphore.handle();
    waitBeforeSignalInfo.wait_value = signal1;
    waitBeforeSignalInfo.wait_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    waitBeforeSignalInfo.signal_value = signal2;

    // signal value on gpu, same semaphore
    vierkant::semaphore_submit_info_t signalInfo = {};
    signalInfo.semaphore = semaphore.handle();
    signalInfo.signal_value = signal1;

    // submit, wait on gpu for signal1, issue signal2
    vierkant::submit(testContext.device, queue1, {}, false, VK_NULL_HANDLE, {waitBeforeSignalInfo});

    // submit, issue signal1
    vierkant::submit(testContext.device, queue2, {}, false, VK_NULL_HANDLE, {signalInfo});

    semaphore.wait(signal2);
    EXPECT_EQ(semaphore.value(), signal2);
}
