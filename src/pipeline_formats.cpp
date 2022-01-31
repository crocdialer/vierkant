//
// Created by crocdialer on 1/20/21.
//

#include <crocore/utils.hpp>
#include <vierkant/pipeline_formats.hpp>
#include "vierkant/shaders.hpp"

namespace vierkant
{

///////////////////////////////////////////////////////////////////////////////////////////////////

ShaderModulePtr create_shader_module(const DevicePtr &device,
                                     const void *spirv_code,
                                     size_t num_bytes)
{
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = num_bytes;
    create_info.pCode = reinterpret_cast<const uint32_t *>(spirv_code);

    VkShaderModule shader_module;
    vkCheck(vkCreateShaderModule(device->handle(), &create_info, nullptr, &shader_module),
            "failed to create shader module!");
    return ShaderModulePtr(shader_module,
                           [device](VkShaderModule s){ vkDestroyShaderModule(device->handle(), s, nullptr); });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<VkRayTracingShaderGroupCreateInfoKHR>
raytracing_shader_groups(const raytracing_shader_map_t &shader_stages)
{
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> ret;

    for(const auto &[stage, shader_module] : shader_stages)
    {
        uint32_t next_index = ret.size();

        VkRayTracingShaderGroupCreateInfoKHR group_create_info = {};
        group_create_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        group_create_info.generalShader = VK_SHADER_UNUSED_KHR;
        group_create_info.closestHitShader = VK_SHADER_UNUSED_KHR;
        group_create_info.anyHitShader = VK_SHADER_UNUSED_KHR;
        group_create_info.intersectionShader = VK_SHADER_UNUSED_KHR;

        switch(stage)
        {
            case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
            case VK_SHADER_STAGE_MISS_BIT_KHR:
                group_create_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
                group_create_info.generalShader = next_index;
                break;

            case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
                group_create_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
                group_create_info.closestHitShader = next_index;
                break;

            case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
                group_create_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
                group_create_info.anyHitShader = next_index;
                break;

            default:
                throw std::runtime_error("raytracing_shader_groups: provided a non-raytracing shader");
        }
        ret.push_back(group_create_info);
    }
    return ret;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

std::map<VkShaderStageFlagBits, ShaderModulePtr> create_shader_stages(const DevicePtr &device, ShaderType t)
{
    std::map<VkShaderStageFlagBits, ShaderModulePtr> ret;

    switch(t)
    {
        case ShaderType::UNLIT:
            ret[VK_SHADER_STAGE_VERTEX_BIT] = create_shader_module(device, shaders::unlit::unlit_vert);
            ret[VK_SHADER_STAGE_FRAGMENT_BIT] = create_shader_module(device, shaders::unlit::unlit_frag);
            break;

        case ShaderType::UNLIT_COLOR:
            ret[VK_SHADER_STAGE_VERTEX_BIT] = create_shader_module(device, shaders::unlit::color_vert);
            ret[VK_SHADER_STAGE_FRAGMENT_BIT] = create_shader_module(device, shaders::unlit::color_frag);
            break;

        case ShaderType::UNLIT_TEXTURE:
            ret[VK_SHADER_STAGE_VERTEX_BIT] = create_shader_module(device, shaders::unlit::texture_vert);
            ret[VK_SHADER_STAGE_FRAGMENT_BIT] = create_shader_module(device, shaders::unlit::texture_frag);
            break;

        case ShaderType::FULLSCREEN_TEXTURE:
            ret[VK_SHADER_STAGE_VERTEX_BIT] = create_shader_module(device, shaders::fullscreen::texture_vert);
            ret[VK_SHADER_STAGE_FRAGMENT_BIT] = create_shader_module(device, shaders::fullscreen::texture_frag);
            break;

        case ShaderType::FULLSCREEN_TEXTURE_DEPTH:
            ret[VK_SHADER_STAGE_VERTEX_BIT] = create_shader_module(device, shaders::fullscreen::texture_vert);
            ret[VK_SHADER_STAGE_FRAGMENT_BIT] = create_shader_module(device, shaders::fullscreen::texture_depth_frag);
            break;

        case ShaderType::UNLIT_COLOR_SKIN:
            ret[VK_SHADER_STAGE_VERTEX_BIT] = create_shader_module(device, shaders::unlit::skin_vert);
            ret[VK_SHADER_STAGE_FRAGMENT_BIT] = create_shader_module(device, shaders::unlit::color_frag);
            break;

        case ShaderType::UNLIT_TEXTURE_SKIN:
            ret[VK_SHADER_STAGE_VERTEX_BIT] = create_shader_module(device, shaders::unlit::skin_vert);
            ret[VK_SHADER_STAGE_FRAGMENT_BIT] = create_shader_module(device, shaders::unlit::texture_frag);
            break;

        case ShaderType::UNLIT_CUBE:
            ret[VK_SHADER_STAGE_VERTEX_BIT] = create_shader_module(device, shaders::unlit::cube_vert);
            ret[VK_SHADER_STAGE_FRAGMENT_BIT] = create_shader_module(device, shaders::unlit::cube_frag);
            break;

        default:
            break;
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////

bool graphics_pipeline_info_t::operator==(const graphics_pipeline_info_t &other) const
{
    if(attachment_count != other.attachment_count){ return false; }

    for(const auto &pair : shader_stages)
    {
        try{ if(other.shader_stages.at(pair.first) != pair.second){ return false; }}
        catch(std::out_of_range &e){ return false; }
    }

    if(binding_descriptions != other.binding_descriptions){ return false; }
    if(attribute_descriptions != other.attribute_descriptions){ return false; }

    if(primitive_topology != other.primitive_topology){ return false; }
    if(primitive_restart != other.primitive_restart){ return false; }
    if(front_face != other.front_face){ return false; }
    if(polygon_mode != other.polygon_mode){ return false; }
    if(cull_mode != other.cull_mode){ return false; }

    bool dynamic_scissor = crocore::contains(dynamic_states, VK_DYNAMIC_STATE_SCISSOR);
    bool dynamic_viewport = crocore::contains(dynamic_states, VK_DYNAMIC_STATE_VIEWPORT);
    if(!dynamic_viewport && memcmp(&viewport, &other.viewport, sizeof(VkViewport)) != 0){ return false; }
    if(!dynamic_scissor && memcmp(&scissor, &other.scissor, sizeof(VkRect2D)) != 0){ return false; }

    if(rasterizer_discard != other.rasterizer_discard){ return false; }
    if(depth_test != other.depth_test){ return false; }
    if(depth_write != other.depth_write){ return false; }
    if(depth_clamp != other.depth_clamp){ return false; }
    if(depth_compare_op != other.depth_compare_op){ return false; }
    if(stencil_test != other.stencil_test){ return false; }
    if(stencil_state_front != other.stencil_state_front){ return false; }
    if(stencil_state_back != other.stencil_state_back){ return false; }
    if(line_width != other.line_width){ return false; }
    if(sample_count != other.sample_count){ return false; }
    if(sample_shading != other.sample_shading){ return false; }
    if(min_sample_shading != other.min_sample_shading){ return false; }
    if(blend_state != other.blend_state){ return false; }
    if(attachment_blend_states != other.attachment_blend_states){ return false; }
    if(renderpass != other.renderpass){ return false; }
    if(subpass != other.subpass){ return false; }
    if(base_pipeline != other.base_pipeline){ return false; }
    if(base_pipeline_index != other.base_pipeline_index){ return false; }
    if(specialization_info != other.specialization_info){ return false; }
    if(pipeline_cache != other.pipeline_cache){ return false; }

    if(dynamic_states != other.dynamic_states){ return false; }

    if(descriptor_set_layouts != other.descriptor_set_layouts){ return false; }
    if(push_constant_ranges != other.push_constant_ranges){ return false; }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool raytracing_pipeline_info_t::operator==(const raytracing_pipeline_info_t &other) const
{
    if(shader_stages != other.shader_stages){ return false; }
    if(max_recursion != other.max_recursion){ return false; }
    if(descriptor_set_layouts != other.descriptor_set_layouts){ return false; }
    if(push_constant_ranges != other.push_constant_ranges){ return false; }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool compute_pipeline_info_t::operator==(const compute_pipeline_info_t &other) const
{
    if(shader_stage != other.shader_stage){ return false; }
    if(descriptor_set_layouts != other.descriptor_set_layouts){ return false; }
    if(push_constant_ranges != other.push_constant_ranges){ return false; }
    return true;
}

}// namespace vierkant

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool operator==(const VkVertexInputBindingDescription &lhs, const VkVertexInputBindingDescription &rhs)
{
    if(lhs.binding != rhs.binding){ return false; }
    if(lhs.inputRate != rhs.inputRate){ return false; }
    if(lhs.stride != rhs.stride){ return false; }
    return true;
}

bool operator!=(const VkVertexInputBindingDescription &lhs, const VkVertexInputBindingDescription &rhs)
{
    return !(lhs == rhs);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool operator==(const VkVertexInputAttributeDescription &lhs, const VkVertexInputAttributeDescription &rhs)
{
    if(lhs.binding != rhs.binding){ return false; }
    if(lhs.format != rhs.format){ return false; }
    if(lhs.location != rhs.location){ return false; }
    if(lhs.offset != rhs.offset){ return false; }
    return true;
}

bool operator!=(const VkVertexInputAttributeDescription &lhs, const VkVertexInputAttributeDescription &rhs)
{
    return !(lhs == rhs);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool operator==(const VkPipelineColorBlendAttachmentState &lhs,
                const VkPipelineColorBlendAttachmentState &rhs)
{
    if(lhs.blendEnable != rhs.blendEnable){ return false; }
    if(lhs.srcColorBlendFactor != rhs.srcColorBlendFactor){ return false; }
    if(lhs.dstColorBlendFactor != rhs.dstColorBlendFactor){ return false; }
    if(lhs.colorBlendOp != rhs.colorBlendOp){ return false; }
    if(lhs.srcAlphaBlendFactor != rhs.srcAlphaBlendFactor){ return false; }
    if(lhs.dstAlphaBlendFactor != rhs.dstAlphaBlendFactor){ return false; }
    if(lhs.alphaBlendOp != rhs.alphaBlendOp){ return false; }
    if(lhs.colorWriteMask != rhs.colorWriteMask){ return false; }
    return true;
}

bool operator!=(const VkPipelineColorBlendAttachmentState &lhs,
                const VkPipelineColorBlendAttachmentState &rhs)
{
    return !(lhs == rhs);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool operator==(const VkStencilOpState &lhs, const VkStencilOpState &rhs)
{
    if(lhs.failOp != rhs.failOp){ return false; }
    if(lhs.passOp != rhs.passOp){ return false; }
    if(lhs.depthFailOp != rhs.depthFailOp){ return false; }
    if(lhs.compareOp != rhs.compareOp){ return false; }
    if(lhs.compareMask != rhs.compareMask){ return false; }
    if(lhs.writeMask != rhs.writeMask){ return false; }
    if(lhs.reference != rhs.reference){ return false; }
    return true;
}

bool operator!=(const VkStencilOpState &lhs, const VkStencilOpState &rhs)
{
    return !(lhs == rhs);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool operator==(const VkPushConstantRange &lhs, const VkPushConstantRange &rhs)
{
    if(lhs.size != rhs.size){ return false; }
    if(lhs.offset != rhs.offset){ return false; }
    if(lhs.stageFlags != rhs.stageFlags){ return false; }
    return true;
}

bool operator!=(const VkPushConstantRange &lhs, const VkPushConstantRange &rhs)
{
    return !(lhs == rhs);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

using crocore::hash_combine;

namespace std
{

template<>
struct hash<VkPipelineColorBlendAttachmentState>
{
    size_t operator()(VkPipelineColorBlendAttachmentState const &blendAttachmentState) const
    {
        size_t h = 0;
        hash_combine(h, blendAttachmentState.blendEnable);
        hash_combine(h, blendAttachmentState.srcColorBlendFactor);
        hash_combine(h, blendAttachmentState.dstColorBlendFactor);
        hash_combine(h, blendAttachmentState.colorBlendOp);
        hash_combine(h, blendAttachmentState.srcAlphaBlendFactor);
        hash_combine(h, blendAttachmentState.dstAlphaBlendFactor);
        hash_combine(h, blendAttachmentState.alphaBlendOp);
        hash_combine(h, blendAttachmentState.colorWriteMask);
        return h;
    }
};

template<>
struct hash<VkStencilOpState>
{
    size_t operator()(VkStencilOpState const &op) const
    {
        size_t h = 0;
        hash_combine(h, op.failOp);
        hash_combine(h, op.passOp);
        hash_combine(h, op.depthFailOp);
        hash_combine(h, op.compareOp);
        hash_combine(h, op.compareMask);
        hash_combine(h, op.writeMask);
        hash_combine(h, op.reference);
        return h;
    }
};

template<>
struct hash<VkPushConstantRange>
{
    size_t operator()(VkPushConstantRange const &pcr) const
    {
        size_t h = 0;
        hash_combine(h, pcr.stageFlags);
        hash_combine(h, pcr.offset);
        hash_combine(h, pcr.size);
        return h;
    }
};

}

size_t std::hash<vierkant::graphics_pipeline_info_t>::operator()(vierkant::graphics_pipeline_info_t const &fmt) const
{
    size_t h = 0;

    bool dynamic_scissor = crocore::contains(fmt.dynamic_states, VK_DYNAMIC_STATE_SCISSOR);
    bool dynamic_viewport = crocore::contains(fmt.dynamic_states, VK_DYNAMIC_STATE_VIEWPORT);

    hash_combine(h, fmt.attachment_count);

    for(const auto &[stage, shader] : fmt.shader_stages)
    {
        hash_combine(h, stage);
        hash_combine(h, shader);
    }

    for(const auto &bd : fmt.binding_descriptions)
    {
        hash_combine(h, bd.binding);
        hash_combine(h, bd.inputRate);
        hash_combine(h, bd.stride);
    }

    for(const auto &ad : fmt.attribute_descriptions)
    {
        hash_combine(h, ad.binding);
        hash_combine(h, ad.format);
        hash_combine(h, ad.location);
        hash_combine(h, ad.offset);
    }
    hash_combine(h, fmt.primitive_topology);
    hash_combine(h, fmt.primitive_restart);
    hash_combine(h, fmt.front_face);
    hash_combine(h, fmt.polygon_mode);
    hash_combine(h, fmt.cull_mode);

    if(!dynamic_viewport)
    {
        hash_combine(h, fmt.viewport.x);
        hash_combine(h, fmt.viewport.y);
        hash_combine(h, fmt.viewport.width);
        hash_combine(h, fmt.viewport.height);
        hash_combine(h, fmt.viewport.minDepth);
        hash_combine(h, fmt.viewport.maxDepth);
    }

    if(!dynamic_scissor)
    {
        hash_combine(h, fmt.scissor.offset.x);
        hash_combine(h, fmt.scissor.offset.y);
        hash_combine(h, fmt.scissor.extent.width);
        hash_combine(h, fmt.scissor.extent.height);
    }

    hash_combine(h, fmt.rasterizer_discard);
    hash_combine(h, fmt.depth_test);
    hash_combine(h, fmt.depth_write);
    hash_combine(h, fmt.depth_clamp);
    hash_combine(h, fmt.depth_compare_op);
    hash_combine(h, fmt.stencil_test);
    hash_combine(h, fmt.stencil_state_front);
    hash_combine(h, fmt.stencil_state_back);
    hash_combine(h, fmt.line_width);
    hash_combine(h, fmt.sample_count);
    hash_combine(h, fmt.sample_shading);
    hash_combine(h, fmt.min_sample_shading);
    hash_combine(h, fmt.blend_state);
    for(const auto &bs : fmt.attachment_blend_states){ hash_combine(h, bs); }
    hash_combine(h, fmt.renderpass);
    hash_combine(h, fmt.subpass);
    hash_combine(h, fmt.base_pipeline);
    hash_combine(h, fmt.base_pipeline_index);
    hash_combine(h, fmt.specialization_info);
    hash_combine(h, fmt.pipeline_cache);
    for(const auto &ds : fmt.dynamic_states){ hash_combine(h, ds); }
    for(const auto &dsl : fmt.descriptor_set_layouts){ hash_combine(h, dsl); }
    for(const auto &pcr : fmt.push_constant_ranges){ hash_combine(h, pcr); }
    return h;
}

size_t
std::hash<vierkant::raytracing_pipeline_info_t>::operator()(vierkant::raytracing_pipeline_info_t const &fmt) const
{
    size_t h = 0;
    for(const auto &[stage, shader] : fmt.shader_stages)
    {
        hash_combine(h, stage);
        hash_combine(h, shader);
    }
    hash_combine(h, fmt.max_recursion);
    for(const auto &dsl : fmt.descriptor_set_layouts){ hash_combine(h, dsl); }
    for(const auto &pcr : fmt.push_constant_ranges){ hash_combine(h, pcr); }
    return h;
}

size_t
std::hash<vierkant::compute_pipeline_info_t>::operator()(vierkant::compute_pipeline_info_t const &fmt) const
{
    size_t h = 0;
    hash_combine(h, fmt.shader_stage);
    for(const auto &dsl : fmt.descriptor_set_layouts){ hash_combine(h, dsl); }
    for(const auto &pcr : fmt.push_constant_ranges){ hash_combine(h, pcr); }
    return h;
}