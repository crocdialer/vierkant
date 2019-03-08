//
// Created by crocdialer on 2/16/19.
//

#pragma once

#include <map>

#include "Image.hpp"

namespace vierkant {

using RenderPassPtr = std::shared_ptr<VkRenderPass_T>;

class Framebuffer
{
public:
    /**
     * @brief   Framebuffer::Format groups information necessary to create a set of Image-Attachments
     */
    struct Format
    {
        uint32_t num_color_attachments = 1;
        bool depth = false;
        bool stencil = false;
        Image::Format color_attachment_format;

        Format();
    };

    /**
     * @brief   Enum to differentiate Image-Attachments
     */
    enum class Attachment
    {
        Any = 0, Color = 1, Resolve = 2, DepthStencil = 3
    };

    using AttachmentMap = std::map<Attachment, std::vector<vierkant::ImagePtr>>;

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
                                    const std::vector<VkSubpassDependency> &subpass_dependencies = {});

    /**
     * @brief   Construct a new Framebuffer. Will create all Image-attachments,
     *          according to the requested Framebuffer::Format and a RenderPass to match those attachments.
     *
     * @param   device      handle for the vierkant::Device to create the Framebuffer with
     * @param   size        the desired size for the Framebuffer in pixels
     * @param   format      an optional Framebuffer::Format object
     */
    Framebuffer(DevicePtr device, VkExtent3D size, Format format = Format(), RenderPassPtr renderpass = nullptr);

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
     * @return  the VkExtent3D used by the Image-Attachments
     */
    const VkExtent3D &extent() const { return m_extent; }

    /**
     *
     * @param   type an Attachment-Enum specifying the type of Attachment to query for
     * @return  the number of Image-Attachments for the specified type
     */
    size_t num_attachments(Attachment type = Attachment::Any) const;

    /**
     * @return  const-ref to a map, holding the Image-Attachments
     */
    const AttachmentMap &attachments() const { return m_attachments; };

    /**
     * @return  handle for the managed VkFramebuffer
     */
    VkFramebuffer handle() const { return m_framebuffer; }

    /**
     * @return  handle for the (possibly shared) VkRenderPass
     */
    RenderPassPtr renderpass() const { return m_renderpass; }

    /**
     * @return  true if this Framebuffer is initialized
     */
    inline explicit operator bool() const { return m_framebuffer && m_renderpass; };


    friend void swap(Framebuffer &lhs, Framebuffer &rhs);

    /**
     * @brief   the clear-value used for Color-Attachments
     */
    VkClearColorValue clear_color = {0.f, 0.f, 0.f, 1.f};

    /**
     * @brief   the clear-value used for DepthStencil-Attachments
     */
    VkClearDepthStencilValue clear_depth_stencil = {1.f, 0};

private:

    DevicePtr m_device;

    VkExtent3D m_extent = {};

    AttachmentMap m_attachments;

    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;

    mutable VkCommandBuffer m_active_commandbuffer = VK_NULL_HANDLE;

    RenderPassPtr m_renderpass;

    Framebuffer::Format m_format;

    AttachmentMap create_attachments(const Format &fmt);

    void init(AttachmentMap attachments, RenderPassPtr renderpass);
};

}// namespace vierkant
