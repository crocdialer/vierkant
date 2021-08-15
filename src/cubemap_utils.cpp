//
// Created by crocdialer on 7/2/20.
//

#include <vierkant/shaders.hpp>
#include "vierkant/cubemap_utils.hpp"

namespace vierkant
{

// helper
struct img_copy_assets_t
{
    vierkant::CommandBuffer command_buffer;
    vierkant::FencePtr fence;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr cubemap_from_panorama(const vierkant::ImagePtr &panorama_img, const glm::vec2 &size,
                                         VkQueue queue,
                                         bool mipmap,
                                         VkFormat format)
{
    if(!panorama_img){ return nullptr; }

    auto device = panorama_img->device();

    VkImageUsageFlags flags = mipmap ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : VK_IMAGE_USAGE_SAMPLED_BIT;
    auto cube = vierkant::create_cube_pipeline(device, size.x, format, queue, false, flags);

    auto ret_img = cube.color_image;

    // set a fragment stage
    cube.drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::unlit::panorama_frag);

    vierkant::descriptor_t desc_image = {};
    desc_image.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_image.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_image.image_samplers = {panorama_img};
    cube.drawable.descriptors[1] = desc_image;

    // stage cube-drawable
    cube.renderer.stage_drawable(cube.drawable);

    std::vector<VkFence> fences;

    auto cmd_buf = cube.renderer.render(cube.framebuffer);
    fences.push_back(cube.framebuffer.submit({cmd_buf}, queue));

    img_copy_assets_t image_copy_asset;

    if(mipmap)
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
        auto mipmap_cube = vierkant::Image::create(device, ret_fmt);
        ret_img = mipmap_cube;

        // copy image into mipmap-chain
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
        region.dstSubresource.mipLevel = 0;

        // transition layouts for copying
        cube.color_image->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                            image_copy_asset.command_buffer.handle());

        mipmap_cube->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image_copy_asset.command_buffer.handle());

        // actual copy command
        vkCmdCopyImage(image_copy_asset.command_buffer.handle(), cube.color_image->image(),
                       cube.color_image->image_layout(), mipmap_cube->image(), mipmap_cube->image_layout(), 1, &region);

        // generate mipmap-chain
        mipmap_cube->generate_mipmaps(image_copy_asset.command_buffer.handle());

        // submit command, sync
        image_copy_asset.command_buffer.submit(queue, false, image_copy_asset.fence.get());
        fences.push_back(image_copy_asset.fence.get());
    }

    // mandatory to sync here
    vkWaitForFences(device->handle(), fences.size(), fences.data(), VK_TRUE, std::numeric_limits<uint64_t>::max());

    return ret_img;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr create_convolution_lambert(const DevicePtr &device, const ImagePtr &cubemap, uint32_t size,
                                              VkQueue queue)
{
    // create a cube-pipeline
    auto cube = vierkant::create_cube_pipeline(device, size, VK_FORMAT_R16G16B16A16_SFLOAT, queue, false,
                                               VK_IMAGE_USAGE_SAMPLED_BIT);

    cube.drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::pbr::convolve_lambert_frag);

    vierkant::descriptor_t desc_image = {};
    desc_image.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_image.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_image.image_samplers = {cubemap};
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
                                          VkQueue queue)
{
    size = crocore::next_pow_2(size);

    vierkant::Image::Format ret_fmt = {};
    ret_fmt.format = VK_FORMAT_R16G16B16A16_SFLOAT;
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

    std::vector<img_copy_assets_t> image_copy_assets(num_mips);

    // collect fences for all operations
    std::vector<VkFence> fences;

    auto frag_module = vierkant::create_shader_module(device, vierkant::shaders::pbr::convolve_ggx_frag);

    for(uint32_t lvl = 0; lvl < num_mips; ++lvl)
    {
        auto &cube = cube_pipelines[lvl];

        // create a cube-pipeline
        cube = vierkant::create_cube_pipeline(device, size, VK_FORMAT_R16G16B16A16_SFLOAT, queue, false,
                                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

        cube.drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] = frag_module;

        // increasing roughness in range [0 .. 1]
        float roughness = lvl / static_cast<float>(num_mips - 1);

        vierkant::descriptor_t desc_ubo = {};
        desc_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_ubo.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        desc_ubo.buffers = {vierkant::Buffer::create(device, &roughness, sizeof(roughness),
                                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU)};
        cube.drawable.descriptors[1] = desc_ubo;

        vierkant::descriptor_t desc_image = {};
        desc_image.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_image.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        desc_image.image_samplers = {cubemap};
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

        ret->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               image_copy_asset.command_buffer.handle());

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
                                     VkQueue queue,
                                     bool depth, VkImageUsageFlags usage_flags)
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
    vierkant::Renderer::create_info_t cuber_render_create_info = {};
//    cuber_render_create_info.renderpass = cube_fb.renderpass();
    cuber_render_create_info.num_frames_in_flight = 1;
    cuber_render_create_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    cuber_render_create_info.viewport.width = cube_fb.extent().width;
    cuber_render_create_info.viewport.height = cube_fb.extent().height;
    cuber_render_create_info.viewport.maxDepth = cube_fb.extent().depth;
    auto cube_render = vierkant::Renderer(device, cuber_render_create_info);

    // create a drawable
    vierkant::Renderer::drawable_t drawable = {};
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::cube::cube_vert);
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_GEOMETRY_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::cube::cube_layers_geom);

    auto cmd_buffer = vierkant::CommandBuffer(device, command_pool.get());
    cmd_buffer.begin();

    auto box = vierkant::Geometry::Box();
    box->colors.clear();
    box->tex_coords.clear();
    box->normals.clear();
    box->tangents.clear();

    vierkant::Mesh::create_info_t mesh_create_info = {};
    mesh_create_info.command_buffer = cmd_buffer.handle();
    mesh_create_info.staging_buffer = vierkant::Buffer::create(device, nullptr, 0, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                               VMA_MEMORY_USAGE_CPU_ONLY);

    drawable.mesh = vierkant::Mesh::create_from_geometry(device, box, mesh_create_info);
    cmd_buffer.submit(queue, true);

    const auto &mesh_entry = drawable.mesh->entries.front();
    drawable.base_index = mesh_entry.base_index;
    drawable.num_indices = mesh_entry.num_indices;
    drawable.base_vertex = mesh_entry.base_vertex;
    drawable.num_vertices = mesh_entry.num_vertices;

    drawable.pipeline_format.binding_descriptions = drawable.mesh->binding_descriptions();
    drawable.pipeline_format.attribute_descriptions = drawable.mesh->attribute_descriptions();
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
    desc_matrices.stage_flags = VK_SHADER_STAGE_GEOMETRY_BIT;
    desc_matrices.buffers = {vierkant::Buffer::create(device, &ubo_data, sizeof(ubo_data),
                                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU)};
    drawable.descriptors[0] = desc_matrices;

    cube_pipeline_t ret = {};
    ret.command_pool = command_pool;
    ret.renderer = std::move(cube_render);
    ret.drawable = std::move(drawable);
    ret.color_image = cube_fb.color_attachment(0);
    ret.depth_image = cube_fb.depth_attachment();
    ret.framebuffer = std::move(cube_fb);
    return ret;
}

}// namespace vierkant