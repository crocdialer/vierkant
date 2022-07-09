# vierkant

![Preview](https://crocdialer.com/wp-content/uploads/2021/09/whisky_dragon.jpg)
- vierkant is a Vulkan 1.2 based rendering library written in C++17.
- ![example workflow](https://github.com/crocdialer/vierkant/actions/workflows/cmake_build.yml/badge.svg)

rendering backends
-
- deferred rasterizer
- pathtracer (Disney-bsdf) using VK_KHR_ray* extensions

HDR
-
- load .hdr panoramas as mip-mapped cubemaps
- utils for lambert- and GGX-convolutions used by rasterizer

model-loading
-
- glTF 2.0 via https://github.com/syoyo/tinygltf
- optional/additional: https://github.com/assimp/assimp

UI
-
- pragmatic+easy interface, thanks https://github.com/ocornut/imgui

submodules:
-
- https://github.com/crocdialer/crocore
- https://github.com/gabime/spdlog
- https://github.com/nothings/stb (stb_truetype.h)
- https://github.com/glfw/glfw
- https://github.com/syoyo/tinygltf
- https://github.com/ocornut/imgui
- https://github.com/zeux/meshoptimizer
- vulkan 1.2 + support for VK_KHR_ray*
