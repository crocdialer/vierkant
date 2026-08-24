//  MIT License
//
//  Copyright (c) 2023 Fabian Schmidt (github.com/crocdialer)
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.

#include <cxxopts.hpp>
#include <vierkant/CameraControl.hpp>
#include <vierkant/PBRDeferred.hpp>
#include <vierkant/PBRPathTracer.hpp>
#include <vierkant/cubemap_utils.hpp>
#include <vierkant/model/model_loading.hpp>

#include "pbr_thumbnailer.h"

#include <ranges>

using double_second = std::chrono::duration<double>;

void PBRThumbnailer::setup()
{
    spdlog::set_level(m_settings.log_level);

    // print vulkan/driver/vierkant-version
    spdlog::info("{}: processing model '{}' -> '{}'", name(), m_settings.model_path.string(),
                 m_settings.result_image_path.string());

    // load model in background
    auto scene_future = background_queue().post(
            [path = m_settings.model_path, &pool = background_queue()] { return load_model_file(path, pool); });

    // TODO: load optional environment-HDR

    // create required vulkan-resources
    create_graphics_context();

    auto scene_data = scene_future.get();

    // load model first — AABB is required to position the camera
    vierkant::AABB model_aabb;
    bool success = false;
    if(scene_data)
    {
        model_aabb = create_mesh(*scene_data);
        success = model_aabb.valid();
    }

    // create camera, fitting frustum to model's native AABB
    if(scene_data) { create_camera(*scene_data, model_aabb); }

    this->running = success;
    if(!success) { return_type = EXIT_FAILURE; }
}

void PBRThumbnailer::update(double /*time_delta*/)
{
    // render image
    {
        spdlog::stopwatch sw;

        uint32_t num_passes = m_settings.num_samples / m_settings.max_samples_per_frame;

        const auto render_start = std::chrono::steady_clock::now();
        const std::chrono::seconds halving_interval{5};
        auto next_halving = render_start + halving_interval;

        for(uint32_t i = 0; i < num_passes; ++i)
        {
            auto render_result =
                    m_context.scene_renderer->render_scene(m_context.renderer, m_scene, m_camera, vierkant::LAYER_ALL);
            auto cmd_buffer = m_context.renderer.render(m_context.framebuffer);
            m_context.framebuffer.submit({cmd_buffer}, m_context.device->queue(), render_result.semaphore_infos);
            m_context.framebuffer.wait_fence();

            // if rendering is slow, halve remaining passes every 5s to produce output in reasonable time
            while(std::chrono::steady_clock::now() >= next_halving)
            {
                uint32_t remaining = num_passes - (i + 1);
                num_passes -= remaining / 2;
                next_halving += halving_interval;
                spdlog::debug("slow render: {} passes remaining", num_passes - (i + 1));
            }
        }
        spdlog::info("rendering done (#spp: ~{} - {})", num_passes * m_settings.max_samples_per_frame, sw.elapsed());
    }

    {
        spdlog::stopwatch sw;

        // download result image from GPU
        vierkant::Buffer::create_info_t host_buffer_info = {};
        host_buffer_info.device = m_context.device;
        host_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        host_buffer_info.mem_usage = VMA_MEMORY_USAGE_CPU_ONLY;
        host_buffer_info.num_bytes = vierkant::num_bytes(m_context.framebuffer.color_attachment()->format().format) *
                                     m_settings.result_image_size.x * m_settings.result_image_size.y;
        auto host_buffer = vierkant::Buffer::create(host_buffer_info);
        m_context.framebuffer.color_attachment()->copy_to(host_buffer);

        // save image to disk
        auto result_img = crocore::Image_<uint8_t>::create(static_cast<uint8_t *>(host_buffer->map()),
                                                           m_settings.result_image_size.x,
                                                           m_settings.result_image_size.y, 4, true);
        crocore::save_image_to_file(result_img, m_settings.result_image_path.string());
        spdlog::info("png/jpg encoding ({})", sw.elapsed());
    }

    // done -> terminate application-loop
    this->running = false;
}

void PBRThumbnailer::teardown()
{
    m_context.device->wait_idle();

    // PBRDeferred retains Object3DPtr/SceneConstPtr in its per-frame cull_result.
    // Reset these before the scene's ObjectStore is destroyed (m_scene outlives m_context
    // in the member-destruction order), otherwise the free-list destructor would assert.
    m_context.scene_renderer.reset();
    m_camera.reset();

    spdlog::info("total: {}s", application_time());
}

