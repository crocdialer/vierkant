//
// Created by crocdialer on 28.09.23.
//

#pragma once

#include <vierkant/math.hpp>
#include <volk.h>

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
static inline void begin_label(VkCommandBuffer commandbuffer, const debug_label_t &label)
{
    if(vkCmdBeginDebugUtilsLabelEXT && !label.text.empty())
    {
        VkDebugUtilsLabelEXT debug_label = {};
        debug_label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        *reinterpret_cast<glm::vec4 *>(debug_label.color) = label.color;
        debug_label.pLabelName = label.text.c_str();
        vkCmdBeginDebugUtilsLabelEXT(commandbuffer, &debug_label);
    }
}

/**
 * @brief   'begin_label' can be used to mark the start of a labeled section within a queue.
 *
 * @param   queue   a provided queue
 * @param   label   a debug-label object.
 */
static inline void begin_label(VkQueue queue, const vierkant::debug_label_t &label)
{
    if(vkCmdBeginDebugUtilsLabelEXT && !label.text.empty())
    {
        VkDebugUtilsLabelEXT debug_label = {};
        debug_label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        *reinterpret_cast<glm::vec4 *>(debug_label.color) = label.color;
        debug_label.pLabelName = label.text.c_str();
        vkQueueBeginDebugUtilsLabelEXT(queue, &debug_label);
    }
}

/**
 * @brief   'end_label' needs to be used after previous calls to 'begin_label',
 *          to mark the end of a labeled section within a queue.
 *
 * @param   queue   a provided queue.
 */
static inline void end_label(VkQueue queue)
{
    if(vkQueueEndDebugUtilsLabelEXT) { vkQueueEndDebugUtilsLabelEXT(queue); }
}

/**
 * @brief   'end_label' needs to be used after previous calls to 'begin_label',
 *          to mark the end of a labeled section within a commandbuffer.
 *
 * @param   commandbuffer   a provided commandbuffer.
 */
static inline void end_label(VkCommandBuffer commandbuffer)
{
    if(vkCmdEndDebugUtilsLabelEXT) { vkCmdEndDebugUtilsLabelEXT(commandbuffer); }
}

/**
 * @brief   insert_label can be used to insert a singular label into a commandbuffer.
 *
 * @param   commandbuffer   a provided commandbuffer
 * @param   label           a debug-label object.
 */
static inline void insert_label(VkCommandBuffer commandbuffer, const vierkant::debug_label_t &label)
{
    if(vkCmdInsertDebugUtilsLabelEXT && !label.text.empty())
    {
        VkDebugUtilsLabelEXT debug_label = {};
        debug_label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        *reinterpret_cast<glm::vec4 *>(debug_label.color) = label.color;
        debug_label.pLabelName = label.text.c_str();
        vkCmdInsertDebugUtilsLabelEXT(commandbuffer, &debug_label);
    }
}

/**
 * @brief   insert_label can be used to insert a singular label into a queue.
 *
 * @param   queue   a provided queue
 * @param   label   a debug-label object.
 */
static inline void insert_label(VkQueue queue, const debug_label_t &label)
{
    if(vkQueueInsertDebugUtilsLabelEXT && !label.text.empty())
    {
        VkDebugUtilsLabelEXT debug_label = {};
        debug_label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        *reinterpret_cast<glm::vec4 *>(debug_label.color) = label.color;
        debug_label.pLabelName = label.text.c_str();
        vkQueueInsertDebugUtilsLabelEXT(queue, &debug_label);
    }
}

}
