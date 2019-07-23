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
#include "vierkant/Font.hpp"
#include "vierkant/Renderer.hpp"
#include "vierkant/Application.hpp"
#include "vierkant/intersection.hpp"

namespace vierkant {

void draw_text(vierkant::Renderer &renderer, const FontPtr &font);

//void draw_mesh(vierkant::Renderer &renderer, const MeshPtr &mesh);

}
namespace vk = vierkant;