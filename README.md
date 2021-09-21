# vierkant
vierkant is a Vulkan based rendering library written in C++17.

# rendering backends
- unlit-forward
- pbr / deferred
- pbr-pathtracer (~UE4-bsdf) using VK_KHR_ray* extensions

# HDR
- load .hdr panoramas as mip-mapped cubemaps
- utils for lambert- and GGX-convolutions used by rasterizer

# UI
- pragmatic+easy interface, thanks https://github.com/ocornut/imgui

# model-loading
- glTF 2.0 via https://github.com/syoyo/tinygltf
- optional/additional: https://github.com/assimp/assimp

dependencies:
-
- https://github.com/crocdialer/crocore
- glfw
- vulkan
