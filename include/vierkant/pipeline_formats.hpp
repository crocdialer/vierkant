//
// Created by crocdialer on 1/20/21.
//

#pragma once

#include <array>
#include <optional>
#include <vierkant/Device.hpp>
#include <vierkant/math.hpp>

namespace vierkant
{

//! shared handle for a VkShaderModule
using ShaderModulePtr = std::shared_ptr<VkShaderModule_T>;

using shader_stage_map_t = std::map<VkShaderStageFlagBits, ShaderModulePtr>;

//! raytracing pipelines can provide multiple shaders per stage
using raytracing_shader_map_t = std::multimap<VkShaderStageFlagBits, ShaderModulePtr>;

/**
 * @brief   Helper function to create a shared VkShaderModule
 *
 * @param   device      handle for the vk::Device to create the VkShaderModule
 * @param   spirv_code  the SPIR-V bytecode for the shader
 * @param   num_bytes   number of bytes in @ref spirv_code
 * @param   group_count optional pointer to a writable uvec3.
 *                      can be used to extract the thread-group-counts of a compute-shader-module.
 *
 * @return  a newly constructed, shared VkShaderModule
 */
ShaderModulePtr create_shader_module(const DevicePtr &device, const void *spirv_code, size_t num_bytes,
                                     glm::uvec3 *group_count);

template<typename T>
ShaderModulePtr create_shader_module(const DevicePtr &device, const T &array, glm::uvec3 *group_count = nullptr)
{
    return create_shader_module(device, array.data(), sizeof(typename T::value_type) * array.size(), group_count);
}

std::vector<VkRayTracingShaderGroupCreateInfoKHR>
raytracing_shader_groups(const raytracing_shader_map_t &shader_stages);

/**
 * @brief   ShaderType is used to refer to different sets of shader-stages
 */
enum class ShaderType
{
    UNLIT,
    UNLIT_COLOR,
    UNLIT_COLOR_SKIN,
    UNLIT_TEXTURE,
    UNLIT_TEXTURE_SKIN,
    UNLIT_CUBE,
    FULLSCREEN_GRID,
    FULLSCREEN_TEXTURE,
    FULLSCREEN_TEXTURE_DEPTH,
    CUSTOM
};

/**
 * @brief   Get a map with shader-stages for a given Shadertype.
 *
 * @param   device  handle for the vk::Device to create the VkShaderModules
 * @param   t       the Shadertype to return the shader-stages for
 * @return  the newly constructed map containing the VkShaderModules
 */
shader_stage_map_t create_shader_stages(const DevicePtr &device, ShaderType t);

/**
 * @brief   pipeline_specialization is used to handle shader/pipeline specialization-constants.
 */
class pipeline_specialization
{
public:
    std::map<uint32_t, std::array<uint8_t, 4>> constant_blobs;

    const VkSpecializationInfo *info()
    {
        m_map_entries.clear();
        m_data.clear();

        for(const auto &[constant_id, blob]: constant_blobs)
        {
            VkSpecializationMapEntry map_entry = {};
            map_entry.constantID = constant_id;
            map_entry.offset = static_cast<uint32_t>(m_data.size());
            map_entry.size = blob.size();

            m_map_entries.push_back(map_entry);
            m_data.insert(m_data.end(), blob.begin(), blob.end());
        }
        m_info.mapEntryCount = static_cast<uint32_t>(m_map_entries.size());
        m_info.pMapEntries = m_map_entries.data();
        m_info.dataSize = m_data.size();
        m_info.pData = m_data.data();
        return &m_info;
    }

    template<typename T>
    void set(uint32_t constant_id, const T &data)
    {
        static_assert((std::integral<T> || std::floating_point<T>) && sizeof(T) == 4, "only 32-bit numerical allowed");
        auto ptr = (uint8_t *) &data;
        auto end = ptr + sizeof(data);
        std::copy(ptr, end, constant_blobs[constant_id].data());
    }

    inline bool operator==(const pipeline_specialization &other) const
    {
        return constant_blobs == other.constant_blobs;
    }

private:
    VkSpecializationInfo m_info;
    std::vector<VkSpecializationMapEntry> m_map_entries;
    std::vector<uint8_t> m_data;
};

/**
 * @brief   graphics_pipeline_info_t groups all sort of information for a graphics pipeline.
 *          graphics_pipeline_info_t is default-constructable, copyable, compare- and hashable.
 *          Can be used as key in std::unordered_map.
 */
struct graphics_pipeline_info_t
{
    uint32_t attachment_count = 1;

