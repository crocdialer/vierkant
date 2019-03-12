//
// Created by crocdialer on 3/2/19.
//

#pragma once

#include "vierkant/Instance.hpp"
#include "vierkant/Device.hpp"
#include "vierkant/Buffer.hpp"
#include "vierkant/Image.hpp"
#include "vierkant/Framebuffer.hpp"
#include "vierkant/Pipeline.hpp"
#include "vierkant/Mesh.hpp"
#include "vierkant/SwapChain.hpp"
#include "vierkant/Window.hpp"
#include "vierkant/Geometry.hpp"
#include "vierkant/intersection.hpp"
#include "vierkant/shaders.hpp"

namespace vierkant
{
    void draw_mesh(VkCommandBuffer command_buffer, const Mesh &mesh);
}
namespace vk = vierkant;