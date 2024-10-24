#include "test_context.hpp"
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

const std::vector<Vertex> vertices = {{{-0.5f, -0.5f, 0.f}, {1.0f, 0.0f, 0.0f, 1.f}, {0.f, 0.f}},
                                      {{-0.5f, 0.5f, 0.f}, {1.0f, 1.0f, 1.0f, 1.f}, {0.f, 1.f}},
                                      {{0.5f, 0.5f, 0.f}, {0.0f, 0.0f, 1.0f, 1.f}, {1.f, 1.f}},
                                      {{0.5f, -0.5f, 0.f}, {0.0f, 1.0f, 0.0f, 1.f}, {1.f, 0.f}},

                                      {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f, 1.f}, {0.f, 0.f}},
                                      {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.f}, {0.f, 1.f}},
                                      {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f, 1.f}, {1.f, 1.f}},
                                      {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.f}, {1.f, 0.f}}};

const std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::MeshPtr create_mesh(const vierkant::DevicePtr &device, const std::vector<Vertex> &vertices,
                              const std::vector<uint32_t> &indices)
{
    auto ret = vierkant::Mesh::create();

    // vertex attributes
    auto vertex_buffer =
            vierkant::Buffer::create(device, vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    vierkant::vertex_attrib_t position, color, tex_coord;
    position.offset = offsetof(Vertex, position);
    position.stride = sizeof(Vertex);
    position.buffer = vertex_buffer;
    position.format = vierkant::format<decltype(Vertex::position)>();
    ret->vertex_attribs[0] = position;

    color.offset = offsetof(Vertex, color);
    color.stride = sizeof(Vertex);
    color.buffer = vertex_buffer;
    color.format = vierkant::format<decltype(Vertex::color)>();
    ret->vertex_attribs[1] = color;

    tex_coord.offset = offsetof(Vertex, tex_coord);
    tex_coord.stride = sizeof(Vertex);
    tex_coord.buffer = vertex_buffer;
    tex_coord.format = vierkant::format<decltype(Vertex::tex_coord)>();
    ret->vertex_attribs[2] = tex_coord;

    ret->index_buffer =
            vierkant::Buffer::create(device, indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    return ret;
}

vierkant::descriptor_map_t create_descriptors(const vierkant::DevicePtr &device)
{
    // host visible, empty uniform-buffer
    auto uniform_buffer = vierkant::Buffer::create(device, nullptr, sizeof(UniformBuffer),
                                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    // fill Uniformbuffer
    auto ubo = static_cast<UniformBuffer *>(uniform_buffer->map());
    ubo->model = glm::mat4(1);
    ubo->view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo->projection = glm::perspective(glm::radians(45.0f), 16 / 9.f, 0.1f, 10.0f);

    vierkant::Image::Format fmt;
    fmt.extent = {512, 512, 1};
    auto texture = vierkant::Image::create(device, fmt);

    // descriptors
    vierkant::descriptor_t desc_ubo, desc_texture;
    desc_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_ubo.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
    desc_ubo.buffers = {uniform_buffer};

    desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_texture.images = {texture};

    return {{0, desc_ubo}, {1, desc_texture}};
}

TEST(Mesh, Constructor)
{
    // vierkant::Mesh is just a data-struct atm, so this is not really exciting here
    auto m = vierkant::Mesh::create();
}

TEST(Mesh, basic)
{
    vulkan_test_context_t test_context;

    auto mesh = create_mesh(test_context.device, vertices, indices);

    auto descriptors = create_descriptors(test_context.device);

    auto descriptor_set_layout = vierkant::create_descriptor_set_layout(test_context.device, descriptors);

    // construct a pool to hold enough descriptors for the mesh
    vierkant::descriptor_count_t descriptor_counts = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                                                      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

    auto pool = vierkant::create_descriptor_pool(test_context.device, descriptor_counts, 16);

    // use the pool to allocate the actual descriptor-set
    auto descriptor_set =
            vierkant::create_descriptor_set(test_context.device, pool, descriptor_set_layout.get(), false);

    // update the descriptor set
    vierkant::update_descriptor_set(test_context.device, descriptors, descriptor_set);
}

TEST(Mesh, Format)
{
    EXPECT_EQ(vierkant::format<float>(), VK_FORMAT_R32_SFLOAT);
    EXPECT_EQ(vierkant::format<glm::vec2>(), VK_FORMAT_R32G32_SFLOAT);
    EXPECT_EQ(vierkant::format<glm::vec3>(), VK_FORMAT_R32G32B32_SFLOAT);
    EXPECT_EQ(vierkant::format<glm::vec4>(), VK_FORMAT_R32G32B32A32_SFLOAT);
    EXPECT_EQ(vierkant::format<int32_t>(), VK_FORMAT_R32_SINT);
    EXPECT_EQ(vierkant::format<glm::ivec2>(), VK_FORMAT_R32G32_SINT);
    EXPECT_EQ(vierkant::format<glm::ivec3>(), VK_FORMAT_R32G32B32_SINT);
    EXPECT_EQ(vierkant::format<glm::ivec4>(), VK_FORMAT_R32G32B32A32_SINT);
    EXPECT_EQ(vierkant::format<uint32_t>(), VK_FORMAT_R32_UINT);
    EXPECT_EQ(vierkant::format<glm::uvec2>(), VK_FORMAT_R32G32_UINT);
    EXPECT_EQ(vierkant::format<glm::uvec3>(), VK_FORMAT_R32G32B32_UINT);
    EXPECT_EQ(vierkant::format<glm::uvec4>(), VK_FORMAT_R32G32B32A32_UINT);

    // only needed to satisfy freakin EXPECT_EQ
    using u16vec2 = glm::vec<2, uint16_t>;
    using u16vec3 = glm::vec<3, uint16_t>;
    using u16vec4 = glm::vec<4, uint16_t>;

    EXPECT_EQ(vierkant::format<u16vec2>(), VK_FORMAT_R16G16_UINT);
    EXPECT_EQ(vierkant::format<u16vec3>(), VK_FORMAT_R16G16B16_UINT);
    EXPECT_EQ(vierkant::format<u16vec4>(), VK_FORMAT_R16G16B16A16_UINT);
}

TEST(Mesh, IndexType)
{
    EXPECT_EQ(vierkant::index_type<uint16_t>(), VK_INDEX_TYPE_UINT16);
    EXPECT_EQ(vierkant::index_type<uint32_t>(), VK_INDEX_TYPE_UINT32);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
