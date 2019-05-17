#include <utility>

//
// Created by crocdialer on 2/16/19.
//

#include "../include/vierkant/Framebuffer.hpp"


namespace vierkant {

///////////////////////////////////////////////////////////////////////////////////////////////////

Framebuffer::Format::Format() :
        num_color_attachments(1),
        depth(false),
        stencil(false)
{
    color_attachment_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    color_attachment_format.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

RenderPassPtr
Framebuffer::create_renderpass(const vierkant::DevicePtr &device,
                               const Framebuffer::AttachmentMap &attachments,
                               const std::vector<VkSubpassDependency> &subpass_dependencies)
{
    VkRenderPass renderpass = VK_NULL_HANDLE;

    auto check_attachment = [](Attachment attachment, const AttachmentMap &map) -> bool
    {
        for(const auto &pair : map)
        {
            if(pair.first == attachment && !pair.second.empty()){ return true; }
        }
        return false;
    };
    bool has_depth_stencil = check_attachment(Attachment::DepthStencil, attachments);
    bool has_resolve = check_attachment(Attachment::Resolve, attachments);

    // TODO: sanity check number of attachments (>= 0 color, <= 1 depth/stencil, resolve==color or 0 !?)

    // create RenderPass according to AttachmentMap
    std::vector<VkAttachmentDescription> attachment_descriptions;

    for(auto &pair : attachments)
    {
        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        switch(pair.first)
        {
            case Attachment::Color:
                loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                break;

            case Attachment::Resolve:
                loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                break;

            case Attachment::DepthStencil:
                loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                storeOp = VK_ATTACHMENT_STORE_OP_STORE;

                if(has_depth_stencil)
                {
                    stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                    stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
                }
                break;

            default:
                break;
        }

        for(const auto &img : pair.second)
        {
            VkAttachmentDescription description = {};
            description.format = img->format().format;
            description.samples = img->format().sample_count;
            description.loadOp = loadOp;
            description.storeOp = storeOp;
            description.stencilLoadOp = stencilLoadOp;
            description.stencilStoreOp = stencilStoreOp;
            description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            description.finalLayout = img->image_layout();
            attachment_descriptions.push_back(description);
        }
    }

    uint32_t attachment_index = 0, num_color_images = 0;
    std::vector<VkAttachmentReference> color_refs, depth_stencil_refs, resolve_refs;

    auto color_it = attachments.find(Attachment::Color);

    if(color_it != attachments.end()){ num_color_images = static_cast<uint32_t>(color_it->second.size()); }

    for(uint32_t i = 0; i < num_color_images; ++i)
    {
        VkAttachmentReference color_attachment_ref = {};
        color_attachment_ref.attachment = attachment_index++;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_refs.push_back(color_attachment_ref);

        if(has_resolve)
        {
            VkAttachmentReference color_attachment_resolve_ref = {};
            color_attachment_resolve_ref.attachment = attachment_index++;
            color_attachment_resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            resolve_refs.push_back(color_attachment_resolve_ref);
        }
    }
    if(has_depth_stencil)
    {
        VkAttachmentReference depth_attachment_ref = {};
        depth_attachment_ref.attachment = attachment_index++;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_stencil_refs = {depth_attachment_ref};
    }

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(color_refs.size());
    subpass.pColorAttachments = color_refs.data();
    subpass.pDepthStencilAttachment = depth_stencil_refs.empty() ? nullptr : depth_stencil_refs.data();
    subpass.pResolveAttachments = resolve_refs.empty() ? nullptr : resolve_refs.data();

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachment_descriptions.size());
    render_pass_info.pAttachments = attachment_descriptions.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = static_cast<uint32_t>(subpass_dependencies.size());
    render_pass_info.pDependencies = subpass_dependencies.data();

    vkCheck(vkCreateRenderPass(device->handle(), &render_pass_info, nullptr, &renderpass),
            "failed to create render pass!");

    return RenderPassPtr(renderpass, [device](VkRenderPass p) { vkDestroyRenderPass(device->handle(), p, nullptr); });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Framebuffer::Framebuffer(DevicePtr device, VkExtent3D size, Format format, RenderPassPtr renderpass) :
        m_device(std::move(device)),
        m_extent(size),
        m_format(std::move(format))
{
    m_format.color_attachment_format.extent = m_extent;
    init(create_attachments(m_format), std::move(renderpass));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Framebuffer::Framebuffer(DevicePtr device, AttachmentMap attachments, RenderPassPtr renderpass) :
        m_device(std::move(device))
{
    init(std::move(attachments), std::move(renderpass));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Framebuffer::Framebuffer(Framebuffer &&other) noexcept :
        Framebuffer()
{
    swap(*this, other);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Framebuffer::~Framebuffer()
{
    if(m_device)
    {
        m_extent = {};
        clear_color = {};
        m_attachments.clear();

        if(m_framebuffer)
        {
            vkDestroyFramebuffer(m_device->handle(), m_framebuffer, nullptr);
            m_framebuffer = VK_NULL_HANDLE;
        }
        m_renderpass.reset();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Framebuffer &Framebuffer::operator=(Framebuffer other)
{
    swap(*this, other);
    return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Framebuffer::begin_renderpass(VkCommandBuffer commandbuffer,
                                   VkSubpassContents subpass_contents) const
{
    if(*this && !m_active_commandbuffer)
    {
        VkRenderPassBeginInfo render_pass_info = {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = m_renderpass.get();
        render_pass_info.framebuffer = m_framebuffer;
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = {m_extent.width, m_extent.height};

        // clear values
        std::vector<VkClearValue> clear_values;

        for(const auto &pair : m_attachments)
        {
            VkClearValue v = {};

            switch(pair.first)
            {
                case Attachment::Color:
                    v.color = clear_color;
                    break;
                case Attachment::DepthStencil:
                    v.depthStencil = clear_depth_stencil;
                    break;

                default:
                    break;
            }
            clear_values.push_back(v);
        }

        render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
        render_pass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(commandbuffer, &render_pass_info, subpass_contents);
        m_active_commandbuffer = commandbuffer;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Framebuffer::end_renderpass() const
{
    if(*this && m_active_commandbuffer)
    {
        vkCmdEndRenderPass(m_active_commandbuffer);
        m_active_commandbuffer = VK_NULL_HANDLE;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

size_t Framebuffer::num_attachments(vierkant::Framebuffer::Attachment type) const
{
    size_t ret = 0;

    for(const auto &pair : m_attachments)
    {
        if(type == pair.first || type == Attachment::Any){ ret += pair.second.size(); }
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Framebuffer::init(AttachmentMap attachments, RenderPassPtr renderpass)
{
    clear_color = {{0.f, 0.f, 0.f, 1.f}};
    clear_depth_stencil = {1.0f, 0};

    if(!renderpass){ renderpass = create_renderpass(m_device, attachments); }
    m_renderpass = renderpass;

    VkExtent3D extent = {0, 0, 0};

    auto is_extent_equal = [](const VkExtent3D &lhs, const VkExtent3D &rhs) -> bool
    {
        return lhs.width == rhs.width && lhs.height == rhs.height && lhs.depth == rhs.depth;
    };

    auto is_extent_zero = [](const VkExtent3D &e) -> bool
    {
        return !e.width && !e.height && !e.depth;
    };

    std::vector<VkImageView> attachment_views;

    for(const auto &pair : attachments)
    {
        for(const auto &img : pair.second)
        {
            attachment_views.push_back(img->image_view());

            // get initial extent
            if(is_extent_zero(extent)){ extent = img->extent(); }
            if(!is_extent_equal(extent, img->extent()))
            {
                // image dimensions do not match
                throw std::invalid_argument("framebuffer: attachment sizes do not match");
            }
        }
    }

    // create vkFramebuffer using the existing VkRenderpass
    VkFramebufferCreateInfo framebuffer_info = {};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = m_renderpass.get();
    framebuffer_info.attachmentCount = static_cast<uint32_t>(attachment_views.size());
    framebuffer_info.pAttachments = attachment_views.data();
    framebuffer_info.width = extent.width;
    framebuffer_info.height = extent.height;
    framebuffer_info.layers = m_format.color_attachment_format.num_layers;

    vkCheck(vkCreateFramebuffer(m_device->handle(), &framebuffer_info, nullptr, &m_framebuffer),
            "failed to create framebuffer!");

    // now move attachments
    m_attachments = std::move(attachments);
    m_extent = extent;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Framebuffer::AttachmentMap Framebuffer::create_attachments(const Framebuffer::Format &fmt)
{
    // create vierkant::Image attachments and insert into AttachmentMap
    std::vector<vierkant::ImagePtr> color_attachments, resolve_attachments, depth_stencil_attachments;

    // color attachments
    for(uint32_t i = 0; i < fmt.num_color_attachments; ++i)
    {
        auto img = vierkant::Image::create(m_device, fmt.color_attachment_format);
        color_attachments.push_back(img);

        // multisampling requested -> add resolve attachment
        if(fmt.color_attachment_format.sample_count != VK_SAMPLE_COUNT_1_BIT)
        {
            auto resolve_fmt = fmt.color_attachment_format;
            resolve_fmt.sample_count = VK_SAMPLE_COUNT_1_BIT;
            resolve_fmt.extent = m_extent;
            auto resolve_img = vierkant::Image::create(m_device, resolve_fmt);
            resolve_attachments.push_back(resolve_img);
        }
    }
    // depth/stencil attachment
    if(fmt.depth || fmt.stencil)
    {
        vierkant::Image::Format img_fmt;
        img_fmt.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        img_fmt.sample_count = fmt.color_attachment_format.sample_count;
        img_fmt.extent = m_extent;
        img_fmt.format = VK_FORMAT_D32_SFLOAT;
        img_fmt.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

        if(fmt.stencil)
        {
            img_fmt.aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
            img_fmt.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
        }
        auto depth_img = vierkant::Image::create(m_device, img_fmt);
        depth_stencil_attachments.push_back(depth_img);
    }

    AttachmentMap attachments;
    if(!color_attachments.empty()){ attachments[Attachment::Color] = std::move(color_attachments); }
    if(!depth_stencil_attachments.empty())
    {
        attachments[Attachment::DepthStencil] = std::move(depth_stencil_attachments);
    }
    if(!resolve_attachments.empty()){ attachments[Attachment::Resolve] = std::move(resolve_attachments); }
    return attachments;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void swap(Framebuffer &lhs, Framebuffer &rhs)
{
    std::swap(lhs.clear_color, rhs.clear_color);
    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_extent, rhs.m_extent);
    std::swap(lhs.m_attachments, rhs.m_attachments);
    std::swap(lhs.m_framebuffer, rhs.m_framebuffer);
    std::swap(lhs.m_active_commandbuffer, rhs.m_active_commandbuffer);
    std::swap(lhs.m_renderpass, rhs.m_renderpass);
    std::swap(lhs.m_format, rhs.m_format);
}

}// namespace vierkant
