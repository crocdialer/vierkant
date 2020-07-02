//
// Created by crocdialer on 7/2/20.
//

#include <vierkant/shaders.hpp>
#include "vierkant/cubemap_utils.hpp"

namespace vierkant
{
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr cubemap_from_panorama(const vierkant::ImagePtr &panorama_img, const glm::vec2 &size)
{
    if(!panorama_img){ return nullptr; }

    auto device = panorama_img->device();

    auto cube = vierkant::create_cube_pipeline(device, size.x, VK_FORMAT_R16G16B16A16_SFLOAT, false);

    // set a fragment stage
    cube.drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::unlit_panorama_frag);

    vierkant::descriptor_t desc_image = {};
    desc_image.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_image.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_image.image_samplers = {panorama_img};
    cube.drawable.descriptors[1] = desc_image;

    // stage cube-drawable
    cube.renderer.stage_drawable(cube.drawable);

    auto cmd_buf = cube.renderer.render(cube.framebuffer);
    auto fence = cube.framebuffer.submit({cmd_buf}, device->queue());

    // mandatory to sync here
    vkWaitForFences(device->handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());

    return cube.color_image;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr create_diffuse_convolution(const DevicePtr &device, const ImagePtr &cubemap, uint32_t size)
{
    // create a cube-pipeline
    auto cube = vierkant::create_cube_pipeline(device, size, VK_FORMAT_R16G16B16A16_SFLOAT, false);

    cube.drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::convolve_diffuse_frag);

    vierkant::descriptor_t desc_image = {};
    desc_image.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_image.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_image.image_samplers = {cubemap};
    cube.drawable.descriptors[1] = desc_image;

    // stage cube-drawable
    cube.renderer.stage_drawable(cube.drawable);

    auto cmd_buf = cube.renderer.render(cube.framebuffer);
    auto fence = cube.framebuffer.submit({cmd_buf}, device->queue());

    // mandatory to sync here
    vkWaitForFences(device->handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());

    return cube.color_image;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr create_specular_convolution(const DevicePtr &device, const ImagePtr &cubemap, uint32_t size)
{
    // create a cube-pipeline
    auto cube = vierkant::create_cube_pipeline(device, size, VK_FORMAT_R16G16B16A16_SFLOAT, false);

    cube.drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::convolve_specular_frag);

    float roughness = 0.f;

    vierkant::descriptor_t desc_ubo = {};
    desc_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_ubo.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_ubo.buffer = vierkant::Buffer::create(device, &roughness, sizeof(roughness),
                                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    cube.drawable.descriptors[1] = desc_ubo;

    vierkant::descriptor_t desc_image = {};
    desc_image.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_image.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_image.image_samplers = {cubemap};
    cube.drawable.descriptors[2] = desc_image;

    // stage cube-drawable
    cube.renderer.stage_drawable(cube.drawable);

    // TODO: mipmapchain

    auto cmd_buf = cube.renderer.render(cube.framebuffer);
    auto fence = cube.framebuffer.submit({cmd_buf}, device->queue());

    // mandatory to sync here
    vkWaitForFences(device->handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());

    return cube.color_image;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

cube_pipeline_t create_cube_pipeline(const vierkant::DevicePtr &device, uint32_t size, VkFormat color_format,
                                     bool depth)
{
    // framebuffer image-format
    vierkant::Image::Format img_fmt = {};
    img_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    img_fmt.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    img_fmt.num_layers = 6;
    img_fmt.format = color_format;

    // create cube framebuffer
    vierkant::Framebuffer::create_info_t fb_create_info = {};
    fb_create_info.size = {size, size, 1};
    fb_create_info.color_attachment_format = img_fmt;
    fb_create_info.num_color_attachments = color_format == VK_FORMAT_UNDEFINED ? 0 : 1;
    fb_create_info.depth = depth;

    auto cube_fb = vierkant::Framebuffer(device, fb_create_info);

    // create cube pipeline with vertex- + geometry-stages

    // render
    vierkant::Renderer::create_info_t cuber_render_create_info = {};
    cuber_render_create_info.renderpass = cube_fb.renderpass();
    cuber_render_create_info.num_frames_in_flight = 1;
    cuber_render_create_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    cuber_render_create_info.viewport.width = cube_fb.extent().width;
    cuber_render_create_info.viewport.height = cube_fb.extent().height;
    cuber_render_create_info.viewport.maxDepth = cube_fb.extent().depth;
    auto cube_render = vierkant::Renderer(device, cuber_render_create_info);

    // create a drawable
    vierkant::Renderer::drawable_t drawable = {};
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::cube_vert);
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_GEOMETRY_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::cube_layers_geom);

    drawable.mesh = vierkant::Mesh::create_from_geometries(device, {vierkant::Geometry::Box()});
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
        glm::mat4 view_matrix[6];
        glm::mat4 model_matrix = glm::mat4(1);
        glm::mat4 projection_matrix = glm::mat4(1);
    };
    geom_shader_ubo_t ubo_data = {};
    memcpy(ubo_data.view_matrix, cube_cam->view_matrices().data(), sizeof(ubo_data.view_matrix));
    ubo_data.projection_matrix = cube_cam->projection_matrix();

    vierkant::descriptor_t desc_matrices = {};
    desc_matrices.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_matrices.stage_flags = VK_SHADER_STAGE_GEOMETRY_BIT;
    desc_matrices.buffer = vierkant::Buffer::create(device, &ubo_data, sizeof(ubo_data),
                                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    drawable.descriptors[0] = desc_matrices;

    vierkant::ImagePtr color_img, depth_img;

    // ref to color-attachment
    auto attach_it = cube_fb.attachments().find(vierkant::Framebuffer::AttachmentType::Color);
    if(attach_it != cube_fb.attachments().end() && !attach_it->second.empty()){ color_img = attach_it->second.front(); }

    // ref to depth-attachment
    attach_it = cube_fb.attachments().find(vierkant::Framebuffer::AttachmentType::DepthStencil);
    if(attach_it != cube_fb.attachments().end() && !attach_it->second.empty()){ depth_img = attach_it->second.front(); }

    cube_pipeline_t ret = {};
    ret.framebuffer = std::move(cube_fb);
    ret.renderer = std::move(cube_render);
    ret.drawable = std::move(drawable);
    ret.color_image = std::move(color_img);
    ret.depth_image = std::move(depth_img);
    return ret;
}

}// namespace vierkant