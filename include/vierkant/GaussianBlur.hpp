//
// Created by crocdialer on 10/17/20.
//

#pragma once

#include "vierkant/Renderer.hpp"

namespace vierkant
{

//! gaussian_blur_t models the ubo-layout for providing offsets and weights.
template<uint32_t Size = 3>
struct gaussian_blur_t
{
    //! weighted offsets. array of floats, encoded as vec4
    glm::vec4 offsets[Size]{};

    //! distribution weights. array of floats, encoded as vec4
    glm::vec4 weights[Size]{};
};

class GaussianBlur
{
public:

    explicit GaussianBlur(uint32_t num_taps = 9, const glm::vec2 &sigma = glm::vec2(0));

    vierkant::ImagePtr apply(const vierkant::ImagePtr &image);

private:

    //! ping-pong post-fx framebuffers
    struct asset_t
    {
        vierkant::Framebuffer framebuffer;
        vierkant::Renderer renderer;
    };
    std::array<asset_t, 2> m_assets;

};

}// namespace vierkant

