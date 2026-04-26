#include "vierkant/Framebuffer.hpp"

#include "bc7enc/rgbcx.h"

namespace vierkant
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool is_depth(VkFormat fmt)
{
    constexpr VkFormat depth_formats[] = {VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,
                                          VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT};
    return crocore::contains(depth_formats, fmt);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool is_stencil(VkFormat fmt)
{
    constexpr VkFormat stencil_formats[] = {VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,
                                            VK_FORMAT_D32_SFLOAT_S8_UINT};
    return crocore::contains(stencil_formats, fmt);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline bool check_attachment(AttachmentType attachment, const attachment_map_t &map)
{
    return std::any_of(map.begin(), map.end(),
                       [attachment](const auto &it) { return it.first == attachment && !it.second.empty(); });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Framebuffer::Framebuffer(DevicePtr device, create_info_t create_info)
    : m_device(std::move(device)), m_extent(create_info.size), m_command_pool(create_info.command_pool),
      m_format(std::move(create_info))
{
    debug_label = m_format.debug_label;
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

    init(create_attachments(m_device, m_format));
    m_format.begin_rendering_info.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Framebuffer::Framebuffer(DevicePtr device, attachment_map_t attachments, const create_info_t &create_info)
    : m_device(std::move(device)), m_format(create_info)
{ init(std::move(attachments)); }

///////////////////////////////////////////////////////////////////////////////////////////////////

Framebuffer::Framebuffer(Framebuffer &&other) noexcept : Framebuffer() { swap(*this, other); }

///////////////////////////////////////////////////////////////////////////////////////////////////

Framebuffer &Framebuffer::operator=(Framebuffer other)
{
    swap(*this, other);
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////

VkCommandBuffer Framebuffer::record_commandbuffer(const std::vector<VkCommandBuffer> &commandbuffers)
{
    // record commandbuffer
    m_commandbuffer.begin(0);//VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
    if(debug_label) { vierkant::begin_label(m_commandbuffer.handle(), *debug_label); }

    // begin rendering
    begin_rendering(m_commandbuffer.handle(), m_format.begin_rendering_info);

    // execute secondary commandbuffers
    if(!commandbuffers.empty())
    {
        vkCmdExecuteCommands(m_commandbuffer.handle(), commandbuffers.size(), commandbuffers.data());
    }

    // passing optional final layout-info here, end dynamic rendering
    end_rendering(m_format.end_rendering_info);

    // end commandbuffer
    if(debug_label) { vierkant::end_label(m_commandbuffer.handle()); }
    m_commandbuffer.end();

    return m_commandbuffer.handle();
}

////////////////////////////////////////////////////////////////////////////////////////////////

VkFence Framebuffer::submit(const std::vector<VkCommandBuffer> &commandbuffers, VkQueue queue,
                            const std::vector<vierkant::semaphore_submit_info_t> &semaphore_infos)
{
    // wait for prior fence
    VkFence fence = m_fence.get();
    vkWaitForFences(m_device->handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
    vkResetFences(m_device->handle(), 1, &fence);

    // record a renderpass into a primary commandbuffer
    record_commandbuffer(commandbuffers);

    // submit primary commandbuffer
    m_commandbuffer.submit(queue, false, fence, semaphore_infos);

    return fence;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Framebuffer::wait_fence()
{
    // wait for prior fence
    VkFence fence = m_fence.get();
    vkWaitForFences(m_device->handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
}

///////////////////////////////////////////////////////////////////////////////////////////////////

size_t Framebuffer::num_attachments(vierkant::AttachmentType type) const
{
    auto attach_it = m_attachments.find(type);
    if(attach_it != m_attachments.end()) { return attach_it->second.size(); }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Framebuffer::init(attachment_map_t attachments)
{
    auto command_pool =
            m_command_pool ? m_command_pool
                           : vierkant::create_command_pool(m_device, vierkant::Device::Queue::GRAPHICS,
                                                           VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    m_commandbuffer = vierkant::CommandBuffer(m_device, command_pool.get());
    m_command_pool = command_pool;
    m_fence = vierkant::create_fence(m_device, true);

    VkExtent3D extent = {0, 0, 0};

    auto is_extent_equal = [](const VkExtent3D &lhs, const VkExtent3D &rhs) -> bool {
        return lhs.width == rhs.width && lhs.height == rhs.height && lhs.depth == rhs.depth;
    };

    auto is_extent_zero = [](const VkExtent3D &e) -> bool { return !e.width && !e.height && !e.depth; };

    std::vector<VkImageView> attachment_views;

    uint32_t num_layers = 0;
    for(const auto &[type, images]: attachments)
    {
        for(const auto &img: images)
        {
            attachment_views.push_back(img->image_view());

            // get initial extent
            if(is_extent_zero(extent)) { extent = img->extent(); }
            if(!is_extent_equal(extent, img->extent()))
            {
                // image dimensions do not match
                throw std::invalid_argument("framebuffer: attachment sizes do not match");
            }
            if(num_layers && num_layers != img->format().num_layers)
            {
                // image dimensions do not match
                throw std::invalid_argument("framebuffer: number of layers does not match");
            }
            num_layers = img->format().num_layers;
        }
    }
    m_format.color_attachment_format.num_layers = num_layers;
    m_format.begin_rendering_info.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

    // populate/update m_color_attachment_formats
    for(const auto &[type, images]: attachments)
    {
        if(type == AttachmentType::Color)
        {
            m_color_attachment_formats.resize(images.size());
            for(uint32_t i = 0; i < m_color_attachment_formats.size(); ++i)
            {
                m_color_attachment_formats[i] = images[i]->format().format;
            }
        }
    }

    // now move attachments
    m_attachments = std::move(attachments);
    m_extent = extent;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

attachment_map_t Framebuffer::create_attachments(const vierkant::DevicePtr &device, Framebuffer::create_info_t fmt)
{
    vierkant::CommandBuffer cmd_buf;

    if(fmt.command_pool && fmt.queue)
    {
        cmd_buf = vierkant::CommandBuffer(device, fmt.command_pool.get());
        cmd_buf.begin();

        fmt.color_attachment_format.initial_cmd_buffer = cmd_buf.handle();
        fmt.depth_attachment_format.initial_cmd_buffer = cmd_buf.handle();
    }
    fmt.color_attachment_format.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // create vierkant::Image attachments and insert into AttachmentMap
    std::vector<vierkant::ImagePtr> color_attachments, resolve_attachments, depth_stencil_attachments;

    // color attachments
    for(uint32_t i = 0; i < fmt.num_color_attachments; ++i)
    {
        auto img = vierkant::Image::create(device, fmt.color_attachment_format);
        color_attachments.push_back(img);

        // multisampling requested -> add resolve attachment
        if(fmt.color_attachment_format.sample_count != VK_SAMPLE_COUNT_1_BIT)
        {
            auto resolve_fmt = fmt.color_attachment_format;
            resolve_fmt.sample_count = VK_SAMPLE_COUNT_1_BIT;
            resolve_fmt.extent = fmt.size;
            auto resolve_img = vierkant::Image::create(device, resolve_fmt);
            resolve_attachments.push_back(resolve_img);
        }
    }

    // depth/stencil attachment
    if(fmt.depth || fmt.stencil)
    {
        auto depth_img = vierkant::Image::create(device, fmt.depth_attachment_format);
        depth_stencil_attachments.push_back(depth_img);
    }

    // if we were provided a queue, submit + sync
    if(cmd_buf) { cmd_buf.submit(fmt.queue, true); }

    attachment_map_t attachments;
    if(!color_attachments.empty()) { attachments[AttachmentType::Color] = std::move(color_attachments); }
    if(!depth_stencil_attachments.empty())
    {
        attachments[AttachmentType::DepthStencil] = std::move(depth_stencil_attachments);
    }
    if(!resolve_attachments.empty()) { attachments[AttachmentType::Resolve] = std::move(resolve_attachments); }
    return attachments;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::ImagePtr Framebuffer::color_attachment(uint32_t index) const
{
    // check for resolve-attachment, fallback to color-attachment
    auto it = m_attachments.find(AttachmentType::Resolve);
    if(it == m_attachments.end()) { it = m_attachments.find(AttachmentType::Color); }

    if(it != m_attachments.end())
    {
        auto &color_attachments = it->second;

        if(index >= color_attachments.size())
        {
            throw std::out_of_range("attachment-index out of bounds: " + std::to_string(index));
        }
        return color_attachments[index];
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

        if(!depth_attachments.empty()) { return depth_attachments.front(); }
    }
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void swap(Framebuffer &lhs, Framebuffer &rhs) noexcept
{
    std::swap(lhs.clear_depth_stencil, rhs.clear_depth_stencil);
    std::swap(lhs.clear_color, rhs.clear_color);
    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_extent, rhs.m_extent);
    std::swap(lhs.m_attachments, rhs.m_attachments);
    std::swap(lhs.m_fence, rhs.m_fence);
    std::swap(lhs.m_command_pool, rhs.m_command_pool);
    std::swap(lhs.m_commandbuffer, rhs.m_commandbuffer);
    std::swap(lhs.m_active_commandbuffer, rhs.m_active_commandbuffer);
    std::swap(lhs.m_format, rhs.m_format);
    std::swap(lhs.m_color_attachment_formats, rhs.m_color_attachment_formats);
}

void Framebuffer::begin_rendering(VkCommandBuffer commandbuffer, const begin_rendering_info_t &info) const
{
    std::vector<VkRenderingAttachmentInfo> color_attachments;
    std::vector<VkRenderingAttachmentInfo> depth_attachments;

    bool use_color_attachment = info.use_color_attachment && check_attachment(AttachmentType::Color, m_attachments);
    bool use_depth_attachment =
            info.use_depth_attachment && check_attachment(AttachmentType::DepthStencil, m_attachments);
    bool has_resolve = check_attachment(AttachmentType::Resolve, m_attachments);

    for(const auto &[type, images]: m_attachments)
    {
        for(uint32_t i = 0; i < images.size(); ++i)
        {
            const auto &img = images[i];
            img->transition_layout(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, commandbuffer);

            if(type == AttachmentType::DepthStencil && use_depth_attachment)
            {
                const auto &depth = images.front();

                VkRenderingAttachmentInfo depth_attachment = {};
                depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depth_attachment.imageView = depth->image_view();
                depth_attachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
                depth_attachment.loadOp =
                        info.clear_depth_attachment ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
                depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                depth_attachment.imageView = depth->image_view();
                depth_attachment.clearValue.depthStencil = clear_depth_stencil;
                depth_attachments.push_back(depth_attachment);
            }
            else if(type == AttachmentType::Color && use_color_attachment)
            {
                VkRenderingAttachmentInfo color_attachment = {};
                color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                color_attachment.imageView = img->image_view();
                color_attachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
                color_attachment.loadOp =
                        info.clear_color_attachment ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
                color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                color_attachment.clearValue.color = {{clear_color.x, clear_color.y, clear_color.z, clear_color.w}};

                if(has_resolve && img->format().sample_count != VK_SAMPLE_COUNT_1_BIT)
                {
                    const auto &resolve_attachments = m_attachments.at(AttachmentType::Resolve);
                    assert(resolve_attachments.size() == images.size());
                    color_attachment.resolveImageView = resolve_attachments[i]->image_view();
                    color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
                    color_attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
                }
                color_attachments.push_back(color_attachment);
            }
        }
    }

    VkRenderingInfo pass_info = {};
    pass_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    pass_info.flags = info.flags;
    pass_info.viewMask = 0;
    pass_info.renderArea.offset = {0, 0};
    pass_info.renderArea.extent = {m_extent.width, m_extent.height};
    pass_info.layerCount = m_format.color_attachment_format.num_layers;
    pass_info.colorAttachmentCount = color_attachments.size();
    pass_info.pColorAttachments = color_attachments.empty() ? nullptr : color_attachments.data();
    pass_info.pDepthAttachment = depth_attachments.empty() ? nullptr : depth_attachments.data();

    // check actual VK_FORMAT and figure our if stencil is required
    pass_info.pStencilAttachment =
            depth_attachment() && is_stencil(depth_attachment()->format().format) ? depth_attachments.data() : nullptr;
    vkCmdBeginRendering(commandbuffer, &pass_info);
    m_direct_rendering_commandbuffer = commandbuffer;
}

void Framebuffer::end_rendering(const end_rendering_info_t &end_rendering_info) const
{
    if(m_direct_rendering_commandbuffer)
    {
        vkCmdEndRendering(m_direct_rendering_commandbuffer);

        for(auto &[type, images]: m_attachments)
        {
            for(auto &img: images)
            {
                if(type == AttachmentType::Color && end_rendering_info.final_layout_color != VK_IMAGE_LAYOUT_UNDEFINED)
                {
                    img->transition_layout(end_rendering_info.final_layout_color, m_direct_rendering_commandbuffer);
                }
                else if(type == AttachmentType::DepthStencil &&
                        end_rendering_info.final_layout_depth != VK_IMAGE_LAYOUT_UNDEFINED)
                {
                    img->transition_layout(end_rendering_info.final_layout_depth, m_direct_rendering_commandbuffer);
                }
            }
        }

        m_direct_rendering_commandbuffer = VK_NULL_HANDLE;
    }
}

}// namespace vierkant