    vierkant::shader_stage_map_t shader_stages;

    // vertex input assembly
    std::vector<VkVertexInputBindingDescription> binding_descriptions;
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions;

    // primitive topology
    VkPrimitiveTopology primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    bool primitive_restart = false;

    // used for patch-primitives / tesselation
    uint32_t num_patch_control_points = 0;

    VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;

    VkCullModeFlagBits cull_mode = VK_CULL_MODE_BACK_BIT;

    VkViewport viewport = {0.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    VkRect2D scissor = {{0, 0}, {0, 0}};

    //! disable rasterizer
    bool rasterizer_discard = false;

    //! enable depth read/write
    bool depth_test = true;
    bool depth_write = true;
    bool depth_clamp = false;
    VkCompareOp depth_compare_op = VK_COMPARE_OP_GREATER_OR_EQUAL;

    bool stencil_test = false;
    VkStencilOpState stencil_state_front = {};
    VkStencilOpState stencil_state_back = {};

    float line_width = 1.f;

    //! multisampling
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
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT};

    // optional attachment-specific blendStates (will override global state if present)
    std::vector<VkPipelineColorBlendAttachmentState> attachment_blend_states;

    VkRenderPass renderpass = VK_NULL_HANDLE;

    // direct rendering
    uint32_t view_mask = 0;
    std::vector<VkFormat> color_attachment_formats;
    VkFormat depth_attachment_format = VK_FORMAT_UNDEFINED;
    VkFormat stencil_attachment_format = VK_FORMAT_UNDEFINED;

    uint32_t subpass = 0;
    VkPipeline base_pipeline = VK_NULL_HANDLE;
    int32_t base_pipeline_index = -1;

    // optionally provide specialization-constants
    std::optional<vierkant::pipeline_specialization> specialization;

    //! optional VkPipelineCache
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;

    std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT};

    // descriptor set layouts / push-constants
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
    std::vector<VkPushConstantRange> push_constant_ranges;

    bool operator==(const graphics_pipeline_info_t &other) const;
};

/**
 * @brief   raytracing_pipeline_info_t groups all sort of information for a raytracing pipeline.
 *          raytracing_pipeline_info_t is default-constructable, copyable, compare- and hashable.
 *          Can be used as key in std::unordered_map.
 */
struct raytracing_pipeline_info_t
{
    raytracing_shader_map_t shader_stages;

    //! maximum recursion depth (default: 1 -> no recursion)
    uint32_t max_recursion = 1;

    //! descriptor set layouts / push-constants
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
    std::vector<VkPushConstantRange> push_constant_ranges;

    //! optional VkPipelineCache
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;

    std::optional<vierkant::pipeline_specialization> specialization;

    bool operator==(const raytracing_pipeline_info_t &other) const;
};

/**
 * @brief   compute_pipeline_info_t groups all sort of information for a compute-pipeline.
 *          compute_pipeline_info_t is default-constructable, copyable, compare- and hashable.
 *          Can be used as key in std::unordered_map.
 */
struct compute_pipeline_info_t
{
    vierkant::ShaderModulePtr shader_stage;

    //! descriptor set layouts / push-constants
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
    std::vector<VkPushConstantRange> push_constant_ranges;

    //! optional VkPipelineCache
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;

    std::optional<vierkant::pipeline_specialization> specialization;

    bool operator==(const compute_pipeline_info_t &other) const;
};

}// namespace vierkant

// template specializations for hashing
namespace std
{
template<>
struct hash<vierkant::pipeline_specialization>
{
    size_t operator()(vierkant::pipeline_specialization const &ps) const;
};

template<>
struct hash<vierkant::graphics_pipeline_info_t>
{
    size_t operator()(vierkant::graphics_pipeline_info_t const &fmt) const;
};

template<>
struct hash<vierkant::raytracing_pipeline_info_t>
{
    size_t operator()(vierkant::raytracing_pipeline_info_t const &fmt) const;
};

template<>
struct hash<vierkant::compute_pipeline_info_t>
{
    size_t operator()(vierkant::compute_pipeline_info_t const &fmt) const;
};
}// namespace std
