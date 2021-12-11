//
// Created by crocdialer on 12/11/21.
//

#pragma once

#include "vierkant/math.hpp"

namespace vierkant
{

/**
 *    A  +-------------+  B
 *      /               \
 *     /                 \
 *    /                   \
 * D +-------------------- +  C
 */
glm::mat4 compute_homography(const glm::vec2 *src, const glm::vec2 *dst);

}// namespace vierkant
