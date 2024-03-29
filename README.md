# vierkant

![Preview](https://crocdialer.com/wp-content/uploads/2023/03/2023_02_24_pbr_viewer_cover.png)
- vierkant is a Vulkan 1.3 based rendering library written in C++20.
- win64: ![example workflow](https://github.com/crocdialer/vierkant/actions/workflows/cmake_build_windows.yml/badge.svg) linux: ![example workflow](https://github.com/crocdialer/vierkant/actions/workflows/cmake_build.yml/badge.svg)

rendering backends
-
- gpu-driven rasterizer with compute-based frustum/occlusion-culling
- optional support for meshlet-based pipelines (using [VK_EXT_mesh_shader](https://www.khronos.org/blog/mesh-shading-for-vulkan))
- pathtracer using [VK_KHR_ray_tracing_pipeline](https://www.khronos.org/blog/vulkan-ray-tracing-final-specification-release), useful for comparing against a groundtruth

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
- https://github.com/nothings/stb (stb_truetype.h)
- https://github.com/glfw/glfw
- https://github.com/syoyo/tinygltf
- https://github.com/ocornut/imgui
- https://github.com/zeux/meshoptimizer
- https://github.com/skypjack/entt
