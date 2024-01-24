#include <vierkant/debug_label.hpp>

namespace vierkant
{

void begin_label(VkCommandBuffer commandbuffer, const debug_label_t &label)
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

void begin_label(VkQueue queue, const vierkant::debug_label_t &label)
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

void end_label(VkQueue queue)
{
    if(vkQueueEndDebugUtilsLabelEXT) { vkQueueEndDebugUtilsLabelEXT(queue); }
}

void end_label(VkCommandBuffer commandbuffer)
{
    if(vkCmdEndDebugUtilsLabelEXT) { vkCmdEndDebugUtilsLabelEXT(commandbuffer); }
}

void insert_label(VkCommandBuffer commandbuffer, const vierkant::debug_label_t &label)
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

void insert_label(VkQueue queue, const debug_label_t &label)
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

}// namespace vierkant