std::optional<vierkant::model::model_assets_t> PBRThumbnailer::load_model_file(const std::filesystem::path &path,
                                                                               crocore::ThreadPoolClassic &pool)
{
    if(exists(path))
    {
        auto start_time = std::chrono::steady_clock::now();

        spdlog::debug("loading model '{}'", path.string());

        // tinygltf
        auto scene_assets = vierkant::model::load_model(path, &pool);

        if(!scene_assets)
        {
            spdlog::error("could not load file: {}", path.string());
            return {};
        }
        spdlog::info("loaded model: '{}' ({})", path.string(),
                     double_second(std::chrono::steady_clock::now() - start_time));
        return scene_assets;
    }
    spdlog::error("could not find file: '{}'", path.string());
    return {};
}

bool PBRThumbnailer::create_graphics_context()
{
    spdlog::stopwatch sw;

    vierkant::Instance::create_info_t instance_info = {};
    instance_info.use_validation_layers = m_settings.use_validation;
    m_context.instance = vierkant::Instance(instance_info);

    VkPhysicalDevice physical_device = m_context.instance.physical_devices().front();

    for(const auto &pd: m_context.instance.physical_devices())
    {
        VkPhysicalDeviceProperties2 device_props = vierkant::device_properties(pd);

        if(device_props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            physical_device = pd;
            break;
        }
    }
    // print vulkan-/driver-/vierkant-version
    spdlog::debug(vierkant::device_info(physical_device));

    // check raytracing-pipeline + ray-query support (path-tracer shaders require both)
    m_settings.use_pathtracer =
            m_settings.use_pathtracer &&
            vierkant::check_device_extension_support(physical_device, vierkant::RayTracer::required_extensions()) &&
            vierkant::check_device_extension_support(physical_device, {VK_KHR_RAY_QUERY_EXTENSION_NAME});

    // create device
    vierkant::Device::create_info_t device_info = {};
    device_info.use_validation = m_context.instance.use_validation_layers();
    device_info.instance = m_context.instance.handle();
    device_info.physical_device = physical_device;
    device_info.direct_function_pointers = true;

    // add the raytracing-extensions
    if(m_settings.use_pathtracer)
    {
        device_info.extensions = crocore::concat_containers<const char *>(vierkant::RayTracer::required_extensions(),
                                                                          vierkant::RayBuilder::required_extensions());
        device_info.extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        if(!vierkant::check_device_extension_support(physical_device, device_info.extensions))
        {
            spdlog::warn("using fallback rasterizer: path-tracer was requested, but required extensions are not "
                         "available {}",
                         device_info.extensions);
        }
    }
    m_context.device = vierkant::Device::create(device_info);

    // setup a scene-renderer
    if(m_settings.use_pathtracer)
    {
        vierkant::PBRPathTracer::create_info_t path_tracer_info = {};
        path_tracer_info.seed = m_settings.random_seed;
        path_tracer_info.settings.compaction = false;
        path_tracer_info.settings.resolution = m_settings.result_image_size;
        path_tracer_info.settings.max_num_batches = m_settings.num_samples / m_settings.max_samples_per_frame;
        path_tracer_info.settings.num_samples = m_settings.max_samples_per_frame;
        path_tracer_info.settings.draw_skybox = m_settings.draw_skybox;
        path_tracer_info.settings.max_trace_depth = m_settings.max_path_length;
        m_context.scene_renderer = vierkant::PBRPathTracer::create(m_context.device, path_tracer_info);
    }
    else
    {
        constexpr auto hdr_format = VK_FORMAT_R16G16B16A16_SFLOAT;// VK_FORMAT_B10G11R11_UFLOAT_PACK32

        vierkant::PBRDeferred::create_info_t pbr_render_info = {};
        pbr_render_info.settings.resolution = m_settings.result_image_size;
        pbr_render_info.settings.output_resolution = m_settings.result_image_size;
        pbr_render_info.settings.draw_skybox = m_settings.draw_skybox;
        pbr_render_info.settings.indirect_draw = false;
        pbr_render_info.settings.use_taa = false;
        pbr_render_info.settings.use_fxaa = true;
        pbr_render_info.hdr_format = hdr_format;

        constexpr uint32_t env_size = 256;
        constexpr uint32_t lambert_size = 64;
        auto env_img = vierkant::cubemap_neutral_environment(m_context.device, env_size, m_context.device->queue(),
                                                             true, hdr_format);
        pbr_render_info.conv_lambert = vierkant::create_convolution_lambert(m_context.device, env_img, lambert_size,
                                                                            hdr_format, m_context.device->queue());
        pbr_render_info.conv_ggx = vierkant::create_convolution_ggx(m_context.device, env_img, env_img->width(),
                                                                    hdr_format, m_context.device->queue());
        m_context.scene_renderer = vierkant::PBRDeferred::create(m_context.device, pbr_render_info);
    }
    vierkant::Rasterizer::create_info_t create_info = {};
    create_info.viewport.width = static_cast<float>(m_settings.result_image_size.x);
    create_info.viewport.height = static_cast<float>(m_settings.result_image_size.y);
    m_context.renderer = vierkant::Rasterizer(m_context.device, create_info);

    // create framebuffer
    vierkant::Framebuffer::create_info_t framebuffer_info = {};
    framebuffer_info.size = {m_settings.result_image_size.x, m_settings.result_image_size.y, 1};
    framebuffer_info.color_attachment_format.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    m_context.framebuffer = vierkant::Framebuffer(m_context.device, framebuffer_info);

    // clear with transparent alpha, if requested
    if(!m_settings.draw_skybox) { m_context.framebuffer.clear_color = glm::vec4(0.f); }

    spdlog::debug("graphics-context initialized: {}", sw.elapsed());
    return true;
}

