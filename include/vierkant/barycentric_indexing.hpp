#pragma once

#include "math.hpp"

namespace vierkant
{

/**
 * @brief   num_micro_triangles returns the number of micro-triangles for a given triangle subdivision-level
 *
 * @param   num_levels  number of subdivision-levels
 * @return  number of micro-triangles
 */
static constexpr inline uint32_t num_micro_triangles(uint32_t num_levels) { return 1 << (num_levels << 1u); }

/**
 * @brief   compute barycentrics for a specific micro-triangle index.
 *
 * @param   index   index for a micro-triangle to retrieve barycentrics for
 * @param   level   the subdivision level
 * @param   uv0     output-barycentrics for micro-triangle vertex v0
 * @param   uv1     output-barycentrics for micro-triangle vertex v1
 * @param   uv2     output-barycentrics for micro-triangle vertex v2
 */
void index2bary(uint32_t index, uint32_t level, glm::vec2 &uv0, glm::vec2 &uv1, glm::vec2 &uv2);

/**
 * @brief   compute a micro-triangle index for provided barycentric coordinates.
 *
 * @param   uv      provided barycentric coordinates
 * @param   level   the subdivision level
 * @return  a micro-triangle index
 */
uint32_t bary2index(const glm::vec2 &uv, uint32_t level);

}// namespace vierkant
