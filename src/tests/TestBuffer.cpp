#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#include "../Buffer.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

void test_buffer(vk::DevicePtr device)
{
    // 1 MB test-bytes
    size_t num_bytes = 1 << 20;

    uint8_t dummy_vals[] = {23, 69, 99};

    // create an empty, host-visible buffer with empty usage-flags
    auto host_buffer = vk::Buffer::create(device, nullptr, num_bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // check for correct size
    BOOST_CHECK(host_buffer->num_bytes() == num_bytes);

    // check host visibility
    BOOST_CHECK(host_buffer->is_host_visible());

    // test mapping to host-memory
    uint8_t *ptr = static_cast<uint8_t *>(host_buffer->map());
    BOOST_CHECK(ptr != nullptr);

    // use ptr to manipulate buffer
    memset(ptr, dummy_vals[0], num_bytes);
    BOOST_CHECK(ptr[num_bytes / 2] == dummy_vals[0]);
    host_buffer->unmap();

    // data can be supplied via std::array and std::vector
    std::vector<uint8_t> dummy_data(num_bytes, dummy_vals[1]);

    // init a gpu only buffer with dummy data from a std::vector (will internally use a staging buffer for upload)
    auto gpu_buffer = vk::Buffer::create(device, dummy_data,
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    // check for correct size
    BOOST_CHECK(gpu_buffer->num_bytes() == dummy_data.size());

    // check host visibility
    BOOST_CHECK(!gpu_buffer->is_host_visible());

    // test failed mapping to host-memory
    ptr = static_cast<uint8_t*>(gpu_buffer->map());
    BOOST_CHECK(ptr == nullptr);

    // download data from gpu-buffer to host-buffer
    gpu_buffer->copy_to(host_buffer);

    // map host-buffer again and compare with original data
    BOOST_CHECK(memcmp(host_buffer->map(), dummy_data.data(), dummy_data.size()) == 0);

    std::fill(dummy_data.begin(), dummy_data.end(), dummy_vals[2]);
    gpu_buffer->set_data(dummy_data);

    // download data from gpu-buffer to host-buffer
    gpu_buffer->copy_to(host_buffer);

    // map host-buffer again and compare with original data
    BOOST_CHECK(memcmp(host_buffer->map(), dummy_data.data(), dummy_data.size()) == 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestBuffer)
{
    bool use_validation = true;
    vk::Instance instance(use_validation, {});

    BOOST_CHECK(instance);
    BOOST_CHECK(instance.use_validation_layers() == use_validation);
    BOOST_CHECK(!instance.physical_devices().empty());

    for(auto physical_device : instance.physical_devices())
    {
        auto device = vk::Device::create(physical_device,
                                         instance.use_validation_layers(),
                                         VK_NULL_HANDLE);
        // run buffer test case
        test_buffer(device);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
