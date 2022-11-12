# vierkant

![Preview](https://crocdialer.com/wp-content/uploads/2022/10/2022-09-30-chessboard.jpg)
- vierkant is a Vulkan 1.3 based rendering library written in C++20.
- ![example workflow](https://github.com/crocdialer/vierkant/actions/workflows/cmake_build.yml/badge.svg)

rendering backends
-
- gpu-driven rasterizer with compute-based frustum/occlusion-culling
- optional support for meshlet-based pipelines (using VK_NV_mesh_shader)
- pathtracer using VK_KHR_ray* extensions, useful for comparing against a groundtruth

features
-
- load .hdr panoramas as mip-mapped cubemaps
- utils for lambert- and GGX-convolutions used by rasterizer
- glTF 2.0 via [tinygltf](https://github.com/syoyo/tinygltf)
  - supports the entire feature-set and all existing glTF2-extensions (transmittance, volumes, irridescence, ...) 
- optional/additional support for: [assimp](https://github.com/assimp/assimp)
- pragmatic+easy interface, thanks [imgui](https://github.com/ocornut/imgui)
- entity-component-system (registry/entity) provided by [entt](https://github.com/skypjack/entt)

submodules:
- 
- https://github.com/crocdialer/crocore
- https://github.com/gabime/spdlog
- https://github.com/nothings/stb (stb_truetype.h)
- https://github.com/glfw/glfw
- https://github.com/syoyo/tinygltf
- https://github.com/ocornut/imgui
- https://github.com/zeux/meshoptimizer
- https://github.com/skypjack/entt
