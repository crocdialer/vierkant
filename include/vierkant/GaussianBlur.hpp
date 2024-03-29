//
// Created by crocdialer on 10/17/20.
//

#pragma once

#include "vierkant/ImageEffect.hpp"
#include "vierkant/Rasterizer.hpp"

namespace vierkant
{

template<uint32_t NUM_TAPS = 9>
class GaussianBlur_ : public ImageEffect
{
public:
    static_assert(NUM_TAPS % 2, "gaussian kernel-size must be odd");

    struct create_info_t
    {
        //! framebuffer size
        VkExtent3D size = {};

        //! framebuffer color-format
        VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM;

        //! number of iterations
        uint32_t num_iterations = 1;

        //! optional sigma, if zero(default) will be deduced from kernel size (num taps)
        glm::vec2 sigma = glm::vec2(0);

        //! optional pipeline-cache to share shaders and piplines
        vierkant::PipelineCachePtr pipeline_cache = nullptr;
        vierkant::DescriptorPoolPtr descriptor_pool = nullptr;
        vierkant::CommandPoolPtr command_pool = nullptr;
    };

    ~GaussianBlur_() override = default;

    static std::unique_ptr<GaussianBlur_> create(const DevicePtr &device, const create_info_t &create_info);

    vierkant::ImagePtr apply(const vierkant::ImagePtr &image, VkQueue queue,
                             const std::vector<vierkant::semaphore_submit_info_t> &semaphore_infos) override;

    vierkant::ImagePtr apply(const vierkant::ImagePtr &image, VkCommandBuffer commandbuffer) override;

private:
    //! used as data for specialization constant
    static constexpr uint32_t num_taps = NUM_TAPS;

    static constexpr uint32_t max_ubo_array_size = 4;

    //! ubo_t models the ubo-layout for providing offsets and weights.
    struct ubo_t
    {
        //! weighted offsets. array of floats, encoded as vec4
        glm::vec4 offsets[max_ubo_array_size]{};

        //! distribution weights. array of floats, encoded as vec4
        glm::vec4 weights[max_ubo_array_size]{};

        //! output-size used to derive texel-size
        glm::vec2 size{};
    };

    GaussianBlur_(const DevicePtr &device, const create_info_t &create_info);

    //! ping-pong post-fx framebuffers
    struct ping_pong_t
    {
        vierkant::Framebuffer framebuffer;
        vierkant::BufferPtr ubo;
        vierkant::drawable_t drawable;
    };
    vierkant::DevicePtr m_device;
    std::array<ping_pong_t, 2> m_ping_pongs;
    vierkant::Rasterizer m_renderer;

    uint32_t m_num_iterations = 0;
    VkFormat m_color_format = VK_FORMAT_UNDEFINED;
    vierkant::CommandBuffer m_command_buffer;
};

extern template class GaussianBlur_<5>;

extern template class GaussianBlur_<7>;

extern template class GaussianBlur_<9>;

extern template class GaussianBlur_<11>;

extern template class GaussianBlur_<13>;

template<uint32_t NUM_TAPS>
using GaussianBlurUPtr_ = std::unique_ptr<GaussianBlur_<NUM_TAPS>>;
template<uint32_t NUM_TAPS>
using GaussianBlurPtr_ = std::shared_ptr<GaussianBlur_<NUM_TAPS>>;

using GaussianBlur = GaussianBlur_<9>;
using GaussianBlurPtr = std::shared_ptr<GaussianBlur>;

}// namespace vierkant
