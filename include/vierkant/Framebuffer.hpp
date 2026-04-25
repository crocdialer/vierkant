//
// Created by crocdialer on 2/16/19.
//

#pragma once

#include <map>
#include <optional>

#include "vierkant/Image.hpp"
#include "vierkant/Semaphore.hpp"

namespace vierkant
{

/**
 * @brief   Enum to differentiate Image-Attachments
 */
enum class AttachmentType : uint32_t
{
    Color = 0,
    Resolve,
    DepthStencil
};

using attachment_map_t = std::map<AttachmentType, std::vector<vierkant::ImagePtr>>;

class Framebuffer
{
public:
    //! group parameters for  'begin_rendering'-routine
    struct begin_rendering_info_t
    {
        bool use_color_attachment = true;
        bool clear_color_attachment = true;
        bool use_depth_attachment = true;
        bool clear_depth_attachment = true;
        VkRenderingFlags flags = 0;
    };

    struct end_rendering_info_t
    {
        VkImageLayout final_layout_color = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout final_layout_depth = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    /**
     * @brief   Framebuffer::Format groups information necessary to create a set of Image-Attachments
     */
    struct create_info_t
    {
        VkExtent3D size;
        uint32_t num_color_attachments = 1;
        bool depth = false;
        bool stencil = false;
        bool clear_color = true;
        bool clear_depth = true;
        Image::Format color_attachment_format;
        Image::Format depth_attachment_format;
        vierkant::CommandPoolPtr command_pool = nullptr;
        VkQueue queue = VK_NULL_HANDLE;
        begin_rendering_info_t begin_rendering_info = {};
        end_rendering_info_t end_rendering_info = {};
        std::optional<vierkant::debug_label_t> debug_label;
    };

    /**
     * @brief   Utility to create an AttachmentMap.
     *
     * @param   device  handle for the vierkant::Device to create the attachments with
     * @param   fmt
     * @return  a newly created AttachmentMap.
     */
    static attachment_map_t create_attachments(const vierkant::DevicePtr &device, create_info_t fmt);

    /**
     * @brief   Construct a new Framebuffer. Will create all Image-attachments,
     *          according to the requested Framebuffer::Format and a RenderPass to match those attachments.
     *
     * @param   device      handle for the vierkant::Device to create the Framebuffer with
     * @param   size        the desired size for the Framebuffer in pixels
     * @param   create_info      an optional Framebuffer::Format object
     */
    Framebuffer(DevicePtr device, create_info_t create_info);

    /**
     *
     * @param   device          handle for the vierkant::Device to create the Framebuffer with
     * @param   attachments     a Framebuffer::AttachmentMap holding the desired Image-attachments
     */
    Framebuffer(DevicePtr device, attachment_map_t attachments, const create_info_t &create_info);

    Framebuffer() = default;

    Framebuffer(Framebuffer &&other) noexcept;

    Framebuffer(const Framebuffer &) = delete;

    Framebuffer &operator=(Framebuffer the_other);

    /**
     * @brief   Execute a provided array of secondary VkCommandBuffers within a dynamic rendering scope.
     *
     * @param   commandbuffers an array of secondary VkCommandBuffers to render into this Framebuffer.
     */
    VkCommandBuffer record_commandbuffer(const std::vector<VkCommandBuffer> &commandbuffers);

    /**
     * @brief   Execute a provided array of secondary VkCommandBuffers within a renderpass for this Framebuffer,
     *          submit to a VkQueue with optional submit_info.
     *
     * @param   commandbuffers  an array of secondary VkCommandBuffers to render into this Framebuffer.
     * @param   queue           a VkQueue to submit the primary VkCommandBuffer to.
     * @param   semaphore_infos an optional array of semaphore_submit_info_t, can be used to pass in signal/wait semaphores
     * @return  a fence that will be signaled when rendering is done.
     */
    VkFence submit(const std::vector<VkCommandBuffer> &commandbuffers, VkQueue queue,
                   const std::vector<vierkant::semaphore_submit_info_t> &semaphore_infos = {});

    /**
     * @brief   wait for a prior frame to finish.
     */
    void wait_fence();

    /**
     * @brief   Begin a direct-rendering-pass using this Framebuffer.
     *
     * @param   commandbuffer   a (primary) VkCommandBuffer handle to record into
     * @param   info            a begin_rendering_info_t struct
     */
    void begin_rendering(VkCommandBuffer commandbuffer, const begin_rendering_info_t &info) const;

    /**
     * @brief   End a direct-rendering-pass using this Framebuffer.
     */
    void end_rendering(const end_rendering_info_t &end_rendering_info) const;

    /**
     * @return  the VkExtent3D used by the Image-Attachments
     */
    const VkExtent3D &extent() const { return m_extent; }

    /**
     *
     * @param   type an Attachment-Enum specifying the type of Attachment to query for
     * @return  the number of Image-Attachments for the specified type
     */
    size_t num_attachments(AttachmentType type = AttachmentType::Color) const;

    /**
     * @return  const-ref to a map, holding the Image-Attachments
     */
    const attachment_map_t &attachments() const { return m_attachments; };

    /**
     * @return  the color-attachment for this index or nullptr if not found.
     */
    vierkant::ImagePtr color_attachment(uint32_t index = 0) const;

    /**
     * @return  the depth-attachment or nullptr if not found.
     */
    vierkant::ImagePtr depth_attachment() const;

    /**
     * @return  true if this Framebuffer is initialized
     */
    inline explicit operator bool() const { return !m_attachments.empty(); };


    friend void swap(Framebuffer &lhs, Framebuffer &rhs) noexcept;

    /**
     * @brief   the clear-value used for Color-Attachments
     */
    glm::vec4 clear_color = {0.f, 0.f, 0.f, 1.f};

    /**
     * @brief   the clear-value used for DepthStencil-Attachments
     */
    VkClearDepthStencilValue clear_depth_stencil = {0.f, 0};

    //! optional debug label. will be attached to renderpass-submissions
    std::optional<vierkant::debug_label_t> debug_label;

private:
    void init(attachment_map_t attachments);

    DevicePtr m_device;

    VkExtent3D m_extent = {};

    attachment_map_t m_attachments;

    vierkant::FencePtr m_fence;

    vierkant::CommandPoolPtr m_command_pool = nullptr;

    vierkant::CommandBuffer m_commandbuffer;

    mutable VkCommandBuffer m_active_commandbuffer = VK_NULL_HANDLE, m_direct_rendering_commandbuffer = VK_NULL_HANDLE;

    Framebuffer::create_info_t m_format;
};

}// namespace vierkant
