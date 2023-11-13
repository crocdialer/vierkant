//
// Created by crocdialer on 13.11.23.
//

#include <vierkant/Compute.hpp>
#include <vierkant/object_overlay.hpp>
#include <vierkant/shaders.hpp>
#include <vierkant/staging_copy.hpp>

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
    vierkant::ImagePtr result;
};

struct alignas(16) object_overlay_ubo_t
{
    VkDeviceAddress id_buffer_address;
    uint32_t num_object_ids;
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
    ret->param_buffer = vierkant::Buffer::create(internal_buffer_info);

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
    img_fmt.format = VK_FORMAT_R8_UNORM;
    img_fmt.initial_layout_transition = false;
    ret->result = vierkant::Image::create(device, img_fmt);
    return ret;
}

vierkant::ImagePtr object_overlay(const object_overlay_context_ptr &context, const object_overlay_params_t &params)
{
    std::vector<uint32_t> id_array = {params.object_ids.begin(), params.object_ids.end()};
    vierkant::staging_copy_info_t copy_ids = {};
    copy_ids.num_bytes = sizeof(uint32_t) * id_array.size();
    copy_ids.data = id_array.data();
    copy_ids.dst_buffer = context->id_buffer;
    copy_ids.dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    copy_ids.dst_access = VK_ACCESS_2_SHADER_READ_BIT;

    object_overlay_ubo_t object_overlay_ubo = {};
    object_overlay_ubo.id_buffer_address = context->id_buffer->device_address();
    object_overlay_ubo.num_object_ids = id_array.size();

    vierkant::staging_copy_info_t copy_ubo = {};
    copy_ubo.num_bytes = sizeof(object_overlay_ubo_t);
    copy_ubo.data = &object_overlay_ubo;
    copy_ids.dst_buffer = context->param_buffer;
    copy_ids.dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    copy_ids.dst_access = VK_ACCESS_2_SHADER_READ_BIT;

    // id-buffer/ubo upload
    vierkant::staging_copy_context_t staging_context = {};
    staging_context.command_buffer = params.commandbuffer;
    staging_context.staging_buffer = context->staging_buffer;
    vierkant::staging_copy(staging_context, {copy_ids, copy_ubo});

    // run compute over input-image dimension
    auto computable = context->mask_computable;
    computable.extent = {vierkant::group_count(params.object_id_img->width(), context->mask_compute_local_size.x),
                         vierkant::group_count(params.object_id_img->height(), context->mask_compute_local_size.y), 1};

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
    context->result->transition_layout(VK_IMAGE_LAYOUT_GENERAL, params.commandbuffer);
    context->mask_compute.dispatch({computable}, params.commandbuffer);
    context->result->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, params.commandbuffer);
    return context->result;
}

}// namespace vierkant