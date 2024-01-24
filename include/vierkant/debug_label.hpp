//
// Created by crocdialer on 28.09.23.
//

#pragma once

#include <vierkant/Instance.hpp>
#include <vierkant/math.hpp>

namespace vierkant
{

//! debug_label_t is a simple struct grouping information for debug-labels
struct debug_label_t
{
    //! a descriptive text
    std::string text;

    //! desired color-value
    glm::vec4 color = {0.6f, 0.6f, 0.6f, 1.f};
};

/**
 * @brief   'begin_label' can be used to mark the start of a labeled section within a commandbuffer.
 *
 * @param   commandbuffer   a provided commandbuffer
 * @param   label           a debug-label object.
 */
void begin_label(VkCommandBuffer commandbuffer, const debug_label_t &label);

/**
 * @brief   'begin_label' can be used to mark the start of a labeled section within a queue.
 *
 * @param   queue   a provided queue
 * @param   label   a debug-label object.
 */
void begin_label(VkQueue queue, const vierkant::debug_label_t &label);

/**
 * @brief   'end_label' needs to be used after previous calls to 'begin_label',
 *          to mark the end of a labeled section within a queue.
 *
 * @param   queue   a provided queue.
 */
void end_label(VkQueue queue);

/**
 * @brief   'end_label' needs to be used after previous calls to 'begin_label',
 *          to mark the end of a labeled section within a commandbuffer.
 *
 * @param   commandbuffer   a provided commandbuffer.
 */
void end_label(VkCommandBuffer commandbuffer);

/**
 * @brief   insert_label can be used to insert a singular label into a commandbuffer.
 *
 * @param   commandbuffer   a provided commandbuffer
 * @param   label           a debug-label object.
 */
void insert_label(VkCommandBuffer commandbuffer, const vierkant::debug_label_t &label);

/**
 * @brief   insert_label can be used to insert a singular label into a queue.
 *
 * @param   queue   a provided queue
 * @param   label   a debug-label object.
 */
void insert_label(VkQueue queue, const debug_label_t &label);

}
