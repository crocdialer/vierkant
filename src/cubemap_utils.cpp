#include "vierkant/cubemap_utils.hpp"

#include <array>

#include <vierkant/Compute.hpp>
#include <vierkant/cubemap_data.hpp>
#include <vierkant/shaders_slang.hpp>

namespace vierkant
{

// helper
struct img_copy_assets_t
{
    vierkant::CommandBuffer command_buffer;
    vierkant::FencePtr fence;
};

vierkant::ImagePtr cubemap_generate_mip_maps(const vierkant::cube_pipeline_t &cube, VkCommandPool pool, VkQueue queue,
                                             VkFormat format, img_copy_assets_t &img_copy_asset,
                                             std::vector<VkFence> &fences)
{
    vierkant::Image::Format ret_fmt = {};
    ret_fmt.extent = cube.color_image->extent();
    ret_fmt.format = format;
    ret_fmt.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ret_fmt.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    ret_fmt.num_layers = 6;
    ret_fmt.use_mipmap = true;
    ret_fmt.autogenerate_mipmaps = false;
    ret_fmt.initial_layout_transition = false;

    // create mipmapped output image
    auto mipmap_cube = vierkant::Image::create(cube.device, ret_fmt);

    // copy image into mipmap-chain
    img_copy_asset.command_buffer = vierkant::CommandBuffer(cube.device, pool);
    img_copy_asset.fence = vierkant::create_fence(cube.device);

    img_copy_asset.command_buffer.begin();

    // copy goes here
    VkImageCopy region = {};
    region.extent = cube.color_image->extent();
    region.dstOffset = region.srcOffset = {0, 0, 0};
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount = 6;
    region.srcSubresource.mipLevel = 0;

    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount = 6;
    region.dstSubresource.mipLevel = 0;

    // transition layouts for copying
    cube.color_image->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img_copy_asset.command_buffer.handle());