vierkant::AABB PBRThumbnailer::create_mesh(const vierkant::model::model_assets_t &mesh_assets)
{
    // additionally required buffer-flags for raytracing/compute/mesh-shading
    VkBufferUsageFlags buffer_flags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    if(m_settings.use_pathtracer)
    {
        buffer_flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    }

    // create a gpu-mesh from loaded assets
    vierkant::model::load_mesh_params_t load_params = {};
    load_params.device = m_context.device;
    load_params.buffer_flags = buffer_flags;
    load_params.mesh_buffers_params.pack_vertices = true;

    // attach mesh to an object, insert into scene
    auto load_mesh_result = vierkant::model::load_mesh(load_params, mesh_assets);
    auto object = m_scene->create_mesh_object({load_mesh_result.mesh});
    assert(object->transform());

    m_scene->asset_provider()->populate(load_mesh_result);

    // keep native scale — only center the AABB at the origin
    auto local_aabb = object->aabb();
    object->set_transform({.translation = -local_aabb.center()});

    m_scene->add_object(object);
    return local_aabb;
}
void PBRThumbnailer::create_camera(const vierkant::model::model_assets_t &mesh_assets, const vierkant::AABB &model_aabb)
{
    vierkant::model::camera_t model_camera = {};

    // prefer/expose cameras included in model-files
    if(m_settings.use_model_camera && !mesh_assets.cameras.empty()) { model_camera = mesh_assets.cameras.front(); }
    else
    {
        const float aspect =
                static_cast<float>(m_settings.result_image_size.x) / static_cast<float>(m_settings.result_image_size.y);
        model_camera.params.aspect = aspect;

        // bounding-sphere radius of the (unscaled) model
        const float r = glm::length(model_aabb.half_extents());

        // tightest half-FOV angle: use the smaller of the two to guarantee the sphere fits both axes
        const float min_half_fov = std::min(model_camera.params.fovx(), model_camera.params.fovy()) * 0.5f;
        const float d = r / std::tan(min_half_fov);

        // clipping planes scaled to model size so near/far work for any scale
        model_camera.params.clipping_distances = {std::max(1e-4f, (d - r) * 0.1f), (d + r) * 10.0f};

        auto orbit_cam_controller = vierkant::OrbitCamera();
        orbit_cam_controller.spherical_coords = m_settings.cam_spherical_coords;
        orbit_cam_controller.distance = d;
        model_camera.transform = orbit_cam_controller.transform();
    }

    // create camera and add to scene
    model_camera.params.aspect =
            static_cast<float>(m_settings.result_image_size.x) / static_cast<float>(m_settings.result_image_size.y);

    vierkant::camera_component_t cam_cmp = {};
    cam_cmp.physical = model_camera.params;
    m_camera = m_scene->create_camera(cam_cmp);
    m_camera->set_transform(model_camera.transform);
}

