#include "heap_test_utils.hpp"
#include <set>
#include <test_shaders/shaders_slang.hpp>
#include <vierkant/Compute.hpp>
#include <vierkant/heap_blocks.hpp>

using heap_test::descriptor_heap_supported;

namespace
{

constexpr uint32_t num_textures = 3;
constexpr uint32_t num_values = 9 + num_textures;

using values_t = std::array<glm::uvec4, num_values>;

glm::uvec4 bits(const glm::vec4 &v) { return glm::floatBitsToUint(v); }

//! one distinct color per call
glm::vec4 next_color()
{
    static uint32_t i = 0;
    i++;
    return {static_cast<float>(i), static_cast<float>(i) + 0.25f, static_cast<float>(i) + 0.5f, -1.f};
}

//! resources and descriptors for heap_descriptors_test.slang, plus the values it must write
struct scene_t
{
    vierkant::descriptor_map_t descriptors;
    vierkant::BufferPtr out_buffer;
    values_t expected = {};
};

vierkant::ImagePtr create_image(const vierkant::DevicePtr &device, vierkant::Image::Format fmt,
                                const std::vector<glm::vec4> &colors, VkImageLayout final_layout)
{
    fmt.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    fmt.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    fmt.autogenerate_mipmaps = false;
    auto img = vierkant::Image::create(device, fmt);

    // one color per mip, then per layer
    vierkant::CommandBuffer cmd(device, device->command_pool_transient());
    cmd.begin();
    img->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmd.handle());
    for(uint32_t i = 0; i < colors.size(); ++i)
    {
        VkClearColorValue clear = {};
        memcpy(clear.float32, &colors[i], sizeof(glm::vec4));
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if(img->num_mip_levels() > 1) { range.baseMipLevel = i; }
        else
        {
            range.baseArrayLayer = i;
        }
        vkCmdClearColorImage(cmd.handle(), img->image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
    }
    img->transition_layout(final_layout, cmd.handle());
    cmd.submit(device->queue(), true);
    return img;
}

scene_t create_scene(const vierkant::DevicePtr &device, const glm::uvec4 &ubo_value)
{
    const auto &limits = device->properties().core.limits;
    scene_t ret;
    auto &descriptors = ret.descriptors;

    // uniform- and storage-buffer, read at a non-zero offset. offset 0 holds a different value.
    auto ubo_offset = limits.minUniformBufferOffsetAlignment;
    std::vector<glm::uvec4> ubo_data(2 * ubo_offset / sizeof(glm::uvec4), glm::uvec4(0xdead));
    ubo_data[ubo_offset / sizeof(glm::uvec4)] = ubo_value;
    auto ubo =
            vierkant::Buffer::create(device, ubo_data, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    descriptors[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT};
    descriptors[0].buffers = {ubo};
    descriptors[0].buffer_ranges = {{ubo_offset, sizeof(glm::uvec4)}};
    ret.expected[0] = ubo_value;

    auto ssbo_offset = limits.minStorageBufferOffsetAlignment;
    glm::uvec4 ssbo_value = ubo_value + 1u;
    std::vector<glm::uvec4> ssbo_data(2 * ssbo_offset / sizeof(glm::uvec4) + 1, glm::uvec4(0xbeef));
    ssbo_data[ssbo_offset / sizeof(glm::uvec4)] = ssbo_value;
    auto ssbo = vierkant::Buffer::create(device, ssbo_data, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU);
    descriptors[1] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT};
    descriptors[1].buffers = {ssbo};
    descriptors[1].buffer_ranges = {{ssbo_offset, 0}};
    ret.expected[1] = ssbo_value;

    vierkant::Image::Format fmt = {};
    fmt.extent = {1, 1, 1};

    // sampled image through a swizzled default view
    auto sampled_color = next_color();
    auto sampled = create_image(device, fmt, {sampled_color}, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                           ->clone({VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_G,
                                    VK_COMPONENT_SWIZZLE_G});
    descriptors[2] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT};
    descriptors[2].images = {sampled};
    ret.expected[2] = bits(glm::vec4(sampled_color.y));

    auto storage_color = next_color();
    auto storage_fmt = fmt;
    storage_fmt.usage = VK_IMAGE_USAGE_STORAGE_BIT;
    descriptors[3] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT};
    descriptors[3].images = {create_image(device, storage_fmt, {storage_color}, VK_IMAGE_LAYOUT_GENERAL)};
    ret.expected[3] = bits(storage_color);