    mipmap_cube->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, img_copy_asset.command_buffer.handle());

    // actual copy command
    vkCmdCopyImage(img_copy_asset.command_buffer.handle(), cube.color_image->image(), cube.color_image->image_layout(),
                   mipmap_cube->image(), mipmap_cube->image_layout(), 1, &region);

    // generate mipmap-chain
    mipmap_cube->generate_mipmaps(img_copy_asset.command_buffer.handle());

    // submit command, sync
    img_copy_asset.command_buffer.submit(queue, false, img_copy_asset.fence.get());
    fences.push_back(img_copy_asset.fence.get());
    return mipmap_cube;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr cubemap_neutral_environment(const vierkant::DevicePtr &device, uint32_t size, VkQueue queue,
                                               bool mipmap, VkFormat format)
{
    VkImageUsageFlags flags = mipmap ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : VK_IMAGE_USAGE_SAMPLED_BIT;
    auto command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    auto cube = vierkant::create_cube_pipeline(device, command_pool, size, format, queue, false, flags);

    auto ret_img = cube.color_image;

    // set a fragment stage
    cube.drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(vierkant::slang_shaders::unlit::environment_white_slang);

    // stage cube-drawable
    cube.renderer.stage_drawable(cube.drawable);

    std::vector<VkFence> fences;
    auto cmd_buf = cube.renderer.render(cube.framebuffer);
    fences.push_back(cube.framebuffer.submit({cmd_buf}, queue));
    img_copy_assets_t image_copy_asset;

    if(mipmap)
    {
        ret_img = cubemap_generate_mip_maps(cube, command_pool.get(), queue, format, image_copy_asset, fences);
    }

    // mandatory to sync here
    vkWaitForFences(device->handle(), fences.size(), fences.data(), VK_TRUE, std::numeric_limits<uint64_t>::max());

    device->set_object_name(reinterpret_cast<uint64_t>(ret_img->image()), VK_OBJECT_TYPE_IMAGE,
                            "cubemap_neutral_environment");

    return ret_img;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr cubemap_from_panorama(const vierkant::DevicePtr &device, const vierkant::ImagePtr &panorama_img,
                                         VkQueue queue, uint32_t size, bool mipmap, VkFormat format)
{
    if(!panorama_img) { return nullptr; }
    VkImageUsageFlags flags = mipmap ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : VK_IMAGE_USAGE_SAMPLED_BIT;
    auto command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    auto cube = vierkant::create_cube_pipeline(device, command_pool, size, format, queue, false, flags);

    auto ret_img = cube.color_image;

    // set a fragment stage
    cube.drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(vierkant::slang_shaders::unlit::panorama_slang);

    vierkant::descriptor_t desc_image = {};
    desc_image.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_image.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_image.images = {panorama_img};
    cube.drawable.descriptors[1] = desc_image;

    // stage cube-drawable
    cube.renderer.stage_drawable(cube.drawable);

    std::vector<VkFence> fences;

    auto cmd_buf = cube.renderer.render(cube.framebuffer);
    fences.push_back(cube.framebuffer.submit({cmd_buf}, queue));
    img_copy_assets_t image_copy_asset;

    if(mipmap)
    {
        ret_img = cubemap_generate_mip_maps(cube, command_pool.get(), queue, format, image_copy_asset, fences);
    }

    // mandatory to sync here
    vkWaitForFences(device->handle(), fences.size(), fences.data(), VK_TRUE, std::numeric_limits<uint64_t>::max());

    device->set_object_name(reinterpret_cast<uint64_t>(ret_img->image()), VK_OBJECT_TYPE_IMAGE,
                            "cubemap_from_panorama");
    return ret_img;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr create_convolution_lambert(const DevicePtr &device, const ImagePtr &cubemap, uint32_t size,
                                              VkFormat format, VkQueue queue)
{
    // create a cube-pipeline
    auto command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    auto cube = vierkant::create_cube_pipeline(device, command_pool, size, format, queue, false,
                                               VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    cube.drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(vierkant::slang_shaders::unlit::convolve_lambert_slang);

    vierkant::descriptor_t desc_image = {};
    desc_image.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_image.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_image.images = {cubemap};
    cube.drawable.descriptors[1] = desc_image;

    // stage cube-drawable
    cube.renderer.stage_drawable(cube.drawable);

    auto cmd_buf = cube.renderer.render(cube.framebuffer);
    auto fence = cube.framebuffer.submit({cmd_buf}, queue);

    // mandatory to sync here
    vkWaitForFences(device->handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());

    return cube.color_image;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr create_convolution_ggx(const DevicePtr &device, const ImagePtr &cubemap, uint32_t size,
                                          VkFormat format, VkQueue queue)
{
    size = crocore::next_pow_2(size);

    vierkant::Image::Format ret_fmt = {};
    ret_fmt.format = format;
    ret_fmt.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ret_fmt.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    ret_fmt.num_layers = 6;
    ret_fmt.use_mipmap = true;
    ret_fmt.autogenerate_mipmaps = false;
    ret_fmt.initial_layout_transition = false;
    ret_fmt.extent = {size, size, 1};
    ret_fmt.name = "cube_convolution_ggx";

    vierkant::ImagePtr ret = vierkant::Image::create(device, ret_fmt);
    uint32_t num_mips = ret->num_mip_levels();

    // keep cube-pipelines alive
    std::vector<cube_pipeline_t> cube_pipelines(num_mips);

    // command pool for background transfer
    auto command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    vierkant::descriptor_count_t descriptor_counts = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64},
                                                      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32}};
    auto descriptor_pool = vierkant::create_descriptor_pool(device, descriptor_counts, 64);

    std::vector<img_copy_assets_t> image_copy_assets(num_mips);

    // collect fences for all operations
    std::vector<VkFence> fences;

    auto frag_module = vierkant::create_shader_module(vierkant::slang_shaders::unlit::convolve_ggx_slang);

    for(uint32_t lvl = 0; lvl < num_mips; ++lvl)
    {
        auto &cube = cube_pipelines[lvl];

        // create a cube-pipeline
        cube = vierkant::create_cube_pipeline(device, command_pool, size, format, queue, false,
                                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT, descriptor_pool);

        cube.drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] = frag_module;

        // increasing roughness in range [0 .. 1]
        float roughness = static_cast<float>(lvl) / static_cast<float>(num_mips - 1);

        vierkant::descriptor_t desc_ubo = {};
        desc_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_ubo.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        desc_ubo.buffers = {vierkant::Buffer::create(device, &roughness, sizeof(roughness),
                                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU)};
        cube.drawable.descriptors[1] = desc_ubo;

        vierkant::descriptor_t desc_image = {};
        desc_image.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_image.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        desc_image.images = {cubemap};
        cube.drawable.descriptors[2] = desc_image;

        // stage cube-drawable
        cube.renderer.stage_drawable(cube.drawable);

        // issue render-command and submit to queue
        auto cmd_buf = cube.renderer.render(cube.framebuffer);
        fences.push_back(cube.framebuffer.submit({cmd_buf}, queue));

        // copy image into mipmap-chain
        img_copy_assets_t &image_copy_asset = image_copy_assets[lvl];
        image_copy_asset.command_buffer = vierkant::CommandBuffer(device, command_pool.get());
        image_copy_asset.fence = vierkant::create_fence(device);

        image_copy_asset.command_buffer.begin();

        // copy goes here
        VkImageCopy region = {};
        region.extent = cube.color_image->extent();
        region.dstOffset = region.srcOffset = {0, 0, 0};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.baseArrayLayer = 0;
        region.srcSubresource.layerCount = 6;
        region.srcSubresource.mipLevel = 0;

        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.baseArrayLayer = 0;
        region.dstSubresource.layerCount = 6;
        region.dstSubresource.mipLevel = lvl;

        // transition layouts for copying
        cube.color_image->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                            image_copy_asset.command_buffer.handle());

        ret->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image_copy_asset.command_buffer.handle());

        // actual copy command
        vkCmdCopyImage(image_copy_asset.command_buffer.handle(), cube.color_image->image(),
                       cube.color_image->image_layout(), ret->image(), ret->image_layout(), 1, &region);

        image_copy_asset.command_buffer.submit(queue, false, image_copy_asset.fence.get());
        fences.push_back(image_copy_asset.fence.get());

        size = std::max<uint32_t>(size / 2, 1);
    }

    // mandatory to sync here
    vkWaitForFences(device->handle(), fences.size(), fences.data(), VK_TRUE, std::numeric_limits<uint64_t>::max());

    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

