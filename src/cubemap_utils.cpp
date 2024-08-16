#include "vierkant/cubemap_utils.hpp"
#include <vierkant/shaders.hpp>

namespace vierkant
{

// helper
struct img_copy_assets_t
{
    vierkant::CommandBuffer command_buffer;
    vierkant::FencePtr fence;
};

vierkant::ImagePtr cubemap_generate_mip_maps(const vierkant::cube_pipeline_t &cube, VkQueue queue, VkFormat format,
                                             img_copy_assets_t &img_copy_asset, std::vector<VkFence> &fences)
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
    img_copy_asset.command_buffer = vierkant::CommandBuffer(cube.device, cube.command_pool.get());
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
    auto cube = vierkant::create_cube_pipeline(device, size, format, queue, false, flags);

    auto ret_img = cube.color_image;

    // set a fragment stage
    cube.drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::unlit::environment_white_frag);

    // stage cube-drawable
    cube.renderer.stage_drawable(cube.drawable);

    std::vector<VkFence> fences;
    auto cmd_buf = cube.renderer.render(cube.framebuffer);
    fences.push_back(cube.framebuffer.submit({cmd_buf}, queue));
    img_copy_assets_t image_copy_asset;

    if(mipmap) { ret_img = cubemap_generate_mip_maps(cube, queue, format, image_copy_asset, fences); }

    // mandatory to sync here
    vkWaitForFences(device->handle(), fences.size(), fences.data(), VK_TRUE, std::numeric_limits<uint64_t>::max());

    return ret_img;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr cubemap_from_panorama(const vierkant::DevicePtr &device, const vierkant::ImagePtr &panorama_img,
                                         VkQueue queue, uint32_t size, bool mipmap, VkFormat format)
{
    if(!panorama_img) { return nullptr; }

    VkImageUsageFlags flags = mipmap ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : VK_IMAGE_USAGE_SAMPLED_BIT;
    auto cube = vierkant::create_cube_pipeline(device, size, format, queue, false, flags);

    auto ret_img = cube.color_image;

    // set a fragment stage
    cube.drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::unlit::panorama_frag);

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

    if(mipmap) { ret_img = cubemap_generate_mip_maps(cube, queue, format, image_copy_asset, fences); }

    // mandatory to sync here
    vkWaitForFences(device->handle(), fences.size(), fences.data(), VK_TRUE, std::numeric_limits<uint64_t>::max());

    return ret_img;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr create_convolution_lambert(const DevicePtr &device, const ImagePtr &cubemap, uint32_t size,
                                              VkFormat format, VkQueue queue)
{
    // create a cube-pipeline
    auto cube = vierkant::create_cube_pipeline(device, size, format, queue, false, VK_IMAGE_USAGE_SAMPLED_BIT);

    cube.drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::pbr::convolve_lambert_frag);

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
    ret_fmt.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ret_fmt.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    ret_fmt.num_layers = 6;
    ret_fmt.use_mipmap = true;
    ret_fmt.autogenerate_mipmaps = false;
    ret_fmt.extent = {size, size, 1};

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

    auto frag_module = vierkant::create_shader_module(device, vierkant::shaders::pbr::convolve_ggx_frag);

    for(uint32_t lvl = 0; lvl < num_mips; ++lvl)
    {
        auto &cube = cube_pipelines[lvl];

        // create a cube-pipeline
        cube = vierkant::create_cube_pipeline(device, size, format, queue, false,
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
        image_copy_asset.command_buffer = vierkant::CommandBuffer(device, cube.command_pool.get());
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

cube_pipeline_t create_cube_pipeline(const vierkant::DevicePtr &device, uint32_t size, VkFormat color_format,
                                     VkQueue queue, bool depth, VkImageUsageFlags usage_flags,
                                     const vierkant::DescriptorPoolPtr &descriptor_pool)
{
    auto command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    // framebuffer image-format
    vierkant::Image::Format img_fmt = {};
    img_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | usage_flags;
    img_fmt.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    img_fmt.num_layers = 6;
    img_fmt.use_mipmap = false;
    img_fmt.format = color_format;

    // create cube framebuffer
    vierkant::Framebuffer::create_info_t fb_create_info = {};
    fb_create_info.size = {size, size, 1};
    fb_create_info.color_attachment_format = img_fmt;
    fb_create_info.num_color_attachments = color_format == VK_FORMAT_UNDEFINED ? 0 : 1;
    fb_create_info.depth = depth;
    fb_create_info.command_pool = command_pool;
    fb_create_info.queue = queue;
    auto cube_fb = vierkant::Framebuffer(device, fb_create_info);

    // create cube pipeline with vertex- + geometry-stages

    // render
    vierkant::Rasterizer::create_info_t cuber_render_create_info = {};
    //    cuber_render_create_info.renderpass = cube_fb.renderpass();
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
            vierkant::create_shader_module(device, vierkant::shaders::cube::cube_vert);

    auto cmd_buffer = vierkant::CommandBuffer(device, command_pool.get());
    cmd_buffer.begin();

    auto box = vierkant::Geometry::Box();
    box->colors.clear();
    box->tex_coords.clear();
    box->normals.clear();
    box->tangents.clear();

    vierkant::Mesh::create_info_t mesh_create_info = {};
    mesh_create_info.command_buffer = cmd_buffer.handle();
    mesh_create_info.staging_buffer =
            vierkant::Buffer::create(device, nullptr, 0, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    drawable.mesh = vierkant::Mesh::create_from_geometry(device, box, mesh_create_info);
    cmd_buffer.submit(queue, true);

    drawable.num_instances = 6;
    const auto &mesh_entry = drawable.mesh->entries.front();
    const auto &lod_0 = mesh_entry.lods.front();
    drawable.base_index = lod_0.base_index;
    drawable.num_indices = lod_0.num_indices;
    drawable.vertex_offset = mesh_entry.vertex_offset;
    drawable.num_vertices = mesh_entry.num_vertices;

    drawable.pipeline_format.binding_descriptions =
            vierkant::create_binding_descriptions(drawable.mesh->vertex_attribs);
    drawable.pipeline_format.attribute_descriptions =
            vierkant::create_attribute_descriptions(drawable.mesh->vertex_attribs);
    drawable.pipeline_format.primitive_topology = mesh_entry.primitive_type;
    drawable.pipeline_format.blend_state.blendEnable = false;
    drawable.pipeline_format.depth_test = false;
    drawable.pipeline_format.depth_write = false;
    drawable.pipeline_format.cull_mode = VK_CULL_MODE_FRONT_BIT;
    drawable.use_own_buffers = true;

    auto cube_cam = vierkant::CubeCamera::create(.1f, 10.f);

    struct geom_shader_ubo_t
    {
        glm::mat4 view_matrix[6]{};
        glm::mat4 model_matrix = glm::mat4(1);
        glm::mat4 projection_matrix = glm::mat4(1);
    };
    geom_shader_ubo_t ubo_data = {};
    memcpy(ubo_data.view_matrix, cube_cam->view_matrices().data(), sizeof(ubo_data.view_matrix));
    ubo_data.projection_matrix = cube_cam->projection_matrix();

    vierkant::descriptor_t desc_matrices = {};
    desc_matrices.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_matrices.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;//VK_SHADER_STAGE_GEOMETRY_BIT;
    desc_matrices.buffers = {vierkant::Buffer::create(device, &ubo_data, sizeof(ubo_data),
                                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU)};
    drawable.descriptors[0] = desc_matrices;

    cube_pipeline_t ret = {};
    ret.device = device;
    ret.command_pool = command_pool;
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
            vierkant::create_shader_module(device, vierkant::shaders::fullscreen::texture_vert);
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::pbr::brdf_lut_frag);

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

}// namespace vierkant