    // 2x1 texels, sampled at u = 1.25: clamp-to-edge reads texel 1, repeat reads texel 0
    std::array<glm::vec4, 2> texels = {next_color(), next_color()};
    auto texel_fmt = fmt;
    texel_fmt.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    texel_fmt.extent = {2, 1, 1};
    texel_fmt.sampler_state.min_filter = texel_fmt.sampler_state.mag_filter = VK_FILTER_NEAREST;
    texel_fmt.sampler_state.mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    texel_fmt.initial_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    auto clamped = vierkant::Image::create(device, texels.data(), texel_fmt);
    auto repeat_state = texel_fmt.sampler_state;
    repeat_state.address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    auto repeated = clamped->clone();
    repeated->set_sampler(repeat_state);
    descriptors[4] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT};
    descriptors[4].images = {clamped, repeated};
    ret.expected[4] = bits(texels[1]);
    ret.expected[5] = bits(texels[0]);

    // mip-view: level 1 of three
    std::vector<glm::vec4> mip_colors = {next_color(), next_color(), next_color()};
    auto mip_fmt = fmt;
    mip_fmt.extent = {4, 4, 1};
    mip_fmt.use_mipmap = true;
    auto mip_image = create_image(device, mip_fmt, mip_colors, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    descriptors[5] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT};
    descriptors[5].images = {mip_image};
    descriptors[5].image_views = {mip_image->mip_image_views()[1]};
    ret.expected[6] = bits(mip_colors[1]);

    // 2d-array view over a cube, layer 4
    std::vector<glm::vec4> face_colors(6);
    for(auto &c: face_colors) { c = next_color(); }
    auto cube_fmt = fmt;
    cube_fmt.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    cube_fmt.num_layers = 6;
    auto cube = create_image(device, cube_fmt, face_colors, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto array_info = cube->view().info;
    array_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    auto array_view = vierkant::create_image_view(device, array_info);
    descriptors[6] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT};
    descriptors[6].images = {cube};
    descriptors[6].image_views = {array_view};
    ret.expected[7] = bits(face_colors[4]);

    ret.out_buffer = vierkant::Buffer::create(device, nullptr, sizeof(values_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                              VMA_MEMORY_USAGE_GPU_TO_CPU);
    descriptors[7] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT};
    descriptors[7].buffers = {ret.out_buffer};

    // variable-count array
    // inline uniform-block
    glm::uvec4 inline_value = ubo_value + 2u;
    descriptors[8] = {VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, VK_SHADER_STAGE_COMPUTE_BIT};
    descriptors[8].inline_uniform_block.resize(sizeof(glm::uvec4));
    memcpy(descriptors[8].inline_uniform_block.data(), &inline_value, sizeof(glm::uvec4));
    ret.expected[8] = inline_value;

    descriptors[9] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT};
    descriptors[9].variable_count = true;
    for(uint32_t i = 0; i < num_textures; ++i)
    {
        auto color = next_color();
        descriptors[9].images.push_back(create_image(device, fmt, {color}, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
        ret.expected[9 + i] = bits(color);
    }
    return ret;
}

values_t read_values(const vierkant::BufferPtr &buffer)
{
    values_t ret;
    memcpy(ret.data(), buffer->map(), sizeof(values_t));
    buffer->unmap();
    return ret;
}

void expect_values(const values_t &values, const values_t &expected, const std::string &label)
{
    for(uint32_t i = 0; i < num_values; ++i) { EXPECT_EQ(values[i], expected[i]) << label << ": value " << i; }
}

vierkant::shader_module_t test_shader()
{ return vierkant::create_shader_module(vierkant::slang_shaders::shaders::heap_descriptors_test_slang); }

}// namespace

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(HeapBlocks, Layout)
{
    if(!descriptor_heap_supported()) { GTEST_SKIP() << "VK_EXT_descriptor_heap not supported"; }
    vulkan_test_context_t test_context(vierkant::DescriptorHeap::required_extensions());
    const auto &props = test_context.device->properties().descriptor_heap;

    vierkant::descriptor_map_t descriptors;
    descriptors[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT};
    descriptors[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT};
    descriptors[1].images.resize(2);
    descriptors[2] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT};
    descriptors[2].variable_count = true;
    auto layout = vierkant::create_heap_block_layout(test_context.device, descriptors);

    ASSERT_EQ(layout.bindings.size(), 3);
    for(const auto &b: layout.bindings)
    {
        bool image = b.type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        EXPECT_EQ(b.offset % (image ? props.imageDescriptorAlignment : props.bufferDescriptorAlignment), 0);
        EXPECT_GE(b.stride, image ? props.imageDescriptorSize : props.bufferDescriptorSize);
    }
    EXPECT_EQ(layout.bindings[1].count, 2);
    EXPECT_GE(layout.bindings[1].offset, layout.bindings[0].offset + layout.bindings[0].stride);
    EXPECT_GE(layout.bindings[2].offset, layout.bindings[1].offset + 2 * layout.bindings[1].stride);
    EXPECT_EQ(layout.num_sampler_bytes, 2 * layout.bindings[1].sampler_stride);

    // the variable-count binding is not part of the fixed block-size
    EXPECT_EQ(layout.num_bytes, layout.bindings[1].offset + 2 * layout.bindings[1].stride);

    // a variable-count binding must be last
    descriptors[3] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT};
    EXPECT_THROW(vierkant::create_heap_block_layout(test_context.device, descriptors), std::runtime_error);
    descriptors.erase(3);

    // inline uniform-blocks: raw data, aligned for uniform-buffer reads
    const auto ubo_alignment = test_context.device->properties().core.limits.minUniformBufferOffsetAlignment;
    descriptors[2] = {VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, VK_SHADER_STAGE_COMPUTE_BIT};
    descriptors[2].inline_uniform_block.resize(20);
    layout = vierkant::create_heap_block_layout(test_context.device, descriptors);
    EXPECT_EQ(layout.bindings[2].offset % ubo_alignment, 0);
    EXPECT_GE(layout.bindings[2].stride, 20);
    EXPECT_EQ(layout.alignment % ubo_alignment, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(HeapBlocks, RoundTrip)
{
    if(!descriptor_heap_supported()) { GTEST_SKIP() << "VK_EXT_descriptor_heap not supported"; }
    vulkan_test_context_t test_context(vierkant::DescriptorHeap::required_extensions());
    const auto &device = test_context.device;

    // two blocks, distinguished by their uniform-buffer and output
    auto scene_a = create_scene(device, glm::uvec4(1, 2, 3, 4));
    auto scene_b = create_scene(device, glm::uvec4(5, 6, 7, 8));

    VkPushConstantRange push_range = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t)};

    // reference: descriptor-sets
    {
        vierkant::descriptor_count_t counts = {
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4},         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8},
                {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32},         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8}, {VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 256}};
        vierkant::Compute::create_info_t compute_info = {};
        compute_info.pipeline_cache = vierkant::PipelineCache::create(device);
        compute_info.descriptor_pool = vierkant::create_descriptor_pool(device, counts, 8);
        vierkant::Compute compute(device, compute_info);

        std::vector<vierkant::Compute::computable_t> computables;
        for(const auto *scene: {&scene_a, &scene_b})
        {
            auto &computable = computables.emplace_back();
            computable.pipeline_info.shader_stage = test_shader();
            computable.pipeline_info.push_constant_ranges = {push_range};
            computable.extent = {1, 1, 1};
            computable.descriptors = scene->descriptors;

            // Compute does not allocate variable descriptor-counts, bind the array with a fixed count
            computable.descriptors[9].variable_count = false;
            computable.push_constants.resize(sizeof(uint32_t));
            memcpy(computable.push_constants.data(), &num_textures, sizeof(uint32_t));
        }
        vierkant::CommandBuffer cmd(device, device->command_pool_transient());
        cmd.begin();
        compute.dispatch(computables, cmd.handle());
        cmd.submit(device->queue(), true);

        expect_values(read_values(scene_a.out_buffer), scene_a.expected, "descriptor-set, block a");
        expect_values(read_values(scene_b.out_buffer), scene_b.expected, "descriptor-set, block b");
    }

    // clear outputs
    for(const auto *scene: {&scene_a, &scene_b})
    {
        memset(scene->out_buffer->map(), 0, sizeof(values_t));
        scene->out_buffer->unmap();
    }

    // descriptor-heap
    auto heap = vierkant::DescriptorHeap::create({device});
    vierkant::HeapBlockAllocator heap_blocks(heap);

    auto layout = std::make_shared<vierkant::heap_block_layout_t>(
            vierkant::create_heap_block_layout(device, scene_a.descriptors));

    vierkant::compute_pipeline_info_t pipeline_info = {};
    pipeline_info.shader_stage = test_shader();
    pipeline_info.push_constant_ranges = {push_range};
    pipeline_info.heap_block_layouts = {layout};
    auto pipeline = vierkant::Pipeline::create(device, pipeline_info);
    EXPECT_EQ(pipeline->layout(), VK_NULL_HANDLE);

    struct push_data_t
    {
        uint32_t num_textures;
        vierkant::heap_block_ref_t block;
    };

    vierkant::CommandBuffer cmd(device, device->command_pool_transient());
    cmd.begin();
    heap->bind(cmd.handle());
    pipeline->bind(cmd.handle());

    for(const auto *scene: {&scene_a, &scene_b})
    {
        push_data_t push_data = {};
        push_data.num_textures = num_textures;
        push_data.block = heap_blocks.write(*layout, scene->descriptors);

        VkPushDataInfoEXT push_info = {};
        push_info.sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT;
        push_info.offset = 0;
        push_info.data = {&push_data, sizeof(push_data)};
        vkCmdPushDataEXT(cmd.handle(), &push_info);
        vkCmdDispatch(cmd.handle(), 1, 1, 1);
    }
    cmd.submit(device->queue(), true);

    expect_values(read_values(scene_a.out_buffer), scene_a.expected, "descriptor-heap, block a");
    expect_values(read_values(scene_b.out_buffer), scene_b.expected, "descriptor-heap, block b");
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(HeapBlocks, SamplerBlocks)
{
    if(!descriptor_heap_supported()) { GTEST_SKIP() << "VK_EXT_descriptor_heap not supported"; }
    vulkan_test_context_t test_context(vierkant::DescriptorHeap::required_extensions());
    const auto &props = test_context.device->properties().descriptor_heap;
    auto heap = vierkant::DescriptorHeap::create({test_context.device});

    const auto stride = static_cast<uint32_t>(props.samplerDescriptorSize);
    vierkant::sampler_state_t linear = {}, nearest = {};
    nearest.min_filter = nearest.mag_filter = VK_FILTER_NEAREST;

    // equal requests share a block
    auto a = heap->sampler_block({{0, linear}, {stride, nearest}}, 2 * stride);
    auto b = heap->sampler_block({{0, linear}, {stride, nearest}}, 2 * stride);
    EXPECT_EQ(a, b);

    // a different state, order or size gets its own block
    auto c = heap->sampler_block({{0, nearest}, {stride, nearest}}, 2 * stride);
    auto d = heap->sampler_block({{0, nearest}, {stride, linear}}, 2 * stride);
    auto e = heap->sampler_block({{0, linear}, {stride, nearest}}, 3 * stride);
    for(const auto &other: {c, d, e}) { EXPECT_NE(a->offset(), other->offset()); }

    // the bytes differ with the state
    const auto *data = static_cast<const uint8_t *>(heap->buffer(vierkant::DescriptorHeapType::SAMPLER)->map());
    EXPECT_NE(memcmp(data + a->offset(), data + c->offset(), stride), 0);
    EXPECT_EQ(memcmp(data + a->offset() + stride, data + c->offset() + stride, stride), 0);

    // dropping all holders returns every range to the heap
    a.reset();
    b.reset();
    c.reset();
    d.reset();
    e.reset();
    const auto &bind = heap->bind_info(vierkant::DescriptorHeapType::SAMPLER);
    VkDeviceSize reserved = (bind.reservedRangeSize + props.samplerDescriptorAlignment - 1) /
                            props.samplerDescriptorAlignment * props.samplerDescriptorAlignment;
    EXPECT_NO_THROW(heap->create_range(vierkant::DescriptorHeapType::SAMPLER, bind.heapRange.size - reserved));

    // a dropped block can be requested again
    EXPECT_NO_THROW(heap->sampler_block({{0, linear}}, stride));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(HeapBlocks, ManyAllocators)
{
    if(!descriptor_heap_supported()) { GTEST_SKIP() << "VK_EXT_descriptor_heap not supported"; }
    vulkan_test_context_t test_context(vierkant::DescriptorHeap::required_extensions());
    const auto &device = test_context.device;
    auto heap = vierkant::DescriptorHeap::create({device});

    vierkant::Image::Format fmt = {};
    fmt.extent = {4, 4, 1};
    fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    auto image = vierkant::Image::create(device, fmt);

    vierkant::descriptor_map_t descriptors;
    descriptors[0] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT};
    descriptors[0].images = {image};
    auto layout = vierkant::create_heap_block_layout(device, descriptors);

    // one allocator per frame-slot of each binder. far more than the sampler-heap holds sampler-ranges.
    constexpr uint32_t num_allocators = 1024;
    std::vector<vierkant::HeapBlockAllocator> allocators(num_allocators, vierkant::HeapBlockAllocator(heap));
    std::set<uint32_t> sampler_offsets;
    for(auto &allocator: allocators)
    {
        ASSERT_NO_THROW(sampler_offsets.insert(allocator.write(layout, descriptors).sampler_offset));
    }
    EXPECT_EQ(sampler_offsets.size(), 1);
}