cube_pipeline_t create_cube_pipeline(const vierkant::DevicePtr &device, const vierkant::CommandPoolPtr &command_pool,
                                     uint32_t size, VkFormat color_format, VkQueue queue, bool depth,
                                     VkImageUsageFlags usage_flags, const vierkant::DescriptorPoolPtr &descriptor_pool)
{
    // framebuffer image-format
    vierkant::Image::Format img_fmt = {};
    img_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | usage_flags;
    img_fmt.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    img_fmt.num_layers = 6;
    img_fmt.use_mipmap = false;
    img_fmt.format = color_format;
    img_fmt.name = "cube_frame_buffer";

    // create cube framebuffer
    vierkant::Framebuffer::create_info_t fb_create_info = {};
    fb_create_info.size = {size, size, 1};
    fb_create_info.color_attachment_format = img_fmt;
    fb_create_info.num_color_attachments = color_format == VK_FORMAT_UNDEFINED ? 0 : 1;
    fb_create_info.depth = depth;
    fb_create_info.command_pool = command_pool;
    fb_create_info.queue = queue;
    auto cube_fb = vierkant::Framebuffer(device, fb_create_info);

    // render
    vierkant::Rasterizer::create_info_t cuber_render_create_info = {};
    cuber_render_create_info.num_frames_in_flight = 1;
    cuber_render_create_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    cuber_render_create_info.viewport.width = static_cast<float>(cube_fb.extent().width);
    cuber_render_create_info.viewport.height = static_cast<float>(cube_fb.extent().height);
    cuber_render_create_info.viewport.maxDepth = static_cast<float>(cube_fb.extent().depth);
    cuber_render_create_info.descriptor_pool = descriptor_pool;
    auto cube_render = vierkant::Rasterizer(device, cuber_render_create_info);

    // create a drawable
    vierkant::drawable_t drawable = {};
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
            vierkant::create_shader_module(vierkant::slang_shaders::unlit::cube_layers_slang);
    drawable.num_instances = 6;
    drawable.num_vertices = 36;
    drawable.pipeline_format.blend_state.blendEnable = false;
    drawable.pipeline_format.depth_test = false;
    drawable.pipeline_format.depth_write = false;
    drawable.pipeline_format.cull_mode = VK_CULL_MODE_FRONT_BIT;
    drawable.use_own_buffers = true;

    // all we need are vanilla view/projection matrices for 6 directions
    auto cube_cam = vierkant::CubeCamera(.1f, 10.f);

    struct geom_shader_ubo_t
    {
        glm::mat4 view_matrix[6]{};
        glm::mat4 model_matrix = glm::mat4(1);
        glm::mat4 projection_matrix = glm::mat4(1);
    };
    geom_shader_ubo_t ubo_data = {};
    memcpy(ubo_data.view_matrix, cube_cam.view_matrices().data(), sizeof(ubo_data.view_matrix));
    ubo_data.projection_matrix = cube_cam.projection_matrix();

    vierkant::descriptor_t desc_matrices = {};
    desc_matrices.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_matrices.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
    desc_matrices.buffers = {vierkant::Buffer::create(device, &ubo_data, sizeof(ubo_data),
                                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU)};
    drawable.descriptors[0] = desc_matrices;

    cube_pipeline_t ret = {};
    ret.device = device;
    ret.renderer = std::move(cube_render);
    ret.drawable = std::move(drawable);
    ret.color_image = cube_fb.color_attachment(0);
    ret.depth_image = cube_fb.depth_attachment();
    ret.framebuffer = std::move(cube_fb);
    return ret;
}

