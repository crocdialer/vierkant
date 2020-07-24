#include "vierkant/Framebuffer.hpp"


namespace vierkant
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline bool is_depth(VkFormat fmt)
{
    VkFormat depth_formats[] = {VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,
                                VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT};
    return crocore::contains(depth_formats, fmt);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline bool is_stencil(VkFormat fmt)
{
    VkFormat stencil_formats[] = {VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,
                                  VK_FORMAT_D32_SFLOAT_S8_UINT};
    return crocore::contains(stencil_formats, fmt);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RenderPassPtr
Framebuffer::create_renderpass(const vierkant::DevicePtr &device,
                               const Framebuffer::AttachmentMap &attachments,
                               bool clear_color, bool clear_depth,
                               const std::vector<VkSubpassDependency> &subpass_dependencies)
{
    VkRenderPass renderpass = VK_NULL_HANDLE;

    auto check_attachment = [](AttachmentType attachment, const AttachmentMap &map) -> bool
    {
        for(const auto &pair : map)
        {
            if(pair.first == attachment && !pair.second.empty()){ return true; }
        }
        return false;
    };
    bool has_depth_stencil = check_attachment(AttachmentType::DepthStencil, attachments);
    bool has_resolve = check_attachment(AttachmentType::Resolve, attachments);

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
            case AttachmentType::Color:
                loadOp = clear_color ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
                storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                break;

            case AttachmentType::Resolve:
                loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                break;

            case AttachmentType::DepthStencil:
                loadOp = clear_depth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
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
            description.initialLayout = img->image_layout();//VK_IMAGE_LAYOUT_UNDEFINED;
            description.finalLayout = img->image_layout();
            attachment_descriptions.push_back(description);
        }
    }

    uint32_t attachment_index = 0, num_color_images = 0;
    std::vector<VkAttachmentReference> color_refs, depth_stencil_refs, resolve_refs;

    auto color_it = attachments.find(AttachmentType::Color);

    if(color_it !=
       attachments.end()){ num_color_images = static_cast<uint32_t>(color_it->second.size()); }

    for(uint32_t i = 0; i < num_color_images; ++i)
    {
        VkAttachmentReference color_attachment_ref = {};
        color_attachment_ref.attachment = i;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_refs.push_back(color_attachment_ref);
        attachment_index++;

        if(has_resolve)
        {
            VkAttachmentReference color_attachment_resolve_ref = {};
            color_attachment_resolve_ref.attachment = i + num_color_images;
            color_attachment_resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            resolve_refs.push_back(color_attachment_resolve_ref);
            attachment_index++;
        }
    }
    if(has_depth_stencil)
    {
        VkAttachmentReference depth_attachment_ref = {};
        depth_attachment_ref.attachment = attachment_index;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_stencil_refs = {depth_attachment_ref};
    }

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(color_refs.size());
    subpass.pColorAttachments = color_refs.data();
    subpass.pDepthStencilAttachment = depth_stencil_refs.empty() ? nullptr
                                                                 : depth_stencil_refs.data();
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

    return RenderPassPtr(renderpass, [device](VkRenderPass p)
    {
        vkDestroyRenderPass(device->handle(), p, nullptr);
    });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Framebuffer::Framebuffer(DevicePtr device, create_info_t format, RenderPassPtr renderpass) :
        m_device(std::move(device)), m_extent(format.size), m_format(std::move(format))
{
    m_format.color_attachment_format.extent = m_extent;

    if(m_format.depth || m_format.stencil)
    {
        if(!(m_format.depth_attachment_format.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
        {
            m_format.depth_attachment_format.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        m_format.depth_attachment_format.sample_count = m_format.color_attachment_format.sample_count;
        m_format.depth_attachment_format.extent = m_extent;

        if(!is_depth(m_format.depth_attachment_format.format))
        {
            m_format.depth_attachment_format.format = VK_FORMAT_D24_UNORM_S8_UINT;
        }
        m_format.depth_attachment_format.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

        if(m_format.stencil)
        {
            m_format.depth_attachment_format.aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

            if(!is_stencil(m_format.depth_attachment_format.format))
            {
                m_format.depth_attachment_format.format = VK_FORMAT_D24_UNORM_S8_UINT;
            }
        }
    }

    init(create_attachments(m_format), std::move(renderpass));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Framebuffer::Framebuffer(DevicePtr device, AttachmentMap attachments, RenderPassPtr renderpass)
        :
        m_device(std::move(device))
{
    init(std::move(attachments), std::move(renderpass));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Framebuffer::Framebuffer(Framebuffer &&other) noexcept:
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
            auto type = pair.first;
            auto &images = pair.second;

            for(uint32_t i = 0; i < images.size(); ++i)
            {
                VkClearValue v = {};

                switch(type)
                {
                    case AttachmentType::Color:
                        v.color = clear_color;
                        break;
                    case AttachmentType::DepthStencil:
                        v.depthStencil = clear_depth_stencil;
                        break;

                    default:
                        break;
                }
                clear_values.push_back(v);
            }
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

////////////////////////////////////////////////////////////////////////////////////////////////

VkFence
Framebuffer::submit(const std::vector<VkCommandBuffer> &commandbuffers, VkQueue queue,
                    VkSubmitInfo submit_info)
{
    // wait for prior fence
    VkFence fence = m_fence.get();
    vkWaitForFences(m_device->handle(), 1, &fence, VK_TRUE,
                    std::numeric_limits<uint64_t>::max());
    vkResetFences(m_device->handle(), 1, &fence);

    // record commandbuffer
    m_commandbuffer.begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

    // begin the renderpass
    begin_renderpass(m_commandbuffer.handle(), VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    // execute secondary commandbuffers
    if(!commandbuffers.empty())
    {
        vkCmdExecuteCommands(m_commandbuffer.handle(), commandbuffers.size(),
                             commandbuffers.data());
    }

    // end renderpass
    end_renderpass();

    // submit primary commandbuffer
    m_commandbuffer.submit(queue, false, fence, submit_info);

    return fence;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Framebuffer::wait_fence()
{
    // wait for prior fence
    VkFence fence = m_fence.get();
    vkWaitForFences(m_device->handle(), 1, &fence, VK_TRUE,
                    std::numeric_limits<uint64_t>::max());
}

///////////////////////////////////////////////////////////////////////////////////////////////////

size_t Framebuffer::num_attachments(vierkant::Framebuffer::AttachmentType type) const
{
    size_t ret = 0;

    for(const auto &pair : m_attachments)
    {
        if(type == pair.first || type == AttachmentType::Any){ ret += pair.second.size(); }
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Framebuffer::init(AttachmentMap attachments, RenderPassPtr renderpass)
{
    m_commandbuffer = vierkant::CommandBuffer(m_device, m_device->command_pool_transient());
    m_fence = vierkant::create_fence(m_device, true);

    clear_color = {{0.f, 0.f, 0.f, 1.f}};
    clear_depth_stencil = {1.0f, 0};

    if(!renderpass){ renderpass = create_renderpass(m_device, attachments, true, true); }
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

    vkCheck(vkCreateFramebuffer(m_device->handle(), &framebuffer_info, nullptr,
                                &m_framebuffer),
            "failed to create framebuffer!");

    // now move attachments
    m_attachments = std::move(attachments);
    m_extent = extent;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Framebuffer::AttachmentMap Framebuffer::create_attachments(Framebuffer::create_info_t fmt)
{
    fmt.color_attachment_format.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

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
        auto depth_img = vierkant::Image::create(m_device, fmt.depth_attachment_format);
        depth_stencil_attachments.push_back(depth_img);
    }

    AttachmentMap attachments;
    if(!color_attachments.empty())
    {
        attachments[AttachmentType::Color] = std::move(color_attachments);
    }
    if(!depth_stencil_attachments.empty())
    {
        attachments[AttachmentType::DepthStencil] = std::move(depth_stencil_attachments);
    }
    if(!resolve_attachments.empty())
    {
        attachments[AttachmentType::Resolve] = std::move(resolve_attachments);
    }
    return attachments;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr Framebuffer::color_attachment(uint32_t index) const
{
    // check for resolve-attachment, fallback to color-attachment
    auto it = m_attachments.find(AttachmentType::Resolve);

    if(it == m_attachments.end())
    {
        it = m_attachments.find(AttachmentType::Color);
    }

    if(it != m_attachments.end())
    {
        auto &color_attachments = it->second;

        if(!color_attachments.empty())
        {
            if(index >= color_attachments.size())
            {
                throw std::out_of_range("attachment-index out of bounds: " + std::to_string(index));
            }
            return color_attachments[index];
        }
    }
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr Framebuffer::depth_attachment() const
{
    auto it = m_attachments.find(AttachmentType::DepthStencil);

    if(it != m_attachments.end())
    {
        auto &depth_attachments = it->second;

        if(!depth_attachments.empty()){ return depth_attachments.front(); }
    }
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void swap(Framebuffer &lhs, Framebuffer &rhs)
{
    std::swap(lhs.clear_depth_stencil, rhs.clear_depth_stencil);
    std::swap(lhs.clear_color, rhs.clear_color);
    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_extent, rhs.m_extent);
    std::swap(lhs.m_attachments, rhs.m_attachments);
    std::swap(lhs.m_framebuffer, rhs.m_framebuffer);
    std::swap(lhs.m_fence, rhs.m_fence);
    std::swap(lhs.m_commandbuffer, rhs.m_commandbuffer);
    std::swap(lhs.m_active_commandbuffer, rhs.m_active_commandbuffer);
    std::swap(lhs.m_renderpass, rhs.m_renderpass);
    std::swap(lhs.m_format, rhs.m_format);
}

}// namespace vierkant
