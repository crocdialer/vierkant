//
// Created by crocdialer on 3/2/19.
//

#pragma once

#include "Instance.hpp"
#include "Device.hpp"
#include "Buffer.hpp"
#include "Image.hpp"
#include "Framebuffer.hpp"
#include "Pipeline.hpp"
#include "Mesh.hpp"
#include "SwapChain.hpp"
#include "Window.hpp"
#include "geometry.hpp"

namespace vierkant
{
    void draw_mesh(VkCommandBuffer command_buffer, const Mesh &mesh);
}
namespace vk = vierkant;