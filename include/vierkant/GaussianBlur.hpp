//
// Created by crocdialer on 10/17/20.
//

#pragma once

#include "vierkant/Renderer.hpp"

namespace vierkant
{

template<uint32_t NUM_TAPS = 9>
class GaussianBlur_
{
public:

    static_assert(NUM_TAPS % 2, "gaussian kernel-size must be odd");

    struct create_info_t
    {
        VkExtent3D size = {};
        VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM;
        vierkant::PipelineCachePtr pipeline_cache = nullptr;
        glm::vec2 sigma = glm::vec2(0);
    };

    static std::unique_ptr<GaussianBlur_> create(const DevicePtr &device, const create_info_t &create_info);

    vierkant::ImagePtr apply(const vierkant::ImagePtr &image);

private:

    static constexpr uint32_t ubo_array_size = NUM_TAPS / 4 + 1;

    //! ubo_t models the ubo-layout for providing offsets and weights.
    struct ubo_t
    {
        //! weighted offsets. array of floats, encoded as vec4
        glm::vec4 offsets[ubo_array_size]{};

        //! distribution weights. array of floats, encoded as vec4
        glm::vec4 weights[ubo_array_size]{};
    };

    GaussianBlur_(const DevicePtr &device, const create_info_t &create_info);

    //! ping-pong post-fx framebuffers
    struct ping_pong_t
    {
        vierkant::Framebuffer framebuffer;
        vierkant::BufferPtr ubo;
        vierkant::Renderer::drawable_t drawable;
    };
    std::array<ping_pong_t, 2> m_ping_pongs;

    vierkant::Renderer m_renderer;

    VkSpecializationInfo m_specialization_info = {};
    VkSpecializationMapEntry m_specialization_entry = {};
};

template<uint32_t NUM_TAPS> using GaussianBlurUPtr_ = std::unique_ptr<GaussianBlur_<NUM_TAPS>>;
template<uint32_t NUM_TAPS> using GaussianBlurPtr_ = std::shared_ptr<GaussianBlur_<NUM_TAPS>>;

extern template
class GaussianBlur_<5>;

using GaussianBlur_5_Tap = GaussianBlur_<5>;

extern template
class GaussianBlur_<7>;

using GaussianBlur_7_Tap = GaussianBlur_<7>;

extern template
class GaussianBlur_<9>;

using GaussianBlur_9_Tap = GaussianBlur_<9>;

}// namespace vierkant

