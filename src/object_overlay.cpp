//
// Created by crocdialer on 13.11.23.
//

#include <vierkant/Compute.hpp>
#include <vierkant/object_overlay.hpp>
#include <vierkant/shaders.hpp>
#include <vierkant/staging_copy.hpp>
#include <volk.h>

namespace vierkant
{

struct object_overlay_context_t
{
    vierkant::BufferPtr id_buffer;
    vierkant::BufferPtr param_buffer;
    vierkant::BufferPtr staging_buffer;

    vierkant::Compute mask_compute;
    glm::uvec3 mask_compute_local_size{};
    vierkant::Compute::computable_t mask_computable;
    vierkant::ImagePtr result, result_swizzle;
};

struct alignas(16) object_overlay_ubo_t
{
    VkDeviceAddress id_buffer_address;
    uint32_t num_object_ids;
    uint32_t silhouette;
};

object_overlay_context_ptr create_object_overlay_context(const DevicePtr &device, const glm::vec2 &size)
{
    auto ret =
            object_overlay_context_ptr(new object_overlay_context_t, std::default_delete<object_overlay_context_t>());
    vierkant::Buffer::create_info_t internal_buffer_info = {};
    internal_buffer_info.device = device;
    internal_buffer_info.num_bytes = 1U << 10U;
    internal_buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    internal_buffer_info.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;

    ret->id_buffer = vierkant::Buffer::create(internal_buffer_info);

    vierkant::Buffer::create_info_t param_buffer_info = {};
    param_buffer_info.device = device;
    param_buffer_info.num_bytes = 1U << 8U;
    param_buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    param_buffer_info.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;
    ret->param_buffer = vierkant::Buffer::create(param_buffer_info);

    vierkant::Buffer::create_info_t staging_buffer_info = {};
    staging_buffer_info.device = device;
    staging_buffer_info.num_bytes = 1U << 10U;
    staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer_info.mem_usage = VMA_MEMORY_USAGE_CPU_ONLY;
    ret->staging_buffer = vierkant::Buffer::create(staging_buffer_info);

    vierkant::Compute::create_info_t compute_info = {};
    compute_info.pipeline_cache = nullptr;
    ret->mask_compute = vierkant::Compute(device, compute_info);

    // overlay-compute
    auto shader_stage = vierkant::create_shader_module(device, vierkant::shaders::pbr::object_overlay_comp,
                                                       &ret->mask_compute_local_size);
    ret->mask_computable.pipeline_info.shader_stage = shader_stage;

    vierkant::Image::Format img_fmt = {};
    img_fmt.extent = {static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y), 1};
    img_fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    img_fmt.initial_layout_transition = false;
    ret->result = vierkant::Image::create(device, img_fmt);

    // create a swizzle-able imageview
    VkImageViewCreateInfo view_create_info = {};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = ret->result->image();
    view_create_info.viewType = ret->result->format().view_type;
    view_create_info.format = ret->result->format().format;
    view_create_info.components = {VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE,
                                   VK_COMPONENT_SWIZZLE_R};
    view_create_info.subresourceRange.aspectMask = ret->result->format().aspect;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    vkCheck(vkCreateImageView(device->handle(), &view_create_info, nullptr, &image_view),
            "failed to create texture image view!");
    vierkant::VkImageViewPtr swizzle_view = {
            image_view, [device](VkImageView v) { vkDestroyImageView(device->handle(), v, nullptr); }};
    ret->result_swizzle = ret->result->clone();
    ret->result_swizzle->set_image_view(swizzle_view);
    return ret;
}

vierkant::ImagePtr object_overlay(const object_overlay_context_ptr &context, const object_overlay_params_t &params)
{
    // debug label
    vierkant::begin_label(params.commandbuffer, {"object_overlay"});

    std::vector<uint32_t> id_array = {params.object_ids.begin(), params.object_ids.end()};
    if(params.mode == ObjectOverlayMode::None){ id_array.clear(); }

    vierkant::staging_copy_info_t copy_ids = {};
    copy_ids.num_bytes = sizeof(uint32_t) * id_array.size();
    copy_ids.data = id_array.data();
    copy_ids.dst_buffer = context->id_buffer;
    copy_ids.dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    copy_ids.dst_access = VK_ACCESS_2_SHADER_READ_BIT;

    object_overlay_ubo_t object_overlay_ubo = {};
    object_overlay_ubo.silhouette = static_cast<uint32_t>(params.mode == ObjectOverlayMode::Silhouette);
    object_overlay_ubo.id_buffer_address = context->id_buffer->device_address();
    object_overlay_ubo.num_object_ids = id_array.size();

    vierkant::staging_copy_info_t copy_ubo = {};
    copy_ubo.num_bytes = sizeof(object_overlay_ubo_t);
    copy_ubo.data = &object_overlay_ubo;
    copy_ubo.dst_buffer = context->param_buffer;
    copy_ubo.dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    copy_ubo.dst_access = VK_ACCESS_2_SHADER_READ_BIT;

    // id-buffer/ubo upload
    vierkant::staging_copy_context_t staging_context = {};
    staging_context.command_buffer = params.commandbuffer;
    staging_context.staging_buffer = context->staging_buffer;
    vierkant::staging_copy(staging_context, {copy_ids, copy_ubo});

    // run compute over input-image dimension
    auto computable = context->mask_computable;
    computable.extent = {vierkant::group_count(context->result->width(), context->mask_compute_local_size.x),
                         vierkant::group_count(context->result->height(), context->mask_compute_local_size.y), 1};

    auto &desc_id_img = computable.descriptors[0];
    desc_id_img.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    desc_id_img.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    desc_id_img.images = {params.object_id_img};

    auto &desc_mask_img = computable.descriptors[1];
    desc_mask_img.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    desc_mask_img.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    desc_mask_img.images = {context->result};

    auto &desc_param_ubo = computable.descriptors[2];
    desc_param_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_param_ubo.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    desc_param_ubo.buffers = {context->param_buffer};

    // dispatch
    auto prev_input_layout = params.object_id_img->image_layout();
    params.object_id_img->transition_layout(VK_IMAGE_LAYOUT_GENERAL, params.commandbuffer);
    context->result->transition_layout(VK_IMAGE_LAYOUT_GENERAL, params.commandbuffer);
    context->mask_compute.dispatch({computable}, params.commandbuffer);
    params.object_id_img->transition_layout(prev_input_layout, params.commandbuffer);
    context->result->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, params.commandbuffer);
    vierkant::end_label(params.commandbuffer);
    return context->result_swizzle;
}

}// namespace vierkant