#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#include "vierkant/vierkant.hpp"

struct Vertex
{
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 tex_coord;
};

struct UniformBuffer
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
};

const std::vector<Vertex> vertices =
        {
                {{-0.5f, -0.5f, 0.f},   {1.0f, 0.0f, 0.0f, 1.f}, {0.f, 0.f}},
                {{-0.5f, 0.5f,  0.f},   {1.0f, 1.0f, 1.0f, 1.f}, {0.f, 1.f}},
                {{0.5f,  0.5f,  0.f},   {0.0f, 0.0f, 1.0f, 1.f}, {1.f, 1.f}},
                {{0.5f,  -0.5f, 0.f},   {0.0f, 1.0f, 0.0f, 1.f}, {1.f, 0.f}},

                {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f, 1.f}, {0.f, 0.f}},
                {{-0.5f, 0.5f,  -0.5f}, {1.0f, 1.0f, 1.0f, 1.f}, {0.f, 1.f}},
                {{0.5f,  0.5f,  -0.5f}, {0.0f, 0.0f, 1.0f, 1.f}, {1.f, 1.f}},
                {{0.5f,  -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.f}, {1.f, 0.f}}
        };

const std::vector<uint32_t> indices =
        {
                0, 1, 2, 0, 2, 3,
                4, 5, 6, 4, 6, 7
        };

///////////////////////////////////////////////////////////////////////////////////////////////////

vk::MeshPtr create_mesh(const vk::DevicePtr &device,
                        const std::vector<Vertex> &vertices,
                        const std::vector<uint32_t> &indices)
{
    auto ret = vk::Mesh::create();

    // vertex attributes
    auto vertex_buffer = vk::Buffer::create(device, vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                            VMA_MEMORY_USAGE_GPU_ONLY);

    vk::Mesh::attrib_t position, color, tex_coord;
    position.offset = offsetof(Vertex, position);
    position.stride = sizeof(Vertex);
    position.buffer = vertex_buffer;
    position.format = vk::format<decltype(Vertex::position)>();
    ret->vertex_attribs[0] = position;

    color.offset = offsetof(Vertex, color);
    color.stride = sizeof(Vertex);
    color.buffer = vertex_buffer;
    color.format = vk::format<decltype(Vertex::color)>();
    ret->vertex_attribs[1] = color;

    tex_coord.offset = offsetof(Vertex, tex_coord);
    tex_coord.stride = sizeof(Vertex);
    tex_coord.buffer = vertex_buffer;
    tex_coord.format = vk::format<decltype(Vertex::tex_coord)>();
    ret->vertex_attribs[2] = tex_coord;

    ret->index_buffer = vk::Buffer::create(device, indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                           VMA_MEMORY_USAGE_GPU_ONLY);
    return ret;
}

vierkant::descriptor_map_t create_descriptors(const vk::DevicePtr &device)
{
    // host visible, empty uniform-buffer
    auto uniform_buffer = vk::Buffer::create(device, nullptr, sizeof(UniformBuffer),
                                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                             VMA_MEMORY_USAGE_CPU_ONLY);
    // fill Uniformbuffer
    auto ubo = static_cast<UniformBuffer*>(uniform_buffer->map());
    ubo->model = glm::mat4(1);
    ubo->view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo->projection = glm::perspective(glm::radians(45.0f), 16 / 9.f, 0.1f, 10.0f);

    vk::Image::Format fmt;
    fmt.extent = {512, 512, 1};
    auto texture = vk::Image::create(device, fmt);

    // descriptors
    vk::descriptor_t desc_ubo, desc_texture;
    desc_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_ubo.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
    desc_ubo.buffers = {uniform_buffer};

    desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_texture.image_samplers = {texture};

    return {{0, desc_ubo}, {1, desc_texture}};
}

BOOST_AUTO_TEST_CASE(TestMesh_Constructor)
{
    // vk::Mesh is just a data-struct atm, so this is not really exciting here
    auto m = vk::Mesh::create();
}

BOOST_AUTO_TEST_CASE(TestMesh)
{
    bool use_validation = true;
    vk::Instance instance(use_validation, {});

    BOOST_CHECK(instance);
    BOOST_CHECK(instance.use_validation_layers() == use_validation);
    BOOST_CHECK(!instance.physical_devices().empty());

    for(auto physical_device : instance.physical_devices())
    {
        vierkant::Device::create_info_t device_info = {};
        device_info.instance = instance.handle();
        device_info.physical_device = physical_device;
        device_info.use_validation = instance.use_validation_layers();
        auto device = vk::Device::create(device_info);

        auto mesh = create_mesh(device, vertices, indices);

        auto descriptors = create_descriptors(device);

        auto descriptor_set_layout = vk::create_descriptor_set_layout(device, descriptors);

        // construct a pool to hold enough descriptors for the mesh
        vk::descriptor_count_t descriptor_counts ={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                                                   {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

        auto pool = vk::create_descriptor_pool(device, descriptor_counts, 16);

        // use the pool to allocate the actual descriptor-set
        auto descriptor_set = vk::create_descriptor_set(device, pool, descriptor_set_layout);

        // update the descriptor set
        vierkant::update_descriptor_set(device, descriptor_set, descriptors);
    }
}

BOOST_AUTO_TEST_CASE(TestFormat)
{
    BOOST_CHECK_EQUAL(vk::format<float>(), VK_FORMAT_R32_SFLOAT);
    BOOST_CHECK_EQUAL(vk::format<glm::vec2>(), VK_FORMAT_R32G32_SFLOAT);
    BOOST_CHECK_EQUAL(vk::format<glm::vec3>(), VK_FORMAT_R32G32B32_SFLOAT);
    BOOST_CHECK_EQUAL(vk::format<glm::vec4>(), VK_FORMAT_R32G32B32A32_SFLOAT);
    BOOST_CHECK_EQUAL(vk::format<int32_t>(), VK_FORMAT_R32_SINT);
    BOOST_CHECK_EQUAL(vk::format<glm::ivec2>(), VK_FORMAT_R32G32_SINT);
    BOOST_CHECK_EQUAL(vk::format<glm::ivec3>(), VK_FORMAT_R32G32B32_SINT);
    BOOST_CHECK_EQUAL(vk::format<glm::ivec4>(), VK_FORMAT_R32G32B32A32_SINT);
    BOOST_CHECK_EQUAL(vk::format<uint32_t>(), VK_FORMAT_R32_UINT);
    BOOST_CHECK_EQUAL(vk::format<glm::uvec2>(), VK_FORMAT_R32G32_UINT);
    BOOST_CHECK_EQUAL(vk::format<glm::uvec3>(), VK_FORMAT_R32G32B32_UINT);
    BOOST_CHECK_EQUAL(vk::format<glm::uvec4>(), VK_FORMAT_R32G32B32A32_UINT);
}

BOOST_AUTO_TEST_CASE(TestIndexType)
{
    BOOST_CHECK_EQUAL(vk::index_type<uint16_t>(), VK_INDEX_TYPE_UINT16);
    BOOST_CHECK_EQUAL(vk::index_type<uint32_t>(), VK_INDEX_TYPE_UINT32);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
