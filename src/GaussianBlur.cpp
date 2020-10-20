//
// Created by crocdialer on 10/17/20.
//

#include "crocore/gaussian.hpp"
#include "vierkant/GaussianBlur.hpp"

namespace vierkant
{

template
class GaussianBlur_<5>;

template
class GaussianBlur_<7>;

template
class GaussianBlur_<9>;

template<uint32_t NUM_TAPS>
std::unique_ptr<GaussianBlur_<NUM_TAPS>>
GaussianBlur_<NUM_TAPS>::create(DevicePtr device, const create_info_t &create_info)
{
    return std::unique_ptr<GaussianBlur_<NUM_TAPS>>(new GaussianBlur_<NUM_TAPS>(std::move(device), create_info));
}

template<uint32_t NUM_TAPS>
GaussianBlur_<NUM_TAPS>::GaussianBlur_(DevicePtr device, const create_info_t &create_info)
{
    // create renderer for g-buffer-pass
    vierkant::Renderer::create_info_t post_render_info = {};
    post_render_info.num_frames_in_flight = 2;
    post_render_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    post_render_info.viewport.width = create_info.size.width;
    post_render_info.viewport.height = create_info.size.height;
    post_render_info.viewport.maxDepth = 1;
    post_render_info.pipeline_cache = create_info.pipeline_cache;
    m_renderer = vierkant::Renderer(device, post_render_info);

    // symmetric 1D gaussian kernels
    auto gaussian_x = crocore::createGaussianKernel_1D<NUM_TAPS>(create_info.sigma.x);
    auto gaussian_y = crocore::createGaussianKernel_1D<NUM_TAPS>(create_info.sigma.y);
}

template<uint32_t NUM_TAPS>
vierkant::ImagePtr GaussianBlur_<NUM_TAPS>::apply(const ImagePtr &image)
{
    auto &ping = m_framebuffers[0], &pong = m_framebuffers[1];

    return pong.color_attachment();
}

}// namespace vierkant