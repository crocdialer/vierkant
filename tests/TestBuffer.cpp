#include "test_context.hpp"
#include "vierkant/vierkant.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

void test_buffer(const vierkant::DevicePtr &device, const vierkant::VmaPoolPtr &pool_host = nullptr,
                 const vierkant::VmaPoolPtr &pool_gpu = nullptr)
{
    // 1 MB test-bytes
    size_t num_bytes = 1 << 20;

    uint8_t dummy_vals[] = {23, 69, 99};

    // create an empty, host-visible buffer with empty usage-flags
    auto host_buffer = vierkant::Buffer::create(device, nullptr, num_bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                VMA_MEMORY_USAGE_CPU_ONLY, pool_host);

    // check for correct size
    EXPECT_TRUE(host_buffer->num_bytes() == num_bytes);

    // check host visibility
    EXPECT_TRUE(host_buffer->is_host_visible());

    // test mapping to host-memory
    auto *ptr = static_cast<uint8_t *>(host_buffer->map());
    EXPECT_TRUE(ptr != nullptr);

    // use ptr to manipulate buffer
    memset(ptr, dummy_vals[0], num_bytes);
    EXPECT_TRUE(ptr[num_bytes / 2] == dummy_vals[0]);
    host_buffer->unmap();

    // data can be supplied via std::array and std::vector
    std::vector<uint8_t> dummy_data(num_bytes, dummy_vals[1]);

    // init a gpu only buffer with dummy data from a std::vector (will internally use a staging buffer for upload)
    auto gpu_buffer = vierkant::Buffer::create(device, dummy_data,
                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                               VMA_MEMORY_USAGE_GPU_ONLY, pool_gpu);
    // check for correct size
    EXPECT_TRUE(gpu_buffer->num_bytes() == dummy_data.size());

    // check host visibility
    EXPECT_TRUE(!gpu_buffer->is_host_visible());

    // buffer device address
    EXPECT_TRUE(gpu_buffer->device_address() != VkDeviceAddress(0));

    // test failed mapping to host-memory
    ptr = static_cast<uint8_t *>(gpu_buffer->map());
    EXPECT_TRUE(ptr == nullptr);

    // download data from gpu-buffer to host-buffer
    gpu_buffer->copy_to(host_buffer);

    // map host-buffer again and compare with original data
    EXPECT_TRUE(memcmp(host_buffer->map(), dummy_data.data(), dummy_data.size()) == 0);

    std::fill(dummy_data.begin(), dummy_data.end(), dummy_vals[2]);
    gpu_buffer->set_data(dummy_data);

    // download data from gpu-buffer to host-buffer
    gpu_buffer->copy_to(host_buffer);

    // map host-buffer again and compare with original data
    EXPECT_TRUE(memcmp(host_buffer->map(), dummy_data.data(), dummy_data.size()) == 0);

    // create an empty buffer that does not allocate any memory initially
    auto empty_buffer = vierkant::Buffer::create(device, nullptr, 0,
                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                 VMA_MEMORY_USAGE_GPU_ONLY, pool_gpu);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(TestBuffer, basic)
{
    vulkan_test_context_t test_context;

    // run buffer test case
    test_buffer(test_context.device);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(TestBuffer, BufferPool)
{
    vulkan_test_context_t test_context;

    auto pool = vierkant::Buffer::create_pool(test_context.device,
                                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                              VMA_MEMORY_USAGE_GPU_ONLY);
    EXPECT_TRUE(pool);

    auto pool_buddy_host = vierkant::Buffer::create_pool(test_context.device,
                                                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                         VMA_MEMORY_USAGE_CPU_ONLY);
    EXPECT_TRUE(pool_buddy_host);

    auto pool_buddy = vierkant::Buffer::create_pool(test_context.device,
                                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                    VMA_MEMORY_USAGE_GPU_ONLY);
    EXPECT_TRUE(pool_buddy);

    // run buffer test case
    test_buffer(test_context.device, pool_buddy_host, pool_buddy);
}