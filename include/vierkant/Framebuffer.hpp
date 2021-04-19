//
// Created by crocdialer on 2/16/19.
//

#pragma once

#include <map>

#include "vierkant/Semaphore.hpp"
#include "vierkant/Image.hpp"

namespace vierkant
{

using RenderPassPtr = std::shared_ptr<VkRenderPass_T>;

class Framebuffer
{
public:

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
    };

    /**
     * @brief   Enum to differentiate Image-Attachments
     */
    enum class AttachmentType
    {
        Color, Resolve, DepthStencil
    };

    using AttachmentMap = std::map<AttachmentType, std::vector<vierkant::ImagePtr>>;

    /**
     * @brief   Utility to create an AttachmentMap.
     *
     * @param   device  handle for the vierkant::Device to create the attachments with
     * @param   fmt
     * @return  a newly created AttachmentMap.
     */
    static AttachmentMap create_attachments(const vierkant::DevicePtr &device, create_info_t fmt);

    /**
     * @brief                           Utility function to create a shared RenderPass.
     *
     * @param   device                  handle for the vierkant::Device to create the RenderPass with
     * @param   attachments             an AttachmentMap to derive the RenderPass from
     * @param   subpass_dependencies    optional array of VkSubpassDependency objects
     *
     * @return  the newly created RenderpassPtr
     */
    static
    RenderPassPtr create_renderpass(const vierkant::DevicePtr &device, const AttachmentMap &attachments,
                                    bool clear_color, bool clear_depth,
                                    const std::vector<VkSubpassDependency> &subpass_dependencies = {});

    /**
     * @brief   Construct a new Framebuffer. Will create all Image-attachments,
     *          according to the requested Framebuffer::Format and a RenderPass to match those attachments.
     *
     * @param   device      handle for the vierkant::Device to create the Framebuffer with
     * @param   size        the desired size for the Framebuffer in pixels
     * @param   format      an optional Framebuffer::Format object
     */
    Framebuffer(DevicePtr device, create_info_t format, RenderPassPtr renderpass = nullptr);

    /**
     *
     * @param   device          handle for the vierkant::Device to create the Framebuffer with
     * @param   attachments     a Framebuffer::AttachmentMap holding the desired Image-attachments
     * @param   renderpass      an optional, shared RenderPass object to be used with the Framebuffer
     */
    Framebuffer(DevicePtr device, AttachmentMap attachments, RenderPassPtr renderpass = nullptr);

    Framebuffer() = default;

    Framebuffer(Framebuffer &&other) noexcept;

    Framebuffer(const Framebuffer &) = delete;

    ~Framebuffer();

    Framebuffer &operator=(Framebuffer the_other);

    /**
     * @brief   Begin a RenderPass using this Framebuffer
     * @param   commandbuffer     the VkCommandBuffer handle to record into
     * @param   subpass_contents
     */
    void begin_renderpass(VkCommandBuffer commandbuffer,
                          VkSubpassContents subpass_contents = VK_SUBPASS_CONTENTS_INLINE) const;

    /**
     * @brief   End an currently active RenderPass. Does nothing, if there is no active RenderPass
     */
    void end_renderpass() const;

    /**
     * @brief   Execute a provided array of secondary VkCommandBuffers within a Renderpass for this Framebuffer.
     *
     * @param   command_buffers an array of secondary VkCommandBuffers to render into this Framebuffer.
     */
    VkCommandBuffer record_commandbuffer(const std::vector<VkCommandBuffer> &commandbuffers);

    /**
     * @brief   Execute a provided array of secondary VkCommandBuffers within a renderpass for this framebuffer,
     *          submit to a VkQueue with optional submit_info.
     *
     * @param   command_buffers an array of secondary VkCommandBuffers to render into this Framebuffer.
     * @param   queue           a VkQueue to submit the primary VkCommandBuffer to.
     * @param   submit_info     an optional VkSubmitInfo struct.
     * @return  a fence that will be signaled when rendering is done.
     */
    VkFence submit(const std::vector<VkCommandBuffer> &commandbuffers, VkQueue queue,
                   const std::vector<vierkant::semaphore_submit_info_t>& semaphore_infos = {});

    /**
     * @brief   wait for a prior frame to finish.
     */
    void wait_fence();

    /**
     * @return  the VkExtent3D used by the Image-Attachments
     */
    const VkExtent3D &extent() const{ return m_extent; }

    /**
     *
     * @param   type an Attachment-Enum specifying the type of Attachment to query for
     * @return  the number of Image-Attachments for the specified type
     */
    size_t num_attachments(AttachmentType type = AttachmentType::Color) const;

    /**
     * @return  const-ref to a map, holding the Image-Attachments
     */
    const AttachmentMap &attachments() const{ return m_attachments; };

    /**
     * @return  the color-attachment for this index or nullptr if not found.
     */
    vierkant::ImagePtr color_attachment(uint32_t index = 0) const;

    /**
     * @return  the depth-attachment or nullptr if not found.
     */
    vierkant::ImagePtr depth_attachment() const;

    /**
     * @return  handle for the managed VkFramebuffer
     */
    VkFramebuffer handle() const{ return m_framebuffer; }

    /**
     * @return  handle for the (possibly shared) VkRenderPass
     */
    RenderPassPtr renderpass() const{ return m_renderpass; }

    /**
     * @return  true if this Framebuffer is initialized
     */
    inline explicit operator bool() const{ return m_framebuffer && m_renderpass; };


    friend void swap(Framebuffer &lhs, Framebuffer &rhs);

    /**
     * @brief   the clear-value used for Color-Attachments
     */
    VkClearColorValue clear_color = {{0.f, 0.f, 0.f, 1.f}};

    /**
     * @brief   the clear-value used for DepthStencil-Attachments
     */
    VkClearDepthStencilValue clear_depth_stencil = {1.f, 0};

private:

    DevicePtr m_device;

    VkExtent3D m_extent = {};

    AttachmentMap m_attachments;

    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;

    vierkant::FencePtr m_fence;

    vierkant::CommandPoolPtr m_command_pool = nullptr;

    vierkant::CommandBuffer m_commandbuffer;

    mutable VkCommandBuffer m_active_commandbuffer = VK_NULL_HANDLE;

    RenderPassPtr m_renderpass;

    Framebuffer::create_info_t m_format;

    void init(AttachmentMap attachments, RenderPassPtr renderpass);
};

}// namespace vierkant