std::optional<PBRThumbnailer::settings_t> parse_settings(int argc, char *argv[])
{
    PBRThumbnailer::settings_t ret = {};

    // available options
    cxxopts::Options options(argv[0], "3d-model thumbnailer with rasterization and path-tracer backends\n");
    options.positional_help("<model-file> [<hdr-image>] <output-image-path>");
    options.add_options()("help", "print this help message");
    options.add_options()("w,width", "result-image width in px", cxxopts::value<uint32_t>());
    options.add_options()("h,height", "result-image height in px", cxxopts::value<uint32_t>());
    options.add_options()("num_samples", "number of samples-per-pixel (spp)", cxxopts::value<uint32_t>());
    options.add_options()("max_path", "maximum path-length", cxxopts::value<uint32_t>());
    options.add_options()("seed", "supply a seed for the random-number-generator", cxxopts::value<uint32_t>());
    options.add_options()("a,angle", "camera rotation-angle in degrees", cxxopts::value<float>());
    options.add_options()("s,skybox", "render skybox");
    options.add_options()("c, camera", "prefer model-camera");
    options.add_options()("r,raster", "force fallback-rasterizer instead of path-tracing");
    options.add_options()("v,verbose", "verbose printing");
    options.add_options()("validation", "enable vulkan validation");
    options.add_options()("files", "provided input files", cxxopts::value<std::vector<std::string>>());
    options.parse_positional("files");

    cxxopts::ParseResult result;

    try
    {
        result = options.parse(argc, argv);
    } catch(std::exception &e)
    {
        spdlog::error(e.what());
        return {};
    }

    if(result.count("files"))
    {
        const auto &files = result["files"].as<std::vector<std::string>>();

        for(const auto &f: files)
        {
            auto file_path = std::filesystem::path(f);
            auto ext = crocore::to_lower(file_path.extension().string());
            bool file_exists = exists(file_path) && is_regular_file(file_path);

            if(file_exists && (ext == ".gltf" || ext == ".glb" || ext == ".obj")) { ret.model_path = file_path; }
            else if(file_exists && (ext == ".hdr")) { ret.environment_path = file_path; }
            else if(ext == ".png") { ret.result_image_path = file_path; }
        }
    }
    if(ret.model_path.empty()) { spdlog::error("no valid model-file (.gltf | .glb | .obj)"); }
    if(ret.result_image_path.empty()) { spdlog::error("no valid output-image path (.png | .jpg)"); }

    bool success = !ret.model_path.empty() && !ret.result_image_path.empty();

    // print usage
    if(!success || result.count("help"))
    {
        spdlog::set_pattern("%v");
        spdlog::info("\n{}", options.help());
        return {};
    }
    if(result.count("width")) { ret.result_image_size.x = result["width"].as<uint32_t>(); }
    if(result.count("height")) { ret.result_image_size.y = result["height"].as<uint32_t>(); }
    if(result.count("num_samples")) { ret.num_samples = result["num_samples"].as<uint32_t>(); }
    if(result.count("max_path")) { ret.max_path_length = result["max_path"].as<uint32_t>(); }
    if(result.count("seed")) { ret.random_seed = result["seed"].as<uint32_t>(); }
    if(result.count("angle")) { ret.cam_spherical_coords.y = glm::radians(result["angle"].as<float>()); }
    if(result.count("skybox") && result["skybox"].as<bool>()) { ret.draw_skybox = true; }
    if(result.count("camera") && result["camera"].as<bool>()) { ret.use_model_camera = true; }
    if(result.count("raster") && result["raster"].as<bool>()) { ret.use_pathtracer = false; }
    if(result.count("validation") && result["validation"].as<bool>()) { ret.use_validation = true; }
    if(result.count("verbose") && result["verbose"].as<bool>()) { ret.log_level = spdlog::level::info; }
    return ret;
}

int main(int argc, char *argv[])
{
    if(auto settings = parse_settings(argc, argv))
    {
        crocore::Application::create_info_t create_info = {};
        create_info.arguments = {argv, argv + argc};
        create_info.num_background_threads = std::min<uint32_t>(4, std::thread::hardware_concurrency());
        auto app = PBRThumbnailer(create_info, *settings);
        return app.run();
    }
    else
    {
        return EXIT_FAILURE;
    }
}
