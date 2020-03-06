//
// Created by crocdialer on 11/14/18.
//

#pragma once

#include "vierkant/Device.hpp"

namespace vierkant
{

using ShaderModulePtr = std::shared_ptr<VkShaderModule_T>;

using shader_stage_map_t = std::map<VkShaderStageFlagBits, ShaderModulePtr>;

/**
 * @brief   Helper function to create a shared VkShaderModule
 *
 * @param   device      handle for the vk::Device to create the VkShaderModule
 * @param   spirv_code  the SPIR-V bytecode for the shader
 * @return  a newly constructed, shared VkShaderModule
 */
ShaderModulePtr create_shader_module(const DevicePtr &device,
                                     const void *spirv_code,
                                     size_t num_bytes);

template<typename T>
ShaderModulePtr create_shader_module(const DevicePtr &device,
                                     const T &array)
{
    return create_shader_module(device, array.data(), sizeof(typename T::value_type) * array.size());
}

/**
 * @brief   ShaderType is used to refer to different sets of shader-stages
 */
enum class ShaderType
{
    UNLIT_COLOR, UNLIT_TEXTURE, UNLIT_SKIN, CUSTOM
};

/**
 * @brief   Get a map with shader-stages for a given Shadertype.
 *
 * @param   device  handle for the vk::Device to create the VkShaderModules
 * @param   t       the Shadertype to return the shader-stages for
 * @return  the newly constructed map containing the VkShaderModules
 */
shader_stage_map_t create_shader_stages(const DevicePtr &device, ShaderType t);

DEFINE_CLASS_PTR(Pipeline)

class Pipeline
{
public:

    /**
     * @brief   Format groups all sort of information, necessary to describe and create a vk::Pipeline.
     *          Format is default-constructable, trivially copyable, comparable and hashable.
     *          Can be used as key in std::unordered_map.
     */
    struct Format
    {
        uint32_t attachment_count = 1;

        std::map<VkShaderStageFlagBits, ShaderModulePtr> shader_stages;

        // vertex input assembly
        std::vector<VkVertexInputBindingDescription> binding_descriptions;
        std::vector<VkVertexInputAttributeDescription> attribute_descriptions;

        // primitive topology
        VkPrimitiveTopology primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        bool primitive_restart = false;

        VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;

        VkCullModeFlagBits cull_mode = VK_CULL_MODE_BACK_BIT;

        VkViewport viewport = {0.f, 0.f, 0.f, 0.f, 0.f, 1.f};
        VkRect2D scissor = {{0, 0},
                            {0, 0}};

        // disable rasterizer entirely
        bool rasterizer_discard = false;

        // enable depth read/write
        bool depth_test = true;
        bool depth_write = true;
        bool depth_clamp = false;

        bool stencil_test = false;
        VkStencilOpState stencil_state_front = {};
        VkStencilOpState stencil_state_back = {};

        float line_width = 1.f;

        // mutlisampling
        VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
        bool sample_shading = false;
        float min_sample_shading = 1.f;

        // global blend-state for the pipeline
        VkPipelineColorBlendAttachmentState blend_state = {

                // enable blending
                .blendEnable = static_cast<VkBool32>(false),

                // color blending
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,

                // alpha blending
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,

                // color mask
                .colorWriteMask =  VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        };

        // optional attachment-specific blendStates (will override global state if present)
        std::vector<VkPipelineColorBlendAttachmentState> attachment_blend_states;

        VkRenderPass renderpass = VK_NULL_HANDLE;

        uint32_t subpass = 0;
        VkPipeline base_pipeline = VK_NULL_HANDLE;
        int32_t base_pipeline_index = -1;

        VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
        std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT};

        // descriptor set layouts / push-constants
        std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
        std::vector<VkPushConstantRange> push_constant_ranges;

        bool operator==(const Format &other) const;

        bool operator!=(const Format &other) const{ return !(*this == other); };
    };

    /**
     * @brief   Construct a new Pipeline object
     *
     * @param   device  handle for the vk::Device to create the Pipeline
     * @param   format  the desired Pipeline::Format
     */
    static PipelinePtr create(DevicePtr device, Format format);

    Pipeline(Pipeline &&other) noexcept = delete;

    Pipeline(const Pipeline &) = delete;

    ~Pipeline();

    Pipeline &operator=(Pipeline other) = delete;

    /**
     * @brief
     * @param   command_buffer
     */
    void bind(VkCommandBuffer command_buffer);

    /**
     * @return  handle for the managed VkPipeline
     */
    VkPipeline handle() const{ return m_pipeline; }

    /**
     * @return  handle for the managed pipeline-layout
     */
    VkPipelineLayout layout() const{ return m_pipeline_layout; }

private:

    Pipeline(DevicePtr device, Format format);

    DevicePtr m_device;

    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;

    VkPipeline m_pipeline = VK_NULL_HANDLE;

    Pipeline::Format m_format;
};

}//namespace vierkant

// comparison operators for some vulkan-structs used by vierkant::Pipeline

bool operator==(const VkVertexInputBindingDescription &lhs, const VkVertexInputBindingDescription &rhs);

bool operator!=(const VkVertexInputBindingDescription &lhs, const VkVertexInputBindingDescription &rhs);

bool operator==(const VkVertexInputAttributeDescription &lhs, const VkVertexInputAttributeDescription &rhs);

bool operator!=(const VkVertexInputAttributeDescription &lhs, const VkVertexInputAttributeDescription &rhs);

bool operator==(const VkPipelineColorBlendAttachmentState &lhs, const VkPipelineColorBlendAttachmentState &rhs);

bool operator!=(const VkPipelineColorBlendAttachmentState &lhs, const VkPipelineColorBlendAttachmentState &rhs);

bool operator==(const VkStencilOpState &lhs, const VkStencilOpState &rhs);

bool operator!=(const VkStencilOpState &lhs, const VkStencilOpState &rhs);

bool operator==(const VkPushConstantRange &lhs, const VkPushConstantRange &rhs);

bool operator!=(const VkPushConstantRange &lhs, const VkPushConstantRange &rhs);

// template specializations for hashing
namespace std
{
template<>
struct hash<vierkant::Pipeline::Format>
{
    size_t operator()(vierkant::Pipeline::Format const &fmt) const;
};
}