vierkant::ImagePtr create_BRDF_lut(const vierkant::DevicePtr &device, VkQueue queue)
{
    const glm::vec2 size(512);

    queue = queue ? queue : device->queue();

    // framebuffer image-format
    vierkant::Image::Format img_fmt = {};
    img_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    img_fmt.format = VK_FORMAT_R16G16_SFLOAT;
    img_fmt.name = "brdf_lut";

    // create framebuffer
    vierkant::Framebuffer::create_info_t fb_create_info = {};
    fb_create_info.size = {static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y), 1};
    fb_create_info.color_attachment_format = img_fmt;

    auto framebuffer = vierkant::Framebuffer(device, fb_create_info);

    // render
    vierkant::Rasterizer::create_info_t render_create_info = {};
    render_create_info.num_frames_in_flight = 1;
    render_create_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    render_create_info.viewport.width = static_cast<float>(framebuffer.extent().width);
    render_create_info.viewport.height = static_cast<float>(framebuffer.extent().height);
    render_create_info.viewport.maxDepth = static_cast<float>(framebuffer.extent().depth);
    auto renderer = vierkant::Rasterizer(device, render_create_info);

    // create a drawable
    vierkant::drawable_t drawable = {};
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
            vierkant::create_shader_module(vierkant::slang_shaders::fullscreen::texture_slang);
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(vierkant::slang_shaders::pbr::brdf_lut_slang);

    drawable.num_vertices = 3;

    drawable.pipeline_format.blend_state.blendEnable = false;
    drawable.pipeline_format.depth_test = false;
    drawable.pipeline_format.depth_write = false;
    drawable.use_own_buffers = true;

    // stage drawable
    renderer.stage_drawable(drawable);

    auto cmd_buf = renderer.render(framebuffer);
    auto fence = framebuffer.submit({cmd_buf}, queue);

    // mandatory to sync here
    vkWaitForFences(device->handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());

    return framebuffer.color_attachment(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//! addressing-block of a format: dimensions in texels plus the bytes one block occupies.
struct format_block_t
{
    uint32_t width = 1, height = 1, num_bytes = 0;
};

static format_block_t format_block(VkFormat format)
{
    switch(format)
    {
        // every BC format in use here packs a 4x4 block into 128 bit
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:
        case VK_FORMAT_BC6H_UFLOAT_BLOCK:
        case VK_FORMAT_BC6H_SFLOAT_BLOCK:
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK: return {4, 4, 16};
        default: return {1, 1, static_cast<uint32_t>(vierkant::num_bytes(format))};
    }
}

size_t cubemap_level_num_bytes(VkFormat format, uint32_t level_size)
{
    constexpr uint32_t num_faces = 6;
    const auto block = format_block(format);

    // a mip-level smaller than the block still occupies one full block
    const uint32_t blocks_x = (level_size + block.width - 1) / block.width;
    const uint32_t blocks_y = (level_size + block.height - 1) / block.height;
    return num_faces * blocks_x * blocks_y * block.num_bytes;
}

cubemap_data_t download_cubemap(const vierkant::ImagePtr &cubemap, VkQueue queue)
{
    constexpr uint32_t num_faces = 6;
    if(!cubemap || cubemap->format().num_layers != num_faces) { return {}; }

    const auto &img_fmt = cubemap->format();
    const uint32_t num_mips = cubemap->num_mip_levels();

    cubemap_data_t ret = {};
    ret.format = img_fmt.format;
    ret.size = img_fmt.extent.width;
    ret.levels.resize(num_mips);

    auto device = cubemap->device();
    auto command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    auto cmd_buf = vierkant::CommandBuffer(device, command_pool.get());
    cmd_buf.begin();

    const VkImageLayout prev_layout = cubemap->image_layout();
    cubemap->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cmd_buf.handle());

    std::vector<vierkant::BufferPtr> level_buffers(num_mips);

    for(uint32_t lvl = 0; lvl < num_mips; ++lvl)
    {
        const uint32_t level_size = std::max<uint32_t>(ret.size >> lvl, 1);
        const VkDeviceSize level_bytes = cubemap_level_num_bytes(img_fmt.format, level_size);
        const VkDeviceSize face_bytes = level_bytes / num_faces;

        level_buffers[lvl] = vierkant::Buffer::create(device, nullptr, level_bytes,
                                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);

        std::array<VkBufferImageCopy2, num_faces> regions = {};

        for(uint32_t face = 0; face < num_faces; ++face)
        {
            auto &region = regions[face];
            region.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
            region.bufferOffset = face * face_bytes;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = lvl;
            region.imageSubresource.baseArrayLayer = face;
            region.imageSubresource.layerCount = 1;
            region.imageExtent = {level_size, level_size, 1};
        }

        VkCopyImageToBufferInfo2 copy_info = {};
        copy_info.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2;
        copy_info.regionCount = static_cast<uint32_t>(regions.size());
        copy_info.pRegions = regions.data();
        copy_info.srcImage = cubemap->image();
        copy_info.srcImageLayout = cubemap->image_layout();
        copy_info.dstBuffer = level_buffers[lvl]->handle();
        vkCmdCopyImageToBuffer2(cmd_buf.handle(), &copy_info);
    }

    // leave the image in the layout it was handed over in. an undefined layout carries no contract,
    // but the caller is about to sample it -> settle on read-only.
    cubemap->transition_layout(prev_layout != VK_IMAGE_LAYOUT_UNDEFINED ? prev_layout
                                                                        : VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                               cmd_buf.handle());
    cmd_buf.submit(queue, true);

    for(uint32_t lvl = 0; lvl < num_mips; ++lvl)
    {
        const auto *ptr = static_cast<const uint8_t *>(level_buffers[lvl]->map());
        ret.levels[lvl] = {ptr, ptr + level_buffers[lvl]->num_bytes()};
        level_buffers[lvl]->unmap();
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr upload_cubemap(const vierkant::DevicePtr &device, const cubemap_data_t &data, VkQueue queue)
{
    constexpr uint32_t num_faces = 6;
    if(!device || data.levels.empty() || !data.size || data.format == VK_FORMAT_UNDEFINED) { return nullptr; }

    vierkant::Image::Format fmt = {};
    fmt.format = data.format;
    fmt.extent = {data.size, data.size, 1};
    fmt.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    fmt.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    fmt.num_layers = num_faces;
    fmt.use_mipmap = data.levels.size() > 1;
    fmt.autogenerate_mipmaps = false;
    fmt.initial_layout_transition = false;

    auto ret = vierkant::Image::create(device, fmt);

    auto command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    auto cmd_buf = vierkant::CommandBuffer(device, command_pool.get());
    cmd_buf.begin();
    ret->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmd_buf.handle());

    const auto num_levels = std::min<uint32_t>(static_cast<uint32_t>(data.levels.size()), ret->num_mip_levels());
    std::vector<vierkant::BufferPtr> level_buffers(num_levels);

    for(uint32_t lvl = 0; lvl < num_levels; ++lvl)
    {
        const uint32_t level_size = std::max<uint32_t>(data.size >> lvl, 1);
        const VkDeviceSize face_bytes = data.levels[lvl].size() / num_faces;

        level_buffers[lvl] = vierkant::Buffer::create(device, data.levels[lvl], VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                      VMA_MEMORY_USAGE_CPU_ONLY);

        for(uint32_t face = 0; face < num_faces; ++face)
        {
            ret->copy_from(level_buffers[lvl], cmd_buf.handle(), face * face_bytes, {},
                           {level_size, level_size, 1}, face, lvl);
        }
    }
    ret->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, cmd_buf.handle());
    cmd_buf.submit(queue, true);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

cubemap_data_t compress_cubemap(const vierkant::ImagePtr &cubemap, VkQueue queue)
{
    constexpr uint32_t num_faces = 6;
    constexpr VkFormat bc6h_format = VK_FORMAT_BC6H_UFLOAT_BLOCK;

    if(!cubemap || cubemap->format().num_layers != num_faces) { return {}; }

    //! mirrors bc6h_params_t in compress_bc6h.slang
    struct bc6h_params_t
    {
        uint32_t texture_size[2];
        uint32_t size_in_blocks[2];
        uint32_t mip_level;
    };

    const auto &img_fmt = cubemap->format();
    const uint32_t num_mips = cubemap->num_mip_levels();

    cubemap_data_t ret = {};
    ret.format = bc6h_format;
    ret.size = img_fmt.extent.width;
    ret.levels.resize(num_mips);

    auto device = cubemap->device();

    // the cube-image is array-compatible, so it can be read per-face through a 2d-array view
    VkImageViewCreateInfo view_create_info = {};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = cubemap->image();
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    view_create_info.format = img_fmt.format;
    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = num_mips;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = num_faces;

    VkImageView raw_view = VK_NULL_HANDLE;
    vkCheck(vkCreateImageView(device->handle(), &view_create_info, nullptr, &raw_view),
            "compress_cubemap: failed to create array-view");
    vierkant::VkImageViewPtr array_view(raw_view, [device](VkImageView v) {
        vkDestroyImageView(device->handle(), v, nullptr);
    });

    // Compute's default descriptor-pool has no SAMPLED_IMAGE, which is what a Texture2DArray binds as
    vierkant::descriptor_count_t descriptor_counts = {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, num_mips},
                                                      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, num_mips}};
    vierkant::Compute::create_info_t compute_info = {};
    compute_info.descriptor_pool = vierkant::create_descriptor_pool(device, descriptor_counts, num_mips);
    vierkant::Compute compute(device, compute_info);

    auto shader_stage = vierkant::create_shader_module(vierkant::slang_shaders::slang::compress_bc6h_slang);
    const auto local_size = *shader_stage.entry_points.at(VK_SHADER_STAGE_COMPUTE_BIT)[0].group_count;

    // the range has to be part of the pipeline-info: it is the key the pipeline is created from
    VkPushConstantRange push_constant_range = {};
    push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(bc6h_params_t);

    auto command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    auto cmd_buf = vierkant::CommandBuffer(device, command_pool.get());
    cmd_buf.begin();

    // the shader samples the cubemap, the descriptor picks up whatever layout it is in
    const VkImageLayout prev_layout = cubemap->image_layout();
    cubemap->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, cmd_buf.handle());

    std::vector<vierkant::BufferPtr> block_buffers(num_mips), host_buffers(num_mips);
    std::vector<vierkant::Compute::computable_t> computables(num_mips);

    for(uint32_t lvl = 0; lvl < num_mips; ++lvl)
    {
        const uint32_t level_size = std::max<uint32_t>(ret.size >> lvl, 1);
        const uint32_t blocks_per_side = (level_size + 3) / 4;
        const VkDeviceSize level_bytes = cubemap_level_num_bytes(bc6h_format, level_size);

        block_buffers[lvl] =
                vierkant::Buffer::create(device, nullptr, level_bytes,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY);
        host_buffers[lvl] = vierkant::Buffer::create(device, nullptr, level_bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                     VMA_MEMORY_USAGE_GPU_TO_CPU);

        bc6h_params_t push_constants = {};
        push_constants.texture_size[0] = push_constants.texture_size[1] = level_size;
        push_constants.size_in_blocks[0] = push_constants.size_in_blocks[1] = blocks_per_side;
        push_constants.mip_level = lvl;

        auto &computable = computables[lvl];
        computable.pipeline_info.shader_stage = shader_stage;
        computable.pipeline_info.push_constant_ranges = {push_constant_range};
        computable.extent = {vierkant::group_count(blocks_per_side, local_size.x),
                             vierkant::group_count(blocks_per_side, local_size.y), num_faces};
        computable.push_constants.resize(sizeof(bc6h_params_t));
        memcpy(computable.push_constants.data(), &push_constants, sizeof(bc6h_params_t));

        auto &desc_src = computable.descriptors[0];
        desc_src.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        desc_src.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
        desc_src.images = {cubemap};
        desc_src.image_views = {array_view.get()};

        auto &desc_blocks = computable.descriptors[1];
        desc_blocks.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_blocks.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
        desc_blocks.buffers = {block_buffers[lvl]};
    }
    compute.dispatch(computables, cmd_buf.handle());

    for(uint32_t lvl = 0; lvl < num_mips; ++lvl)
    {
        block_buffers[lvl]->barrier(cmd_buf.handle(), VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                    VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT,
                                    VK_ACCESS_2_TRANSFER_READ_BIT);
        block_buffers[lvl]->copy_to(host_buffers[lvl], cmd_buf.handle());
    }
    cubemap->transition_layout(prev_layout != VK_IMAGE_LAYOUT_UNDEFINED ? prev_layout
                                                                        : VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                               cmd_buf.handle());
    cmd_buf.submit(queue, true);

    for(uint32_t lvl = 0; lvl < num_mips; ++lvl)
    {
        const auto *ptr = static_cast<const uint8_t *>(host_buffers[lvl]->map());
        ret.levels[lvl] = {ptr, ptr + host_buffers[lvl]->num_bytes()};
        host_buffers[lvl]->unmap();
    }
    return ret;
}

}// namespace vierkant
