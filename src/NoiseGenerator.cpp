#include "vierkant/NoiseGenerator.hpp"
#include "vierkant/shaders_slang.hpp"

namespace vierkant
{

std::unique_ptr<NoiseGenerator> NoiseGenerator::create(const DevicePtr &device, const create_info_t &create_info)
{ return std::unique_ptr<NoiseGenerator>(new NoiseGenerator(device, create_info)); }

NoiseGenerator::NoiseGenerator(const DevicePtr &device, const create_info_t &create_info)
{
    m_device = device;
    m_gradient = create_info.gradient;
    const bool is_3d = create_info.size.depth > 1;

    vierkant::Compute::create_info_t compute_info = {};
    compute_info.pipeline_cache = create_info.pipeline_cache;
    compute_info.descriptor_pool = create_info.descriptor_pool;
    m_compute = vierkant::Compute(device, compute_info);

    vierkant::Image::Format fmt = {};
    fmt.extent = create_info.size;
    fmt.format = create_info.color_format;
    fmt.image_type = is_3d ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    fmt.view_type = is_3d ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;
    fmt.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    fmt.use_mipmap = create_info.use_mipmap;

    // the noise tiles, so sample it that way
    fmt.sampler_state.address_mode_u = fmt.sampler_state.address_mode_v = fmt.sampler_state.address_mode_w =
            VK_SAMPLER_ADDRESS_MODE_REPEAT;
    m_image = vierkant::Image::create(device, fmt);

    // storage-views need identity-swizzle, only the returned clone swizzles to (r, r, r, 1)
    m_out_image = m_gradient ? m_image
                             : m_image->clone({VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R,
                                               VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE});

    auto shader_stage = vierkant::create_shader_module(vierkant::slang_shaders::slang::noise_slang);
    shader_stage.entry_point_name = is_3d ? "main_3d" : "main_2d";
    glm::uvec3 local_size = {};

    for(const auto &entry_point: shader_stage.entry_points.at(VK_SHADER_STAGE_COMPUTE_BIT))
    {
        if(entry_point.name == shader_stage.entry_point_name) { local_size = *entry_point.group_count; }
    }

    VkPushConstantRange push_constant_range = {};
    push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_constant_range.size = sizeof(params_t);

    m_computable.pipeline_info.shader_stage = shader_stage;
    m_computable.pipeline_info.push_constant_ranges = {push_constant_range};
    m_computable.extent = {vierkant::group_count(create_info.size.width, local_size.x),
                           vierkant::group_count(create_info.size.height, local_size.y),
                           vierkant::group_count(create_info.size.depth, local_size.z)};

    // bindings: 0 -> 2D image, 1 -> 3D image
    auto &desc_image = m_computable.descriptors[is_3d ? 1 : 0];
    desc_image.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    desc_image.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    desc_image.images = {m_image};
}

vierkant::ImagePtr NoiseGenerator::generate(VkCommandBuffer commandbuffer, glm::vec3 scale, glm::vec3 seed, float alpha)
{
    // no scale along a missing z-axis
    if(m_image->depth() == 1) { scale.z = 0.f; }

    params_t params = {};
    params.scale = scale;
    params.seed = seed;
    params.alpha = alpha;
    params.gradient = m_gradient;

    m_computable.push_constants.resize(sizeof(params_t));
    memcpy(m_computable.push_constants.data(), &params, sizeof(params_t));

    vierkant::begin_label(commandbuffer, {"NoiseGenerator::generate"});
    m_image->transition_layout(VK_IMAGE_LAYOUT_GENERAL, commandbuffer);
    m_compute.dispatch({m_computable}, commandbuffer);

    if(m_image->num_mip_levels() > 1)
    {
        // generate_mipmaps blits from TRANSFER_DST and leaves the image READ_ONLY_OPTIMAL
        m_image->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandbuffer);
        m_image->generate_mipmaps(commandbuffer);
    }
    else { m_image->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, commandbuffer); }
    vierkant::end_label(commandbuffer);
    return m_out_image;
}

}// namespace vierkant
