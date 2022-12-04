//
// Created by crocdialer on 11/14/18.
//

#include "vierkant/Pipeline.hpp"

namespace vierkant
{

// shader stages from map/multimap
template<typename ShaderMap_T>
std::vector<VkPipelineShaderStageCreateInfo>
shader_stage_create_infos(const ShaderMap_T &shader_stages,
                          const VkSpecializationInfo *specialization_info = nullptr)
{
    std::vector<VkPipelineShaderStageCreateInfo> ret;

    for(const auto &[stage, shader_module]: shader_stages)
    {
        VkPipelineShaderStageCreateInfo stage_info = {};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = stage;
        stage_info.module = shader_module.get();
        stage_info.pName = "main";
        stage_info.pSpecializationInfo = specialization_info;
        ret.push_back(stage_info);
    }
    return ret;
};

PipelinePtr Pipeline::create(DevicePtr device, graphics_pipeline_info_t format)
{
    // no vertex shader -> fail
    if(!format.shader_stages.count(VK_SHADER_STAGE_VERTEX_BIT) &&
       !format.shader_stages.count(VK_SHADER_STAGE_MESH_BIT_EXT))
    {
        throw std::runtime_error("pipeline creation failed: no vertex/mesh shader stage provided");
    }

    // our shader stages
    auto stage_create_infos = shader_stage_create_infos(format.shader_stages, format.specialization_info);

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

    VkPipelineTessellationStateCreateInfo tessellation_state_create_info = {};
    tessellation_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellation_state_create_info.patchControlPoints = format.num_patch_control_points;
    bool use_tesselation = format.primitive_topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST &&
                           format.shader_stages.count(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);

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
    depth_stencil.depthCompareOp = format.depth_compare_op;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.minDepthBounds = 0.0f;
    depth_stencil.maxDepthBounds = 1.0f;
    depth_stencil.stencilTestEnable = static_cast<VkBool32>(format.stencil_test);
    depth_stencil.front = format.stencil_state_front;
    depth_stencil.back = format.stencil_state_back;

    // blend settings (per framebuffer)
    std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments = {};

    if(format.attachment_blend_states.size() == format.attachment_count)
    {
        // apply attachment-specific blend-configuration
        color_blend_attachments = format.attachment_blend_states;
    }
    else
    {
        // apply global blend-configuration for all attachments
        color_blend_attachments.resize(format.attachment_count, format.blend_state);
    }

    // blend settings (global)
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = color_blend_attachments.size();
    colorBlending.pAttachments = color_blend_attachments.data();
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

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    vkCheck(vkCreatePipelineLayout(device->handle(), &pipeline_layout_info, nullptr, &pipeline_layout),
            "failed to create pipeline layout!");

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = static_cast<uint32_t>(stage_create_infos.size());
    pipeline_info.pStages = stage_create_infos.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &inputAssembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &colorBlending;
    pipeline_info.pDynamicState = &dynamic_state_create_info;
    pipeline_info.pTessellationState = use_tesselation ? &tessellation_state_create_info : nullptr;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = format.renderpass;
    pipeline_info.subpass = format.subpass;
    pipeline_info.basePipelineHandle = format.base_pipeline;
    pipeline_info.basePipelineIndex = format.base_pipeline_index;

    VkPipeline pipeline = VK_NULL_HANDLE;
    vkCheck(vkCreateGraphicsPipelines(device->handle(), format.pipeline_cache, 1, &pipeline_info, nullptr,
                                      &pipeline),
            "failed to create graphics pipeline!");

    return PipelinePtr(new Pipeline(std::move(device), pipeline_layout, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PipelinePtr Pipeline::create(DevicePtr device, vierkant::raytracing_pipeline_info_t raytracing_info)
{
    auto vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(
            device->handle(), "vkCreateRayTracingPipelinesKHR"));

    if(!vkCreateRayTracingPipelinesKHR){ return nullptr; }

    // shader stages
    auto stage_create_infos = shader_stage_create_infos(raytracing_info.shader_stages,
                                                        raytracing_info.specialization_info);

    // shader groups
    auto group_create_infos = raytracing_shader_groups(raytracing_info.shader_stages);

    // define pipeline layout (uniforms, push-constants, ...)
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(raytracing_info.descriptor_set_layouts.size());
    pipeline_layout_info.pSetLayouts = raytracing_info.descriptor_set_layouts.data();
    pipeline_layout_info.pushConstantRangeCount = static_cast<uint32_t>(raytracing_info.push_constant_ranges.size());
    pipeline_layout_info.pPushConstantRanges = raytracing_info.push_constant_ranges.data();

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    vkCheck(vkCreatePipelineLayout(device->handle(), &pipeline_layout_info, nullptr, &pipeline_layout),
            "failed to create pipeline layout!");

    VkRayTracingPipelineCreateInfoKHR pipeline_create_info = {};
    pipeline_create_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipeline_create_info.stageCount = stage_create_infos.size();
    pipeline_create_info.pStages = stage_create_infos.data();
    pipeline_create_info.groupCount = group_create_infos.size();
    pipeline_create_info.pGroups = group_create_infos.data();
    pipeline_create_info.layout = pipeline_layout;
    pipeline_create_info.maxPipelineRayRecursionDepth = raytracing_info.max_recursion;

    VkPipeline pipeline = VK_NULL_HANDLE;
    vkCheck(vkCreateRayTracingPipelinesKHR(device->handle(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipeline_create_info,
                                           VK_NULL_HANDLE, &pipeline), "could not create raytracing pipeline");

    return PipelinePtr(
            new Pipeline(std::move(device), pipeline_layout, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PipelinePtr Pipeline::create(DevicePtr device, vierkant::compute_pipeline_info_t compute_info)
{
    // define pipeline layout (uniforms, push-constants, ...)
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(compute_info.descriptor_set_layouts.size());
    pipeline_layout_info.pSetLayouts = compute_info.descriptor_set_layouts.data();
    pipeline_layout_info.pushConstantRangeCount = static_cast<uint32_t>(compute_info.push_constant_ranges.size());
    pipeline_layout_info.pPushConstantRanges = compute_info.push_constant_ranges.data();

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    vkCheck(vkCreatePipelineLayout(device->handle(), &pipeline_layout_info, nullptr, &pipeline_layout),
            "failed to create pipeline layout!");

    // our shader stages
    vierkant::shader_stage_map_t shader_map;
    shader_map[VK_SHADER_STAGE_COMPUTE_BIT] = compute_info.shader_stage;

    auto stage_create_infos = shader_stage_create_infos(shader_map, compute_info.specialization_info);

    VkComputePipelineCreateInfo pipeline_create_info = {};
    pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_create_info.layout = pipeline_layout;
    pipeline_create_info.stage = stage_create_infos.back();

    VkPipeline pipeline = VK_NULL_HANDLE;
    vkCheck(vkCreateComputePipelines(device->handle(), VK_NULL_HANDLE, 1, &pipeline_create_info,
                                     VK_NULL_HANDLE, &pipeline), "could not create compute pipeline");

    return PipelinePtr(
            new Pipeline(std::move(device), pipeline_layout, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Pipeline::Pipeline(DevicePtr device, VkPipelineLayout pipeline_layout, VkPipelineBindPoint bind_point,
                   VkPipeline pipeline) :
        m_device(std::move(device)),
        m_pipeline_layout(pipeline_layout),
        m_bind_point(bind_point),
        m_pipeline(pipeline)
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Pipeline::~Pipeline()
{
    if(m_device)
    {
        if(m_pipeline_layout){ vkDestroyPipelineLayout(m_device->handle(), m_pipeline_layout, nullptr); }
        if(m_pipeline){ vkDestroyPipeline(m_device->handle(), m_pipeline, nullptr); }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Pipeline::bind(VkCommandBuffer command_buffer)
{
    // bind pipeline
    vkCmdBindPipeline(command_buffer, m_bind_point, m_pipeline);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

}// namespace vierkant
