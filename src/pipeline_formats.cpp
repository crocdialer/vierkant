#include "vierkant/shaders.hpp"
#include "vierkant/shaders_slang.hpp"
#include <vierkant/hash.hpp>
#include <vierkant/pipeline_formats.hpp>

#include "spirv_reflect.h"

//! comparison operators for some vulkan-structs used by vierkant::Pipeline
static inline bool operator==(const VkVertexInputBindingDescription &lhs, const VkVertexInputBindingDescription &rhs)
{
    if(lhs.binding != rhs.binding) { return false; }
    if(lhs.inputRate != rhs.inputRate) { return false; }
    if(lhs.stride != rhs.stride) { return false; }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline bool operator==(const VkVertexInputAttributeDescription &lhs,
                              const VkVertexInputAttributeDescription &rhs)
{
    if(lhs.binding != rhs.binding) { return false; }
    if(lhs.format != rhs.format) { return false; }
    if(lhs.location != rhs.location) { return false; }
    if(lhs.offset != rhs.offset) { return false; }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline bool operator==(const VkPipelineColorBlendAttachmentState &lhs,
                              const VkPipelineColorBlendAttachmentState &rhs)
{
    if(lhs.blendEnable != rhs.blendEnable) { return false; }
    if(lhs.srcColorBlendFactor != rhs.srcColorBlendFactor) { return false; }
    if(lhs.dstColorBlendFactor != rhs.dstColorBlendFactor) { return false; }
    if(lhs.colorBlendOp != rhs.colorBlendOp) { return false; }
    if(lhs.srcAlphaBlendFactor != rhs.srcAlphaBlendFactor) { return false; }
    if(lhs.dstAlphaBlendFactor != rhs.dstAlphaBlendFactor) { return false; }
    if(lhs.alphaBlendOp != rhs.alphaBlendOp) { return false; }
    if(lhs.colorWriteMask != rhs.colorWriteMask) { return false; }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static inline bool operator==(const VkPushConstantRange &lhs, const VkPushConstantRange &rhs)
{
    if(lhs.size != rhs.size) { return false; }
    if(lhs.offset != rhs.offset) { return false; }
    if(lhs.stageFlags != rhs.stageFlags) { return false; }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

namespace vierkant
{

shader_module_t create_shader_module(const void *spirv_code, size_t num_bytes)
{
    shader_module_t ret{};
    ret.create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ret.create_info.codeSize = num_bytes;
    ret.create_info.pCode = static_cast<const uint32_t *>(spirv_code);

    SpvReflectShaderModule spv_shader_module;
    spvReflectCreateShaderModule(num_bytes, spirv_code, &spv_shader_module);

    const std::unordered_map<SpvExecutionModel, VkShaderStageFlags> stage_lut = {
            {SpvExecutionModelVertex, VK_SHADER_STAGE_VERTEX_BIT},
            {SpvExecutionModelTessellationControl, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT},
            {SpvExecutionModelTessellationEvaluation, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT},
            {SpvExecutionModelGeometry, VK_SHADER_STAGE_GEOMETRY_BIT},
            {SpvExecutionModelFragment, VK_SHADER_STAGE_FRAGMENT_BIT},
            {SpvExecutionModelGLCompute, VK_SHADER_STAGE_COMPUTE_BIT},
            {SpvExecutionModelRayGenerationKHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
            {SpvExecutionModelClosestHitKHR, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
            {SpvExecutionModelAnyHitKHR, VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
            {SpvExecutionModelIntersectionKHR, VK_SHADER_STAGE_INTERSECTION_BIT_KHR},
            {SpvExecutionModelMissKHR, VK_SHADER_STAGE_MISS_BIT_KHR},
            {SpvExecutionModelCallableKHR, VK_SHADER_STAGE_CALLABLE_BIT_KHR},
            {SpvExecutionModelMeshEXT, VK_SHADER_STAGE_MESH_BIT_EXT},
            {SpvExecutionModelTaskEXT, VK_SHADER_STAGE_TASK_BIT_EXT}};

    for(uint32_t i = 0; i < spv_shader_module.entry_point_count; ++i)
    {
        const auto &spv_entry_point = spv_shader_module.entry_points[i];
        assert(stage_lut.contains(spv_entry_point.spirv_execution_model));

        // insert entry-point
        auto &entry_point = ret.entry_points[stage_lut.at(spv_entry_point.spirv_execution_model)].emplace_back();
        entry_point.name = spv_entry_point.name;
        entry_point.group_count = {spv_entry_point.local_size.x, spv_entry_point.local_size.y,
                                   spv_entry_point.local_size.z};

        for(uint32_t j = 0; j < spv_entry_point.descriptor_set_count; ++j)
        {
            const auto &spv_descriptor_set = spv_entry_point.descriptor_sets[j];
            for(uint32_t k = 0; k < spv_descriptor_set.binding_count; ++k)
            {
                const auto &spv_descriptor_binding = spv_descriptor_set.bindings[k];
                entry_point.bindings.insert(spv_descriptor_binding->binding);
            }
        }
    }
    spvReflectDestroyShaderModule(&spv_shader_module);
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

VkPipelineShaderStageCreateInfo shader_stage_create_info(VkShaderStageFlagBits stage,
                                                         const shader_module_t &shader_module,
                                                         const VkSpecializationInfo *specialization_info)
{
    VkPipelineShaderStageCreateInfo stage_info = {};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = stage;

    // no module, pNext contains ShaderModuleCreateinfo
    stage_info.pNext = &shader_module.create_info;

    if(!shader_module.entry_point_name.empty())
    {
        if(auto entry_it = shader_module.entry_points.find(stage); entry_it != shader_module.entry_points.end())
        {
            // iterate over entry-points for the current stage
            assert(!entry_it->second.empty());
            for(const auto &entry_point: entry_it->second)
            {
                if(entry_point.name.find(shader_module.entry_point_name) != std::string::npos)
                {
                    stage_info.pName = entry_point.name.c_str();
                }
            }
        }
    }
    else { stage_info.pName = shader_module.entry_points.at(stage).front().name.c_str(); }

    stage_info.pSpecializationInfo = specialization_info;
    return stage_info;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

raytracing_shader_layout_t raytracing_shader_layout(const raytracing_pipeline_info_t &pipeline_info,
                                                    const VkSpecializationInfo *specialization_info)
{
    raytracing_shader_layout_t ret;

    VkRayTracingShaderGroupCreateInfoKHR group_create_info = {};
    group_create_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    group_create_info.generalShader = VK_SHADER_UNUSED_KHR;
    group_create_info.closestHitShader = VK_SHADER_UNUSED_KHR;
    group_create_info.anyHitShader = VK_SHADER_UNUSED_KHR;
    group_create_info.intersectionShader = VK_SHADER_UNUSED_KHR;

    // append a stage, return its index
    auto add_stage = [&ret, specialization_info](VkShaderStageFlagBits stage, const shader_module_t &shader_module) {
        ret.stages.push_back(shader_stage_create_info(stage, shader_module, specialization_info));
        return static_cast<uint32_t>(ret.stages.size() - 1);
    };

    // one general group per shader of that stage, in the order the shader-map provides them
    auto add_general_groups = [&](VkShaderStageFlagBits stage) {
        uint32_t num_groups = 0;
        auto [begin, end] = pipeline_info.shader_stages.equal_range(stage);

        for(auto it = begin; it != end; ++it)
        {
            auto general_group = group_create_info;
            general_group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            general_group.generalShader = add_stage(stage, it->second);
            ret.groups.push_back(general_group);
            num_groups++;
        }
        return num_groups;
    };

    for(const auto &[stage, shader_module]: pipeline_info.shader_stages)
    {
        if(stage != VK_SHADER_STAGE_RAYGEN_BIT_KHR && stage != VK_SHADER_STAGE_MISS_BIT_KHR &&
           stage != VK_SHADER_STAGE_CALLABLE_BIT_KHR)
        {
            throw std::runtime_error("raytracing_shader_layout: shader_stages holds a hit-stage, "
                                     "those belong into raytracing_pipeline_info_t::hit_groups");
        }
    }

    // emission-order is raygen, hit, miss, callable - the shader-binding-table's region-order
    ret.num_raygen_groups = add_general_groups(VK_SHADER_STAGE_RAYGEN_BIT_KHR);

    for(const auto &hit_group: pipeline_info.hit_groups)
    {
        auto group = group_create_info;
        group.type = hit_group.intersection ? VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR
                                            : VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        group.closestHitShader = add_stage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, hit_group.closest_hit);

        if(hit_group.any_hit) { group.anyHitShader = add_stage(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, *hit_group.any_hit); }
        if(hit_group.intersection)
        {
            group.intersectionShader = add_stage(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, *hit_group.intersection);
        }
        ret.groups.push_back(group);
    }
    ret.num_hit_groups = static_cast<uint32_t>(pipeline_info.hit_groups.size());

    ret.num_miss_groups = add_general_groups(VK_SHADER_STAGE_MISS_BIT_KHR);
    ret.num_callable_groups = add_general_groups(VK_SHADER_STAGE_CALLABLE_BIT_KHR);
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

std::map<VkShaderStageFlagBits, shader_module_t> create_shader_stages(ShaderType t)
{
    std::map<VkShaderStageFlagBits, shader_module_t> ret;

    switch(t)
    {
        case ShaderType::UNLIT:
        {
            auto unlit_stage = create_shader_module(slang_shaders::unlit::unlit_slang);
            ret[VK_SHADER_STAGE_VERTEX_BIT] = unlit_stage;
            ret[VK_SHADER_STAGE_FRAGMENT_BIT] = unlit_stage;
        }
        break;

        case ShaderType::UNLIT_COLOR:
        {
            auto unlit_color_stage = create_shader_module(slang_shaders::unlit::color_slang);
            ret[VK_SHADER_STAGE_VERTEX_BIT] = unlit_color_stage;
            ret[VK_SHADER_STAGE_FRAGMENT_BIT] = unlit_color_stage;
        }
        break;

        case ShaderType::UNLIT_TEXTURE:
        {
            const auto shader_module = create_shader_module(slang_shaders::unlit::texture_slang);
            ret[VK_SHADER_STAGE_VERTEX_BIT] = shader_module;
            ret[VK_SHADER_STAGE_FRAGMENT_BIT] = shader_module;
        }
        break;

        case ShaderType::FULLSCREEN_GRID:
        {
            auto fs_texture_module = create_shader_module(slang_shaders::fullscreen::texture_slang);
            ret[VK_SHADER_STAGE_VERTEX_BIT] = fs_texture_module;
        }
            ret[VK_SHADER_STAGE_FRAGMENT_BIT] = create_shader_module(slang_shaders::fullscreen::grid_slang);
            break;

        case ShaderType::FULLSCREEN_TEXTURE:
        {
            auto fs_texture_module = create_shader_module(slang_shaders::fullscreen::texture_slang);
            ret[VK_SHADER_STAGE_VERTEX_BIT] = fs_texture_module;

            fs_texture_module.entry_point_name = "fragment_main";
            ret[VK_SHADER_STAGE_FRAGMENT_BIT] = fs_texture_module;
        }

        break;

        case ShaderType::FULLSCREEN_TEXTURE_DEPTH:
        {
            auto fs_texture_module = create_shader_module(slang_shaders::fullscreen::texture_slang);
            ret[VK_SHADER_STAGE_VERTEX_BIT] = fs_texture_module;

            fs_texture_module.entry_point_name = "fragment_depth_main";
            ret[VK_SHADER_STAGE_FRAGMENT_BIT] = fs_texture_module;
        }
        break;

        case ShaderType::UNLIT_CUBE:
        {
            auto cube_module = create_shader_module(slang_shaders::unlit::cube_slang);
            ret[VK_SHADER_STAGE_VERTEX_BIT] = cube_module;
            ret[VK_SHADER_STAGE_FRAGMENT_BIT] = cube_module;
        }
        break;

        default: break;
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////

bool graphics_pipeline_info_t::operator==(const graphics_pipeline_info_t &other) const
{
    if(attachment_count != other.attachment_count) { return false; }

    for(const auto &pair: shader_stages)
    {
        try
        {
            if(other.shader_stages.at(pair.first) != pair.second) { return false; }
        } catch(std::out_of_range &e) { return false; }
    }

    if(binding_descriptions != other.binding_descriptions) { return false; }
    if(attribute_descriptions != other.attribute_descriptions) { return false; }

    if(primitive_topology != other.primitive_topology) { return false; }
    if(primitive_restart != other.primitive_restart) { return false; }
    if(num_patch_control_points != other.num_patch_control_points) { return false; }
    if(front_face != other.front_face) { return false; }
    if(polygon_mode != other.polygon_mode) { return false; }
    if(cull_mode != other.cull_mode) { return false; }

    bool dynamic_scissor = crocore::contains(dynamic_states, VK_DYNAMIC_STATE_SCISSOR);
    bool dynamic_viewport = crocore::contains(dynamic_states, VK_DYNAMIC_STATE_VIEWPORT);
    if(!dynamic_viewport && memcmp(&viewport, &other.viewport, sizeof(VkViewport)) != 0) { return false; }
    if(!dynamic_scissor && memcmp(&scissor, &other.scissor, sizeof(VkRect2D)) != 0) { return false; }

    if(rasterizer_discard != other.rasterizer_discard) { return false; }
    if(depth_test != other.depth_test) { return false; }
    if(depth_write != other.depth_write) { return false; }
    if(depth_clamp != other.depth_clamp) { return false; }
    if(depth_compare_op != other.depth_compare_op) { return false; }
    if(stencil_test != other.stencil_test) { return false; }
    if(memcmp(&stencil_state_front, &other.stencil_state_front, sizeof(VkStencilOpState)) != 0) { return false; }
    if(memcmp(&stencil_state_back, &other.stencil_state_back, sizeof(VkStencilOpState)) != 0) { return false; }
    if(line_width != other.line_width) { return false; }
    if(sample_count != other.sample_count) { return false; }
    if(sample_shading != other.sample_shading) { return false; }
    if(min_sample_shading != other.min_sample_shading) { return false; }
    if(memcmp(&blend_state, &other.blend_state, sizeof(VkPipelineColorBlendAttachmentState)) != 0) { return false; }
    if(attachment_blend_states != other.attachment_blend_states) { return false; }
    if(view_mask != other.view_mask) { return false; }

    if(color_attachment_formats.size() != other.color_attachment_formats.size()) { return false; }
    for(uint32_t i = 0; i < color_attachment_formats.size(); ++i)
    {
        if(color_attachment_formats[i] != other.color_attachment_formats[i]) { return false; }
    }

    if(depth_attachment_format != other.depth_attachment_format) { return false; }
    if(stencil_attachment_format != other.stencil_attachment_format) { return false; }
    if(subpass != other.subpass) { return false; }
    if(base_pipeline != other.base_pipeline) { return false; }
    if(base_pipeline_index != other.base_pipeline_index) { return false; }
    if(specialization != other.specialization) { return false; }
    if(pipeline_cache != other.pipeline_cache) { return false; }
    if(dynamic_states != other.dynamic_states) { return false; }
    if(descriptor_set_layouts != other.descriptor_set_layouts) { return false; }
    if(push_constant_ranges != other.push_constant_ranges) { return false; }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool raytracing_pipeline_info_t::operator==(const raytracing_pipeline_info_t &other) const
{
    if(shader_stages != other.shader_stages) { return false; }
    if(hit_groups != other.hit_groups) { return false; }
    if(max_recursion != other.max_recursion) { return false; }
    if(descriptor_set_layouts != other.descriptor_set_layouts) { return false; }
    if(push_constant_ranges != other.push_constant_ranges) { return false; }
    if(pipeline_cache != other.pipeline_cache) { return false; }
    if(specialization != other.specialization) { return false; }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool compute_pipeline_info_t::operator==(const compute_pipeline_info_t &other) const
{
    if(shader_stage != other.shader_stage) { return false; }
    if(descriptor_set_layouts != other.descriptor_set_layouts) { return false; }
    if(push_constant_ranges != other.push_constant_ranges) { return false; }
    if(pipeline_cache != other.pipeline_cache) { return false; }
    if(specialization != other.specialization) { return false; }
    return true;
}

}// namespace vierkant

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using vierkant::hash_combine;

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

}// namespace std

size_t std::hash<vierkant::shader_module_t>::operator()(vierkant::shader_module_t const &sm) const noexcept
{
    size_t h = 0;
    hash_combine(h, sm.create_info.pCode);
    hash_combine(h, sm.create_info.codeSize);
    hash_combine(h, sm.entry_point_name);
    return h;
}

size_t
std::hash<vierkant::pipeline_specialization>::operator()(vierkant::pipeline_specialization const &ps) const noexcept
{
    size_t h = 0;
    for(const auto &[constant_id, blob]: ps.constant_blobs)
    {
        hash_combine(h, constant_id);
        for(const auto &byte: blob) { hash_combine(h, byte); }
    }
    return h;
}

size_t
std::hash<vierkant::graphics_pipeline_info_t>::operator()(vierkant::graphics_pipeline_info_t const &fmt) const noexcept
{
    size_t h = 0;

    bool dynamic_scissor = crocore::contains(fmt.dynamic_states, VK_DYNAMIC_STATE_SCISSOR);
    bool dynamic_viewport = crocore::contains(fmt.dynamic_states, VK_DYNAMIC_STATE_VIEWPORT);

    hash_combine(h, fmt.attachment_count);

    for(const auto &[stage, shader]: fmt.shader_stages)
    {
        hash_combine(h, stage);
        hash_combine(h, shader);
    }

    for(const auto &bd: fmt.binding_descriptions)
    {
        hash_combine(h, bd.binding);
        hash_combine(h, bd.inputRate);
        hash_combine(h, bd.stride);
    }

    for(const auto &ad: fmt.attribute_descriptions)
    {
        hash_combine(h, ad.binding);
        hash_combine(h, ad.format);
        hash_combine(h, ad.location);
        hash_combine(h, ad.offset);
    }
    hash_combine(h, fmt.primitive_topology);
    hash_combine(h, fmt.primitive_restart);
    hash_combine(h, fmt.num_patch_control_points);
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
    for(const auto &bs: fmt.attachment_blend_states) { hash_combine(h, bs); }
    hash_combine(h, fmt.view_mask);
    for(const auto &caf: fmt.color_attachment_formats) { hash_combine(h, caf); }
    hash_combine(h, fmt.depth_attachment_format);
    hash_combine(h, fmt.stencil_attachment_format);
    hash_combine(h, fmt.subpass);
    hash_combine(h, fmt.base_pipeline);
    hash_combine(h, fmt.base_pipeline_index);
    hash_combine(h, fmt.specialization);
    hash_combine(h, fmt.pipeline_cache);
    for(const auto &ds: fmt.dynamic_states) { hash_combine(h, ds); }
    for(const auto &dsl: fmt.descriptor_set_layouts) { hash_combine(h, dsl); }
    for(const auto &pcr: fmt.push_constant_ranges) { hash_combine(h, pcr); }
    return h;
}

size_t std::hash<vierkant::raytracing_pipeline_info_t>::operator()(
        vierkant::raytracing_pipeline_info_t const &fmt) const noexcept
{
    size_t h = 0;
    for(const auto &[stage, shader]: fmt.shader_stages)
    {
        hash_combine(h, stage);
        hash_combine(h, shader);
    }
    for(const auto &hit_group: fmt.hit_groups)
    {
        hash_combine(h, hit_group.closest_hit);
        if(hit_group.any_hit) { hash_combine(h, *hit_group.any_hit); }
        if(hit_group.intersection) { hash_combine(h, *hit_group.intersection); }
    }
    hash_combine(h, fmt.max_recursion);
    for(const auto &dsl: fmt.descriptor_set_layouts) { hash_combine(h, dsl); }
    for(const auto &pcr: fmt.push_constant_ranges) { hash_combine(h, pcr); }
    hash_combine(h, fmt.pipeline_cache);
    hash_combine(h, fmt.specialization);
    return h;
}

size_t
std::hash<vierkant::compute_pipeline_info_t>::operator()(vierkant::compute_pipeline_info_t const &fmt) const noexcept
{
    size_t h = 0;
    hash_combine(h, fmt.shader_stage);
    for(const auto &dsl: fmt.descriptor_set_layouts) { hash_combine(h, dsl); }
    for(const auto &pcr: fmt.push_constant_ranges) { hash_combine(h, pcr); }
    hash_combine(h, fmt.pipeline_cache);
    hash_combine(h, fmt.specialization);
    return h;
}
