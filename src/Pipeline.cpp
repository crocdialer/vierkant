//
// Created by crocdialer on 11/14/18.
//

#include "vierkant/Pipeline.hpp"

namespace vierkant
{

///////////////////////////////////////////////////////////////////////////////////////////////////

ShaderModulePtr create_shader_module(const DevicePtr &device,
                                     const void* spirv_code,
                                     size_t num_bytes)
{
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = num_bytes;
    create_info.pCode = reinterpret_cast<const uint32_t*>(spirv_code);

    VkShaderModule shader_module;
    vkCheck(vkCreateShaderModule(device->handle(), &create_info, nullptr, &shader_module),
            "failed to create shader module!");
    return ShaderModulePtr(shader_module,
                           [device](VkShaderModule s){ vkDestroyShaderModule(device->handle(), s, nullptr); });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Pipeline::Pipeline(DevicePtr device, Format format) :
        m_device(std::move(device)),
        m_pipeline(VK_NULL_HANDLE),
        m_format(format)
{
    // no vertex shader -> fail
    if(!format.shader_stages.count(VK_SHADER_STAGE_VERTEX_BIT))
    {
        throw std::runtime_error("pipeline creation failed: no vertex shader stage provided");
    }

    // our shader stages
    std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_infos;

    for(const auto &pair : format.shader_stages)
    {
        VkPipelineShaderStageCreateInfo stage_info = {};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = pair.first;
        stage_info.module = pair.second.get();
        stage_info.pName = "main";
        shader_stage_create_infos.push_back(stage_info);
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(format.binding_descriptions.size());
    vertex_input_info.pVertexBindingDescriptions = format.binding_descriptions.data();
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(format.attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions = format.attribute_descriptions.data();

    // primitive topology
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = format.primitive_topology;
    inputAssembly.primitiveRestartEnable = static_cast<VkBool32>(format.primitive_restart);

    if(!format.scissor.extent.width || !format.scissor.extent.height)
    {
        format.scissor.extent = {static_cast<uint32_t>(format.viewport.width),
                                 static_cast<uint32_t>(format.viewport.height)};
    }

    // viewport + scissor create info
    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &format.viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &format.scissor;

    // rasterizer settings
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = static_cast<VkBool32>(format.depth_clamp);
    rasterizer.rasterizerDiscardEnable = static_cast<VkBool32>(format.rasterizer_discard);
    rasterizer.polygonMode = format.polygon_mode;
    rasterizer.lineWidth = format.line_width;
    rasterizer.cullMode = format.cull_mode;
    rasterizer.frontFace = format.front_face;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    // multisampling settings
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = static_cast<VkBool32>(format.sample_shading);
    multisampling.rasterizationSamples = format.sample_count;
    multisampling.minSampleShading = format.min_sample_shading;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    // depth / stencil settings
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = static_cast<VkBool32>(format.depth_test);
    depth_stencil.depthWriteEnable = static_cast<VkBool32>(format.depth_write);
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.minDepthBounds = 0.0f;
    depth_stencil.maxDepthBounds = 1.0f;
    depth_stencil.stencilTestEnable = static_cast<VkBool32>(format.stencil_test);
    depth_stencil.front = format.stencil_state_front;
    depth_stencil.back = format.stencil_state_back;


    // blend settings (per framebuffer)
    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = static_cast<VkBool32>(format.blending);
    color_blend_attachment.srcColorBlendFactor = format.src_color_blend_factor;
    color_blend_attachment.dstColorBlendFactor = format.dst_color_blend_factor;
    color_blend_attachment.colorBlendOp = format.color_blend_op;
    color_blend_attachment.srcAlphaBlendFactor = format.src_alpha_blend_factor;
    color_blend_attachment.dstAlphaBlendFactor = format.dst_alpha_blend_factor;
    color_blend_attachment.alphaBlendOp = format.alpha_blend_op;

    // blend settings (global)
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &color_blend_attachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // configure dynamic states
    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {};
    dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_create_info.dynamicStateCount = static_cast<uint32_t>(format.dynamic_states.size());
    dynamic_state_create_info.pDynamicStates = format.dynamic_states.data();

    // define pipeline layout (uniforms, push-constants, ...)
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(format.descriptor_set_layouts.size());
    pipeline_layout_info.pSetLayouts = format.descriptor_set_layouts.data();
    pipeline_layout_info.pushConstantRangeCount = static_cast<uint32_t>(format.push_constant_ranges.size());
    pipeline_layout_info.pPushConstantRanges = format.push_constant_ranges.data();

    vkCheck(vkCreatePipelineLayout(m_device->handle(), &pipeline_layout_info, nullptr, &m_pipeline_layout),
            "failed to create pipeline layout!");

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = static_cast<uint32_t>(shader_stage_create_infos.size());
    pipeline_info.pStages = shader_stage_create_infos.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &inputAssembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &colorBlending;
    pipeline_info.pDynamicState = &dynamic_state_create_info;

    pipeline_info.layout = m_pipeline_layout;
    pipeline_info.renderPass = format.renderpass;
    pipeline_info.subpass = format.subpass;
    pipeline_info.basePipelineHandle = format.base_pipeline;
    pipeline_info.basePipelineIndex = format.base_pipeline_index;

    vkCheck(vkCreateGraphicsPipelines(m_device->handle(), format.pipeline_cache, 1, &pipeline_info, nullptr,
                                      &m_pipeline),
            "failed to create graphics pipeline!");
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Pipeline::Pipeline(Pipeline &&other) noexcept:
        Pipeline()
{
    swap(*this, other);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Pipeline::~Pipeline()
{
    if(m_device)
    {
        if(m_pipeline_layout){ vkDestroyPipelineLayout(m_device->handle(), m_pipeline_layout, nullptr); }
        if(m_pipeline){ vkDestroyPipeline(m_device->handle(), m_pipeline, nullptr); }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Pipeline &Pipeline::operator=(Pipeline other)
{
    swap(*this, other);
    return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Pipeline::bind(VkCommandBuffer command_buffer)
{
    // bind pipeline
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void swap(Pipeline &lhs, Pipeline &rhs)
{
    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_pipeline_layout, rhs.m_pipeline_layout);
    std::swap(lhs.m_pipeline, rhs.m_pipeline);
    std::swap(lhs.m_format, rhs.m_format);
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

///////////////////////////////////////////////////////////////////////////////////////////////////

bool operator!=(const VkStencilOpState &lhs, const VkStencilOpState &rhs){ return !(lhs == rhs); }

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Pipeline::Format::operator==(const Pipeline::Format &other) const
{
    for(const auto &pair : shader_stages)
    {
        try{ if(other.shader_stages.at(pair.first) != pair.second){ return false; }}
        catch(std::out_of_range &e){ return false; }
    }

    if(binding_descriptions.size() != other.binding_descriptions.size()){ return false; }

    for(uint32_t i = 0; i < binding_descriptions.size(); ++i)
    {
        const auto &lhs = binding_descriptions[i];
        const auto &rhs = other.binding_descriptions[i];
        if(lhs.binding != rhs.binding){ return false; }
        if(lhs.inputRate != rhs.inputRate){ return false; }
        if(lhs.stride != rhs.stride){ return false; }
    }

    if(attribute_descriptions.size() != other.attribute_descriptions.size()){ return false; }

    for(uint32_t i = 0; i < attribute_descriptions.size(); ++i)
    {
        const auto &lhs = attribute_descriptions[i];
        const auto &rhs = other.attribute_descriptions[i];
        if(lhs.binding != rhs.binding){ return false; }
        if(lhs.format != rhs.format){ return false; }
        if(lhs.location != rhs.location){ return false; }
        if(lhs.offset != rhs.offset){ return false; }
    }

    if(primitive_topology != other.primitive_topology){ return false; }
    if(primitive_restart != other.primitive_restart){ return false; }
    if(front_face != other.front_face){ return false; }
    if(polygon_mode != other.polygon_mode){ return false; }
    if(cull_mode != other.cull_mode){ return false; }
    if(viewport.x != other.viewport.x){ return false; }
    if(viewport.y != other.viewport.y){ return false; }
    if(viewport.width != other.viewport.width){ return false; }
    if(viewport.height != other.viewport.height){ return false; }
    if(viewport.minDepth != other.viewport.minDepth){ return false; }
    if(viewport.maxDepth != other.viewport.maxDepth){ return false; }
    if(scissor.offset.x != other.scissor.offset.x){ return false; }
    if(scissor.offset.y != other.scissor.offset.y){ return false; }
    if(scissor.extent.width != other.scissor.extent.width){ return false; }
    if(scissor.extent.height != other.scissor.extent.height){ return false; }
    if(rasterizer_discard != other.rasterizer_discard){ return false; }
    if(depth_test != other.depth_test){ return false; }
    if(depth_write != other.depth_write){ return false; }
    if(depth_clamp != other.depth_clamp){ return false; }
    if(stencil_test != other.stencil_test){ return false; }
    if(stencil_state_front != other.stencil_state_front){ return false; }
    if(stencil_state_back != other.stencil_state_back){ return false; }
    if(line_width != other.line_width){ return false; }
    if(sample_count != other.sample_count){ return false; }
    if(sample_shading != other.sample_shading){ return false; }
    if(min_sample_shading != other.min_sample_shading){ return false; }
    if(blending != other.blending){ return false; }
    if(src_color_blend_factor != other.src_color_blend_factor){ return false; }
    if(dst_color_blend_factor != other.dst_color_blend_factor){ return false; }
    if(color_blend_op != other.color_blend_op){ return false; }
    if(src_alpha_blend_factor != other.src_alpha_blend_factor){ return false; }
    if(dst_alpha_blend_factor != other.dst_alpha_blend_factor){ return false; }
    if(alpha_blend_op != other.alpha_blend_op){ return false; }
    if(renderpass != other.renderpass){ return false; }
    if(subpass != other.subpass){ return false; }
    if(base_pipeline != other.base_pipeline){ return false; }
    if(base_pipeline_index != other.base_pipeline_index){ return false; }
    if(pipeline_cache != other.pipeline_cache){ return false; }

    if(src_alpha_blend_factor != other.src_alpha_blend_factor){ return false; }

    if(dynamic_states.size() != other.dynamic_states.size()){ return false; }
    for(uint32_t i = 0; i < dynamic_states.size(); ++i)
    {
        const auto &lhs = dynamic_states[i];
        const auto &rhs = other.dynamic_states[i];
        if(lhs != rhs){ return false; }
    }
    if(descriptor_set_layouts.size() != other.descriptor_set_layouts.size()){ return false; }
    for(uint32_t i = 0; i < descriptor_set_layouts.size(); ++i)
    {
        const auto &lhs = descriptor_set_layouts[i];
        const auto &rhs = other.descriptor_set_layouts[i];
        if(lhs != rhs){ return false; }
    }
    if(push_constant_ranges.size() != other.push_constant_ranges.size()){ return false; }
    for(uint32_t i = 0; i < push_constant_ranges.size(); ++i)
    {
        const auto &lhs = push_constant_ranges[i];
        const auto &rhs = other.push_constant_ranges[i];
        if(lhs.size != rhs.size){ return false; }
        if(lhs.offset != rhs.offset){ return false; }
        if(lhs.stageFlags != rhs.stageFlags){ return false; }
    }
    return true;
}

}// namespace vierkant

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Create a hash for a value and combine with existing hash
 * @see https://www.boost.org/doc/libs/1_55_0/doc/html/hash/reference.html#boost.hash_combine
 */
template<class T>
inline void hash_combine(std::size_t &seed, const T &v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

namespace std
{
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

size_t std::hash<vierkant::Pipeline::Format>::operator()(vierkant::Pipeline::Format const &fmt) const
{
    size_t h = 0;

    for(const auto &pair : fmt.shader_stages)
    {
        hash_combine(h, pair.first);
        hash_combine(h, pair.second);
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
    hash_combine(h, fmt.viewport.x);
    hash_combine(h, fmt.viewport.y);
    hash_combine(h, fmt.viewport.width);
    hash_combine(h, fmt.viewport.height);
    hash_combine(h, fmt.viewport.minDepth);
    hash_combine(h, fmt.viewport.maxDepth);
    hash_combine(h, fmt.scissor.offset.x);
    hash_combine(h, fmt.scissor.offset.y);
    hash_combine(h, fmt.scissor.extent.width);
    hash_combine(h, fmt.scissor.extent.height);
    hash_combine(h, fmt.rasterizer_discard);
    hash_combine(h, fmt.depth_test);
    hash_combine(h, fmt.depth_write);
    hash_combine(h, fmt.depth_clamp);
    hash_combine(h, fmt.stencil_test);
    hash_combine(h, fmt.stencil_state_front);
    hash_combine(h, fmt.stencil_state_back);
    hash_combine(h, fmt.line_width);
    hash_combine(h, fmt.sample_count);
    hash_combine(h, fmt.sample_shading);
    hash_combine(h, fmt.min_sample_shading);
    hash_combine(h, fmt.blending);
    hash_combine(h, fmt.src_color_blend_factor);
    hash_combine(h, fmt.dst_color_blend_factor);
    hash_combine(h, fmt.color_blend_op);
    hash_combine(h, fmt.src_alpha_blend_factor);
    hash_combine(h, fmt.dst_alpha_blend_factor);
    hash_combine(h, fmt.alpha_blend_op);
    hash_combine(h, fmt.renderpass);
    hash_combine(h, fmt.subpass);
    hash_combine(h, fmt.base_pipeline);
    hash_combine(h, fmt.base_pipeline_index);
    hash_combine(h, fmt.pipeline_cache);
    for(const auto &ds : fmt.dynamic_states){ hash_combine(h, ds); }
    for(const auto &dsl : fmt.descriptor_set_layouts){ hash_combine(h, dsl); }
    for(const auto &pcr : fmt.push_constant_ranges){ hash_combine(h, pcr); }
    return h;
}