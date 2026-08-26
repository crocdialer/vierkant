#include "vierkant_ed.hpp"
#include <crocore/filesystem.hpp>
#include <cxxopts.hpp>
#include <format>
#include <fstream>
#include <vierkant/Visitor.hpp>
#include <vierkant/cubemap_utils.hpp>

#include "vierkant_ed_serialization.hpp"
#include <vierkant/bundle.hpp>

#include <ranges>

using double_second = std::chrono::duration<double>;

constexpr char g_cache_path[] = "cache";
constexpr char g_model_store_path[] = "models";
constexpr char g_material_store_path[] = "materials";
constexpr char g_zip_path[] = "cache.zip";
constexpr char g_file_suffix_model[] = "4km";

std::filesystem::path VierkantEd::material_bundle_path(const std::string &scene_path) const
{
    auto file_name = std::format(
            "{}.{}", crocore::filesystem::remove_extension(crocore::filesystem::get_filename_part(scene_path)),
            g_file_suffix_model);
    return m_project_root / g_cache_path / g_material_store_path / file_name;
}

void VierkantEd::establish_project_root(const std::filesystem::path &top_scene_path)
{
    // an explicit --project-root always wins and is never overridden by a scene-open.
    if(m_project_root_explicit) { return; }

    if(!top_scene_path.empty())
    {
        auto abs = std::filesystem::weakly_canonical(std::filesystem::absolute(top_scene_path));
        m_project_root = abs.parent_path();
    }
    else
    {
        m_project_root = std::filesystem::current_path();
    }
    spdlog::debug("project root: {}", m_project_root.string());
}

std::string VierkantEd::project_key(const std::filesystem::path &p) const
{
    if(p.empty()) { return {}; }

    auto abs = std::filesystem::weakly_canonical(std::filesystem::absolute(p));

    // lexically under the root? -> portable, root-relative key. else keep absolute + warn.
    auto rel = abs.lexically_relative(m_project_root);
    if(!rel.empty() && *rel.begin() != "..") { return rel.generic_string(); }

    spdlog::warn("asset outside project-root, not portable: {}", abs.string());
    return abs.generic_string();
}

std::filesystem::path VierkantEd::resolve(const std::string &key) const
{
    std::filesystem::path k(key);
    return k.is_absolute() ? k : (m_project_root / k);
}

void VierkantEd::add_to_recent_files(const std::filesystem::path &f)
{
    main_queue().post([this, f] {
        m_settings.recent_files.push_back(f.string());
        while(m_settings.recent_files.size() > 10) { m_settings.recent_files.pop_front(); }
    });
};

void VierkantEd::load_model(const load_model_params_t &params)
{
    vierkant::MeshPtr mesh;

    auto load_task = [this, params]() {
        m_num_loading++;
        auto start_time = std::chrono::steady_clock::now();
        auto load_mesh_result = load_mesh(params.path);
        bool success = static_cast<bool>(load_mesh_result.mesh);

        auto done_cb = [this, load_mesh_result = std::move(load_mesh_result), start_time, params]() {
            m_selected_objects.clear();

            vierkant::Object3DPtr object;
            auto mesh = load_mesh_result.mesh;

            if(params.mesh_library)
            {
                object = m_object_store->create_object();

                // iterate mesh-entries, create sub-objects
                vierkant::mesh_component_t mesh_component = {mesh};

                // set library flag
                mesh_component.library = true;
                using filter_key_t = std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>;
                std::set<filter_key_t> duplicate_filter;

                for(uint32_t i = 0; i < mesh->entries.size(); ++i)
                {
                    auto &mesh_entry = mesh->entries[i];

                    if(params.mesh_library_no_dups)
                    {
                        filter_key_t key = {mesh_entry.material_index, mesh_entry.vertex_offset,
                                            mesh_entry.num_vertices, mesh_entry.lods.front().base_index,
                                            mesh_entry.lods.front().num_indices};
                        if(duplicate_filter.contains(key)) { continue; }
                        duplicate_filter.insert(key);
                    }

                    mesh_component.entry_indices = {i};
                    auto entry_obj = m_scene->create_mesh_object(mesh_component);

                    // inherit name and transform from entry
                    entry_obj->name = mesh_entry.name;
                    entry_obj->set_transform(mesh_entry.transform);

                    // add as child-object
                    object->add_child(entry_obj);
                }
            }
            else
            {
                object = m_scene->create_mesh_object({mesh});
            }

            object->name = std::filesystem::path(params.path).filename().string();

            // create child-objects for placed lightsource-instances (assets already registered via populate)
            for(const auto &li: load_mesh_result.light_instances)
            {
                auto light_obj = m_scene->create_object();
                const auto *light_asset = m_scene->asset_provider()->light(li.light_id);
                light_obj->name = light_asset && !light_asset->name.empty() ? light_asset->name : "light";
                light_obj->set_transform(li.transform);
                light_obj->add_component<vierkant::lightsource_component_t>({li.light_id});
                object->add_child(light_obj);
            }

            if(params.normalize_size)
            {
                vierkant::transform_t transform = {};
                // scale
                transform.scale = glm::vec3(5.f / glm::length(object->aabb().half_extents()));

                // center aabb
                auto aabb = object->aabb().transform(transform);
                transform.translation = -aabb.center() + glm::vec3(0.f, aabb.height() / 2.f, 3.f);
                object->set_transform(transform);
            }

            if(params.clear_scene)
            {
                // a scene-camera does not survive the clear, keep rendering through the editor-camera
                m_render_camera = m_editor_camera;
                m_scene->clear();
            }
            m_scene->add_object(object);
            if(m_path_tracer) { m_path_tracer->reset_accumulator(); }

            auto dur = double_second(std::chrono::steady_clock::now() - start_time);
            spdlog::debug("loaded '{}' -- ({:03.2f})", params.path.string(), dur.count());
            --m_num_loading;
        };
        if(success) { main_queue().post(done_cb); }
        else
        {
            spdlog::warn("could not load model: {}", params.path.string());
            --m_num_loading;
        }
    };
    background_queue().post(load_task);
}

void VierkantEd::load_texture(const std::string &path)
{
    auto load_img_fn = [this, path] {
        spdlog::debug("load image: {}", path);
        auto img = crocore::create_image_from_file(resolve(path).string(), m_settings.texture_compression ? 0 : 4);
        vierkant::TextureId texture_id;

        // TODO: check when/if this makes sense
        m_material_data.textures[texture_id] = img;

        vierkant::ImagePtr texture;
        vierkant::Image::Format fmt;
        fmt.sampler_state.max_anisotropy = m_device->properties().core.limits.maxSamplerAnisotropy;

        if(m_settings.texture_compression)
        {
            vierkant::bcn::compress_info_t compress_info = {};
            compress_info.image = img;
            compress_info.mode = vierkant::bcn::BC7;
            compress_info.generate_mipmaps = true;
            compress_info.delegate_fn = [this](const auto &fn) { return background_queue().post(fn); };
            auto compressed_img = vierkant::bcn::compress(compress_info);
            texture = vierkant::model::create_compressed_texture(m_device, compressed_img, fmt, m_queue_image_loading);

            // TODO: check when/if this makes sense
            m_material_data.textures[texture_id] = std::move(compressed_img);
        }
        else
        {
            texture = vierkant::model::create_texture(m_device, img, fmt, m_queue_image_loading);
        }

        // store gpu-texture
        m_scene->asset_provider()->add_texture({texture_id, vierkant::SamplerId::nil()}, texture);
    };
    background_queue().post(load_img_fn);
}

void VierkantEd::load_environment(const std::string &path)
{
    auto load_task = [&, path]() {
        ++m_num_loading;

        auto start_time = std::chrono::steady_clock::now();

        vierkant::ImagePtr skybox, conv_lambert, conv_ggx;
        auto img = crocore::create_image_from_file(resolve(path).string(), 4);

        if(img)
        {
            // acquire lock for image-queue // TODO: a bit more fine-grained!?
            auto lock = std::lock_guard(*m_device->queue_asset(m_queue_image_loading)->mutex);

            bool use_float = (img->num_bytes() / (img->width() * img->height() * img->num_components())) > 1;

            // command pool for background transfer
            auto command_pool = vierkant::create_command_pool(m_device, vierkant::Device::Queue::GRAPHICS,
                                                              VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

            {
                vierkant::ImagePtr panorama;
                auto cmd_buf = vierkant::CommandBuffer(m_device, command_pool.get());
                cmd_buf.begin();

                vierkant::Image::Format fmt = {};
                fmt.extent = {img->width(), img->height(), 1};
                fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                fmt.format = use_float ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
                fmt.initial_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                fmt.initial_cmd_buffer = cmd_buf.handle();
                panorama = vierkant::Image::create(m_device, nullptr, fmt);

                auto buf = vierkant::Buffer::create(m_device, img->data(), img->num_bytes(),
                                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

                // copy and layout transition
                panorama->copy_from(buf, cmd_buf.handle());
                panorama->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, cmd_buf.handle());

                // submit and sync
                {
                    cmd_buf.submit(m_queue_image_loading);

                    // derive sane resolution for cube from panorama-width
                    uint32_t res = crocore::next_pow_2(std::max(img->width(), img->height()) / 4);
                    skybox = vierkant::cubemap_from_panorama(m_device, panorama, m_queue_image_loading, res, true,
                                                             m_hdr_format);
                }
            }

            if(skybox)
            {
                constexpr uint32_t lambert_size = 128;
                conv_lambert = vierkant::create_convolution_lambert(m_device, skybox, lambert_size, m_hdr_format,
                                                                    m_queue_image_loading);
                conv_ggx = vierkant::create_convolution_ggx(m_device, skybox, skybox->width(), m_hdr_format,
                                                            m_queue_image_loading);

                auto cmd_buf = vierkant::CommandBuffer(m_device, command_pool.get());
                cmd_buf.begin();

                conv_lambert->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, cmd_buf.handle());
                conv_ggx->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, cmd_buf.handle());

                // submit and sync
                cmd_buf.submit(m_queue_image_loading, true);
            }
        }

        main_queue().post([this, path, skybox, conv_lambert, conv_ggx, start_time]() {
            m_scene->set_environment(skybox);

            m_pbr_renderer->set_environment(conv_lambert, conv_ggx);

            if(m_path_tracer) { m_path_tracer->reset_accumulator(); }

            m_scene_data.environment_path = project_key(path);
            auto dur = double_second(std::chrono::steady_clock::now() - start_time);
            spdlog::debug("loaded '{}' -- ({:03.2f})", path, dur.count());
            --m_num_loading;
        });
    };
    background_queue().post(load_task);
}

void VierkantEd::save_settings(VierkantEd::settings_t settings, const std::filesystem::path &path) const
{
    // window settings
    vierkant::Window::create_info_t window_info = {};
    window_info.size = m_window->size();
    window_info.position = m_window->position();
    window_info.fullscreen = m_window->fullscreen();
    window_info.sample_count = m_window->swapchain().sample_count();
    window_info.title = m_window->title();
    window_info.vsync = m_window->swapchain().v_sync();
    window_info.use_hdr = m_window->swapchain().hdr();
    settings.window_info = window_info;

    // logger settings
    settings.log_level = spdlog::get_level();

    // target-fps
    settings.target_fps = static_cast<float>(target_loop_frequency);

    // camera-control settings
    settings.camera_control =
            m_camera_control.current == m_camera_control.fly ? CameraControlMode::Fly : CameraControlMode::Orbit;
    settings.orbit_camera = m_camera_control.orbit;
    settings.fly_camera = m_camera_control.fly;

    const auto *cam_cmp = m_editor_camera->get_component_ptr<vierkant::camera_component_t>();
    settings.ortho_camera = cam_cmp && cam_cmp->projection == vierkant::camera_component_t::ORTHO;

    // renderer settings
    settings.pbr_settings = m_pbr_renderer->settings;
    if(m_path_tracer) { settings.path_tracer_settings = m_path_tracer->settings; }
    settings.path_tracing = m_scene_renderer == m_path_tracer;

    std::ofstream ofs(path.string());
    try
    {
        vierkant_ed::save_settings(ofs, settings);
    } catch(std::exception &e) { spdlog::error(e.what()); }

    spdlog::debug("save settings: {}", path.string());
}

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=109561 -> hitting a GCC 12 bug
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
std::optional<VierkantEd::settings_t> VierkantEd::load_settings(const std::filesystem::path &path)
{
    // create and open a character archive for input
    std::ifstream file_stream(path.string());

    if(file_stream.is_open())
    {
        spdlog::debug("loading settings: {}", path.string());
        return vierkant_ed::load_settings(file_stream);
    }
    return {};
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

void VierkantEd::load_file(const std::string &path, bool clear)
{
    switch(crocore::filesystem::get_file_type(path))
    {
        case crocore::filesystem::FileType::IMAGE:
            add_to_recent_files(path);

            if(crocore::filesystem::get_extension(path) == ".hdr") { load_environment(path); }
            else
            {
                load_texture(path);
            }
            break;

        case crocore::filesystem::FileType::MODEL:
        {
            add_to_recent_files(path);
            load_model_params_t load_params = {project_key(path)};
            load_params.clear_scene = clear;
            load_model(load_params);
            break;
        }

        case crocore::filesystem::FileType::OTHER:
            if(std::filesystem::path(path).extension() == ".json")
            {
                if(auto loaded_scene = load_scene_data(path))
                {
                    if(clear)
                    {
                        m_render_camera = m_editor_camera;
                        m_scene->clear();
                    }
                    add_to_recent_files(path);
                    vierkant::SceneId scene_id;
                    m_scene_paths[scene_id] = project_key(path);
                    build_scene(loaded_scene, clear, scene_id);
                }
            }
            break;
        default: break;
    }
}

void VierkantEd::save_scene(std::filesystem::path path)
{
    // handle empty path: fall back to the current scene-key, resolved to an openable path.
    if(path.empty())
    {
        if(auto it = m_scene_paths.find(m_scene_id); it != m_scene_paths.end()) { path = resolve(it->second.string()); }
        else
        {
            spdlog::warn("{}: unable to figure out save-path", __func__);
            return;
        }
    }
    spdlog::debug("save scene: {}", path.string());
    m_scene_paths[m_scene_id] = project_key(path);

    // scene traversal
    scene_data_t data;
    data.name = m_scene->root()->name;
    data.environment_path = m_scene_data.environment_path;
    data.environment_factor = m_scene->environment_factor;

    // derived (texture-)bundle savepath under the project-root cache. no longer stored in the
    // scene-JSON (recomputed on load from the scene-filename) -> removes a persisted path (W4).
    auto material_path = material_bundle_path(path.string());

    // lightsource-assets
    data.lights = m_scene->asset_provider()->lights();

    // material- and sampler-assets stored in scene-JSON (like lights).
    data.materials = m_scene->asset_provider()->materials();
    data.texture_samplers = m_material_data.texture_samplers;

    // set of mesh-ids
    std::unordered_set<vierkant::MeshId> mesh_ids;
    std::map<vierkant::Object3D *, size_t> obj_to_node_index;

    // bone-attached objects, paired with the nearest non-bone ancestor they are emitted under
    std::vector<std::pair<vierkant::Object3D *, uint32_t>> bone_attachments;

    vierkant::LambdaVisitor visitor;
    visitor.traverse(*m_scene->root(), [&](vierkant::Object3D &obj) -> bool {
        if(&obj == m_scene->root().get()) { return true; }

        // editor-layer objects are tool-state, not scene-content. skip them and their subtrees.
        if(obj.layers & vierkant::LAYER_EDITOR) { return false; }

        // bone-objects mirror a mesh's skeleton and are re-created on demand, so they are not
        // emitted. anything attached below them is ordinary content, so keep descending.
        if(obj.has_component<vierkant::bone_component_t>()) { return true; }

        auto node_index = static_cast<uint32_t>(data.nodes.size());
        obj_to_node_index[&obj] = node_index;

        scene_node_t &node = data.nodes.emplace_back();
        node.name = obj.name;
        node.enabled = obj.enabled;
        if(const auto *obj_transform = obj.transform()) { node.transform = *obj_transform; }
        node.transform_space = obj.transform_space();

        // attached to a bone: anchor by bone-id and emit the node under the nearest non-bone ancestor,
        // since the bones in between have no node to be a child of.
        auto *ancestor = obj.parent();
        if(const auto *bone_cmp = ancestor ? ancestor->get_component_ptr<vierkant::bone_component_t>() : nullptr)
        {
            node.bone_anchor = bone_cmp->node_id;
            while(ancestor && ancestor->has_component<vierkant::bone_component_t>()) { ancestor = ancestor->parent(); }
            if(ancestor) { bone_attachments.emplace_back(ancestor, node_index); }
        }

        if(auto *flags_cmp = obj.get_component_ptr<vierkant::subscene_component_t>())
        {
            if(flags_cmp->scene_id)
            {
                node.scene_id = flags_cmp->scene_id;

                auto path_it = m_scene_paths.find(flags_cmp->scene_id);
                if(path_it != m_scene_paths.end())
                {
                    data.scene_paths[flags_cmp->scene_id] = path_it->second.generic_string();
                }

                // handled as subscene, bail out
                return false;
            }
        }

        if(obj.has_component<vierkant::animation_component_t>())
        {
            node.animation_state = obj.get_component<vierkant::animation_component_t>();
        }

        if(obj.has_component<vierkant::physics_component_t>())
        {
            node.physics_state = obj.get_component<vierkant::physics_component_t>();
        }

        if(obj.has_component<vierkant::constraint_component_t>())
        {
            node.constraints = obj.get_component<vierkant::constraint_component_t>();
        }

        if(obj.has_component<vierkant::camera_component_t>())
        {
            node.camera_state = obj.get_component<vierkant::camera_component_t>();
        }

        if(obj.has_component<vierkant::lightsource_component_t>())
        {
            node.light_state = obj.get_component<vierkant::lightsource_component_t>();
        }

        if(obj.has_component<vierkant::mesh_component_t>())
        {
            const auto &mesh_component = obj.get_component<vierkant::mesh_component_t>();
            const auto &mesh = mesh_component.mesh;

            if(!mesh_ids.contains(mesh->id))
            {
                if(const auto path_it = m_model_paths.find(mesh->id); path_it != m_model_paths.end())
                {
                    data.model_paths[mesh->id] = path_it->second.generic_string();
                    mesh_ids.insert(mesh->id);
                }
            }
        }
        return true;
    });

    // add top-lvl scenegraph-nodes. children skipped above (editor-layer) have no node to point at.
    for(const auto &child: m_scene->root()->children)
    {
        if(auto it = obj_to_node_index.find(child.get()); it != obj_to_node_index.end())
        {
            data.scene_roots.push_back(static_cast<uint32_t>(it->second));
        }
    }

    // the camera we look through, if it is one of this scene's own nodes. the editor-camera has no
    // node (it is skipped above), and neither has a camera inside a sub-scene instance - both leave
    // this absent, which loads as 'view through the editor-camera'.
    if(const auto it = obj_to_node_index.find(m_render_camera.get()); it != obj_to_node_index.end())
    {
        data.active_camera = static_cast<uint32_t>(it->second);
    }

    visitor.traverse(*m_scene->root(), [&](vierkant::Object3D &obj) -> bool {
        if(!obj_to_node_index.contains(&obj)) { return true; }

        // skip object from sub-scenes
        if(auto *flags_cmp = obj.get_component_ptr<vierkant::subscene_component_t>())
        {
            if(flags_cmp->scene_id) { return false; }
        }
        auto &node = data.nodes[obj_to_node_index[&obj]];
        for(const auto &child: obj.children)
        {
            // children that were skipped above (editor-layer, bones) have no node to point at
            if(auto it = obj_to_node_index.find(child.get()); it != obj_to_node_index.end())
            {
                node.children.push_back(it->second);
            }
        }

        if(auto *mesh_component = obj.get_component_ptr<vierkant::mesh_component_t>())
        {
            mesh_state_t mesh_state = {mesh_component->mesh->id, mesh_component->entry_indices,
                                       mesh_component->material_ids, mesh_component->library};
            node.mesh_state = mesh_state;
        }
        return true;
    });

    // link bone-attachments, the loop above only sees direct children
    for(const auto &[ancestor, node_index]: bone_attachments)
    {
        if(auto it = obj_to_node_index.find(ancestor); it != obj_to_node_index.end())
        {
            data.nodes[it->second].children.push_back(node_index);
        }
    }

    std::ofstream ofs(path.string());
    try
    {
        vierkant_ed::save_scene_data(ofs, data);
    } catch(std::exception &e) { spdlog::error(e.what()); }

    // store scene-textures only (materials/samplers now live inline in the scene-JSON above)
    vierkant::material_data_t texture_bundle;
    texture_bundle.textures = m_material_data.textures;
    save_material_bundle(texture_bundle, material_path);
}

void VierkantEd::build_scene(const std::optional<scene_data_t> &scene_data_in, bool clear_scene,
                            vierkant::SceneId scene_id)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    auto load_task = [this, scene_data_in, scene_id, clear_scene, start_time]() {
        // load background (resolve the stored env-key to an openable path)
        if(scene_data_in && clear_scene && !scene_data_in->environment_path.empty())
        {
            load_file(resolve(scene_data_in->environment_path).string(), false);
        }

        struct scene_data_assets_t
        {
            scene_data_t scene_data;
            vierkant::SceneId scene_id;
            //! root-relative key of this scene's file; seeds the derived texture-bundle path.
            std::string scene_key;
            vierkant::material_data_t material_data;
            std::unordered_map<vierkant::texture_key_t, vierkant::ImagePtr> gpu_textures;
            std::unordered_map<vierkant::MeshId, vierkant::MeshPtr> meshes;
            std::vector<vierkant::Object3DPtr> objects;
        };
        std::vector<scene_data_assets_t> scene_assets(1);

        if(scene_data_in)
        {
            scene_assets[0].scene_data = *scene_data_in;
            scene_assets[0].scene_id = scene_id;
            if(auto it = m_scene_paths.find(scene_id); it != m_scene_paths.end())
            {
                scene_assets[0].scene_key = it->second.generic_string();
            }

            // sub-scenes
            std::deque<std::pair<vierkant::SceneId, std::string>> sub_scene_paths = {
                    scene_assets[0].scene_data.scene_paths.begin(), scene_assets[0].scene_data.scene_paths.end()};

            // iterate subscene-paths bfs
            while(!sub_scene_paths.empty())
            {
                auto [id, p] = std::move(sub_scene_paths.front());
                sub_scene_paths.pop_front();

                m_scene_paths[id] = p;

                if(auto sub_scene_data = load_scene_data(resolve(p)))
                {
                    auto &new_scene_asset = scene_assets.emplace_back();
                    new_scene_asset.scene_data = std::move(*sub_scene_data);
                    new_scene_asset.scene_id = id;
                    new_scene_asset.scene_key = p;

                    for(const auto &[sub_scene_id, sub_scene_path]: new_scene_asset.scene_data.scene_paths)
                    {
                        sub_scene_paths.emplace_back(sub_scene_id, sub_scene_path);
                    }
                }
                else { spdlog::error("could not load sub-scene: {}", p); }
            }
            std::unordered_map<std::string, std::future<vierkant::model::load_mesh_result_t>> mesh_future_cache;

            // schedule background creation of meshes
            for(auto &asset: scene_assets)
            {
                for(const auto &path: asset.scene_data.model_paths | std::views::values)
                {
                    if(auto cache_it = mesh_future_cache.find(path); cache_it != mesh_future_cache.end()) {}
                    else
                    {
                        mesh_future_cache[path] = background_queue().post([this, path] { return load_mesh(path); });
                    }
                }

                // load the derived texture-bundle for scene and sub-scenes. its path is no longer
                // stored in the scene-JSON (W4) -> recompute from the scene-key. fall back to any
                // legacy stored path for pre-P1 scenes.
                auto bundle_key = !asset.scene_key.empty() ? material_bundle_path(asset.scene_key).string()
                                                           : asset.scene_data.material_bundle_path;
                if(auto material_data = load_material_bundle(bundle_key))
                {
                    assert(asset.material_data.materials.empty());
                    asset.material_data = std::move(*material_data);

                    for(const auto &[tex_id, tex_variant]: asset.material_data.textures)
                    {
                        vierkant::Image::Format fmt;
                        fmt.sampler_state.max_anisotropy = m_device->properties().core.limits.maxSamplerAnisotropy;

                        std::visit(
                                [this, &asset, tex_id, fmt](auto &&img) {
                                    using T = std::decay_t<decltype(img)>;

                                    const vierkant::texture_key_t tex_key = {tex_id, vierkant::SamplerId::nil()};

                                    if constexpr(std::is_same_v<T, crocore::ImagePtr>)
                                    {
                                        asset.gpu_textures[tex_key] = vierkant::model::create_texture(
                                                m_device, img, fmt, m_queue_image_loading);
                                    }

                                    if constexpr(std::is_same_v<T, vierkant::bcn::compress_result_t>)
                                    {
                                        asset.gpu_textures[tex_key] = vierkant::model::create_compressed_texture(
                                                m_device, img, fmt, m_queue_image_loading);
                                    }
                                },
                                tex_variant);
                    }
                }
            }

            std::unordered_map<std::string, vierkant::model::load_mesh_result_t> mesh_cache;
            for(auto &[path, mesh_future]: mesh_future_cache) { mesh_cache[path] = mesh_future.get(); }

            // load meshes for scene and sub-scenes
            for(auto &asset: scene_assets)
            {
                for(const auto &[mesh_id, path]: asset.scene_data.model_paths)
                {
                    // sync and check
                    const auto &load_mesh_result = mesh_cache[path];

                    // model-file missing or unreadable -> skip. nodes referencing this mesh-id will
                    // find no entry in 'asset.meshes' and become plain (empty) objects instead.
                    if(!load_mesh_result.mesh)
                    {
                        spdlog::warn("skipping missing model: {}", path);
                        continue;
                    }
                    const auto &materials = asset.material_data.materials;

                    // optional material override(s)
                    for(const auto &mat_id: load_mesh_result.mesh->material_ids)
                    {
                        if(auto it = materials.find(mat_id); it != materials.end())
                        {
                            // TODO: test if this makes sense
                            spdlog::trace("material found in cache: {}", it->second.name);
                            m_scene->asset_provider()->add_material(it->second);
                        }
                    }
                    asset.meshes[mesh_id] = load_mesh_result.mesh;

                    asset.material_data.materials.insert(std::make_move_iterator(load_mesh_result.materials.begin()),
                                                         std::make_move_iterator(load_mesh_result.materials.end()));

                    asset.gpu_textures.insert(std::make_move_iterator(load_mesh_result.textures.begin()),
                                              std::make_move_iterator(load_mesh_result.textures.end()));
                }
            }
        }
        else
        {
            auto cube_result = load_mesh("cube");
            scene_assets[0].meshes[cube_result.mesh->id] = cube_result.mesh;
            scene_assets[0].material_data.materials[m_primitive_material.id] = m_primitive_material;
            scene_node_t node = {};
            node.name = "cube";
            node.mesh_state = {vierkant::MeshId::from_name(node.name)};
            scene_assets[0].scene_data.nodes = {node};
            scene_assets[0].scene_data.scene_roots = {0};
            scene_assets[0].scene_id = scene_id;
            m_scene_paths[scene_id] = s_default_scene_path;
        }

        auto create_root_object = [this](const scene_data_t &scene_data,
                                         const std::unordered_map<vierkant::MeshId, vierkant::MeshPtr> &meshes,
                                         std::vector<vierkant::Object3DPtr> &out_objects) -> vierkant::Object3DPtr {
            auto root = m_object_store->create_object();
            root->name = scene_data.name;

            // create objects for all nodes
            for(const auto &node: scene_data.nodes)
            {
                vierkant::Object3DPtr obj;

                if(node.mesh_state && meshes.contains(node.mesh_state->mesh_id))
                {
                    const auto &mesh = meshes.at(node.mesh_state->mesh_id);
                    obj = m_scene->create_mesh_object({mesh, node.mesh_state->entry_indices,
                                                       node.mesh_state->material_ids, node.mesh_state->mesh_library});
                }
                else
                {
                    obj = m_object_store->create_object();
                }
                obj->name = node.name;
                obj->enabled = node.enabled;
                if(node.transform) { obj->set_transform(*node.transform); }
                if(node.transform_space) { obj->set_transform_space(node.transform_space); }
                if(node.animation_state) { obj->add_component(*node.animation_state); }
                if(node.physics_state) { obj->add_component(*node.physics_state); }
                if(node.constraints) { obj->add_component(*node.constraints); }
                if(node.camera_state) { obj->add_component(*node.camera_state); }
                if(node.light_state) { obj->add_component(*node.light_state); }

                out_objects.push_back(obj);
            }

            // recreate node-hierarchy
            for(uint32_t i = 0; i < scene_data.nodes.size(); ++i)
            {
                for(const auto child_index: scene_data.nodes[i].children)
                {
                    out_objects[i]->add_child(out_objects[child_index]);
                }
            }

            // re-attach bone-anchored objects. the save-rule linked them to the mesh-object,
            // so their current parent is the object owning the skeleton.
            for(uint32_t i = 0; i < scene_data.nodes.size(); ++i)
            {
                const auto &node = scene_data.nodes[i];
                if(!node.bone_anchor) { continue; }

                vierkant::Object3D *bone_object = nullptr;

                if(auto *mesh_object = out_objects[i]->parent())
                {
                    m_scene->ensure_bone_mirror(*mesh_object);
                    bone_object = vierkant::bone_object_by_id(*mesh_object, *node.bone_anchor);
                }
                if(bone_object) { bone_object->add_child(out_objects[i]); }
                else { spdlog::warn("could not resolve bone-anchor for node: {}", node.name); }
            }

            // add scene-roots
            for(auto idx: scene_data.scene_roots)
            {
                if(idx < out_objects.size()) { root->add_child(out_objects[idx]); }
                else
                {
                    spdlog::error("scene_data corrupted: index {}", idx);
                }
            }
            return root;
        };

        auto done_cb = [this, scene_assets = std::move(scene_assets), create_root_object, clear_scene,
                        start_time]() mutable {
            // root nodes for all (sub-)scenes
            std::vector<vierkant::Object3DPtr> root_objects(scene_assets.size());

            // map scene-ids to their root-objects
            std::unordered_map<vierkant::SceneId, vierkant::Object3DPtr> scene_root_map;

            for(uint32_t i = 0; i < scene_assets.size(); ++i)
            {
                root_objects[i] =
                        create_root_object(scene_assets[i].scene_data, scene_assets[i].meshes, scene_assets[i].objects);
                scene_root_map[scene_assets[i].scene_id] = root_objects[i];
                auto &cmp = root_objects[i]->add_component<vierkant::subscene_component_t>();
                cmp.scene_id = scene_assets[i].scene_id;
            }

            for(uint32_t i = 0; i < scene_assets.size(); ++i)
            {
                auto &scene_asset = scene_assets[i];

                // stable key for the containing scene: the top-scene's SceneId is random per load, so
                // anchor its instances to a fixed sentinel; sub-scenes use their (stable) file SceneId.
                const std::string containing_key = (i == 0) ? std::string("root") : scene_asset.scene_id.str();

                // connect sub-scenes to nodes
                for(uint32_t j = 0; j < scene_asset.scene_data.nodes.size(); ++j)
                {
                    const auto &node = scene_asset.scene_data.nodes[j];

                    if(node.scene_id)
                    {
                        // a sub-scene that failed to load leaves the slot empty, but keep the flag below
                        // so the reference survives a save-roundtrip instead of being silently dropped.
                        if(auto root_it = scene_root_map.find(*node.scene_id); root_it != scene_root_map.end())
                        {
                            const auto &children = root_it->second->children;

                            // clone into this instance-slot. deriving new body-ids from a stable per-slot seed
                            // keeps them constant across reloads, so constraints referencing this sub-scene's
                            // bodies (incl. from the enclosing scene) keep resolving after save/load.
                            const std::string instance_seed = containing_key + "/" + std::to_string(j);
                            for(auto clones = clone_objects({children.begin(), children.end()}, instance_seed);
                                const auto &child: clones)
                            {
                                scene_asset.objects[j]->add_child(child);
                            }
                        }
                        else
                        {
                            spdlog::error("node '{}': missing sub-scene {}", node.name, node.scene_id->str());
                        }

                        // flag object to contain a sub-scene
                        auto &cmp = scene_asset.objects[j]->add_component<vierkant::subscene_component_t>();
                        cmp.scene_id = *node.scene_id;
                    }
                }
            }

            if(root_objects[0])
            {
                if(clear_scene)
                {
                    m_render_camera = m_editor_camera;
                    m_scene->clear();
                    for(const auto children = root_objects[0]->children; const auto &child: children)
                    {
                        m_scene->add_object(child);
                    }
                    // restore the camera the scene was saved with. m_render_camera already points at
                    // the editor-camera, which is what an absent or unresolvable index means.
                    if(const auto &active_camera = scene_assets[0].scene_data.active_camera)
                    {
                        const auto &objects = scene_assets[0].objects;
                        vierkant::Object3DPtr cam;
                        if(*active_camera < objects.size()) { cam = objects[*active_camera]; }

                        if(cam && cam->has_component<vierkant::camera_component_t>()) { m_render_camera = cam; }
                        else
                        {
                            spdlog::warn("scene-data: active_camera ({}) does not resolve to a camera",
                                         *active_camera);
                        }
                    }

                    m_scene_id = scene_assets[0].scene_id;
                    m_scene->root()->name = root_objects[0]->name.empty() ? "scene" : root_objects[0]->name;
                    m_scene->environment_factor = scene_assets[0].scene_data.environment_factor;

                    // reset host-side store; the GPU store is pruned below once the new scene is assembled
                    m_material_data = {};
                }
                else
                {
                    m_scene->add_object(root_objects[0]);
                }

                const auto &provider = m_scene->asset_provider();

                // material-library roots: everything loaded from the scene's material-bundle(s), so
                // deliberately-authored materials survive the prune even when no mesh references them
                std::unordered_set<vierkant::MaterialId> library_materials;

                // same for lightsource-assets from the scene-file(s)
                std::unordered_set<vierkant::LightId> library_lights;

                for(const auto &scene_asset: scene_assets)
                {
                    // host-side texture/sampler store (kept for serialization)
                    m_material_data.textures.insert(scene_asset.material_data.textures.begin(),
                                                    scene_asset.material_data.textures.end());
                    m_material_data.texture_samplers.insert(scene_asset.material_data.texture_samplers.begin(),
                                                            scene_asset.material_data.texture_samplers.end());
                    // ...and inline authored samplers from the scene-JSON
                    m_material_data.texture_samplers.insert(scene_asset.scene_data.texture_samplers.begin(),
                                                            scene_asset.scene_data.texture_samplers.end());

                    // GPU runtime store (the AssetProvider owns the materials)
                    for(const auto &mat: scene_asset.material_data.materials | std::views::values)
                    {
                        provider->add_material(mat);
                        library_materials.insert(mat.id);
                    }
                    // inline authored materials from scene-JSON.
                    // bundle-materials loop above is a no-op for freshly-saved scenes
                    for(const auto &mat: scene_asset.scene_data.materials | std::views::values)
                    {
                        provider->add_material(mat);
                        library_materials.insert(mat.id);
                    }
                    for(const auto &[key, tex]: scene_asset.gpu_textures) { provider->add_texture(key, tex); }

                    for(const auto &l: scene_asset.scene_data.lights | std::views::values)
                    {
                        provider->add_light(l);
                        library_lights.insert(l.id);
                    }
                }

                if(clear_scene)
                {
                    // drop assets from the previous scene (keeping the material-library roots), then
                    // re-assert the always-present primitives
                    m_scene->prune_assets(library_materials, library_lights);
                    provider->add_material(m_primitive_material);
                    provider->add_texture({m_primitive_texture_id, vierkant::SamplerId::nil()}, m_primitive_texture);
                    provider->add_texture({m_noise_texture_id, vierkant::SamplerId::nil()}, m_noise_texture);
                }
            }
            if(m_path_tracer) { m_path_tracer->reset_accumulator(); }

            // log timing
            auto build_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::high_resolution_clock::now() - start_time)
                                    .count() /
                            1000.f;
            spdlog::debug("done building scene ({:.2f} s): {}", build_ms, m_scene_paths[m_scene_id].string());
        };
        main_queue().post(done_cb);
    };
    background_queue().post(load_task);
}

std::vector<vierkant::Object3DPtr> VierkantEd::clone_objects(const std::set<vierkant::Object3DPtr> &objects,
                                                            const std::optional<std::string> &instance_seed) const
{
    std::unordered_map<vierkant::BodyId, vierkant::BodyId> body_id_map = {
            {vierkant::BodyId::nil(), vierkant::BodyId::nil()}};

    // derive a new body-id for a cloned body. with an instance-seed the result is deterministic
    // (stable across reloads), so persisted constraints referencing sub-scene bodies keep resolving;
    // without a seed a fresh random id is used (a genuinely new object, e.g. copy/paste).
    auto make_body_id = [&instance_seed](const vierkant::BodyId &orig) {
        return instance_seed ? vierkant::BodyId::from_name(*instance_seed + orig.str()) : vierkant::BodyId::random();
    };

    auto remap_id = [&body_id_map](vierkant::BodyId &body_id) {
        if(auto it = body_id_map.find(body_id); it != body_id_map.end()) { body_id = it->second; }
    };

    std::vector<vierkant::Object3DPtr> clones;
    clones.reserve(objects.size());

    // 1st pass, generate clones + new body-ids and init map
    for(const auto &obj: objects)
    {
        auto cloned_obj = m_object_store->clone(obj.get());
        clones.push_back(cloned_obj);

        vierkant::LambdaVisitor visitor;
        visitor.traverse(*cloned_obj, [&body_id_map, &make_body_id](auto &obj) -> bool {
            if(auto *phy_cmp_ptr = obj.template get_component_ptr<vierkant::physics_component_t>())
            {
                phy_cmp_ptr->mode = vierkant::physics_component_t::INACTIVE;

                // assign new body id
                auto new_body_id = make_body_id(phy_cmp_ptr->body_id);
                body_id_map[phy_cmp_ptr->body_id] = new_body_id;
                phy_cmp_ptr->body_id = new_body_id;
            }
            return true;
        });
    }

    // 2nd pass: adjust body-ids in constraints
    for(const auto &cloned_obj: clones)
    {
        vierkant::LambdaVisitor visitor;
        visitor.traverse(*cloned_obj, [&remap_id](auto &obj) -> bool {
            // remap body-ids for constraints
            if(auto *constraint_cmp = obj.template get_component_ptr<vierkant::constraint_component_t>())
            {
                for(auto &body_constraint: constraint_cmp->body_constraints)
                {
                    remap_id(body_constraint.body_id1);
                    remap_id(body_constraint.body_id2);
                }
            }
            return true;
        });
    }
    return clones;
}

vierkant::model::load_mesh_result_t VierkantEd::load_mesh(const std::filesystem::path &path)
{
    ++m_num_loading;
    vierkant::model::load_mesh_result_t result;

    bool is_primitive = false;
    for(const auto &[prim_type, prim_name]: vierkant::AssetProvider::primitive_names())
    {
        if(path == prim_name)
        {
            is_primitive = true;
            result.mesh = m_scene->asset_provider()->primitive_mesh(prim_type);
            result.materials = {{m_primitive_material.id, m_primitive_material}};
            break;
        }
    }


    if(!is_primitive && !path.empty())
    {
        // 'path' is a project-key (root-relative, portable). identity is seeded from that key,
        // while all file-I/O happens against the key resolved to an absolute path.
        const std::string key = path.generic_string();
        const std::filesystem::path abs = resolve(key);
        spdlog::debug("loading model '{}'", abs.string());

        // opt-in to CPU opacity-micromap baking (alpha-masked geometry only)
        std::optional<vierkant::model::omm_gen_params_t> omm_params;
        if(m_settings.opacity_micromaps) { omm_params = vierkant::model::omm_gen_params_t{}; }

        // canonical cache-path for filename+params (hash uses filename() only), search existing bundle
        std::filesystem::path bundle_path =
                m_project_root / g_cache_path / g_model_store_path /
                vierkant::model_bundle_filename(abs, m_settings.mesh_buffer_params,
                                                       m_settings.texture_compression, omm_params);

        auto mesh_id = vierkant::MeshId::from_name(key);
        bool bundle_created = false;
        auto model_assets = load_asset_bundle(bundle_path);

        if(!model_assets)
        {
            // load model-file and bake a self-contained asset-bundle (lods/meshlets/texture-compression)
            // seed asset-ids from the stable project-key (not the machine-local absolute path), so
            // baked texture/material/sampler-ids survive project relocation
            vierkant::bundle_params_t bundle_params = {.mesh_buffer_params = m_settings.mesh_buffer_params,
                                                              .compress_textures = m_settings.texture_compression,
                                                              .omm_params = omm_params,
                                                              .id_seed = key,
                                                              .pool = &background_queue()};
            model_assets = vierkant::create_model_bundle(abs, bundle_params);

            if(!model_assets)
            {
                --m_num_loading;
                return {};
            }
            bundle_created = true;
        }

        vierkant::model::load_mesh_params_t load_params = {};
        load_params.device = m_device;
        load_params.load_queue = m_queue_model_loading;
        load_params.mesh_buffers_params = m_settings.mesh_buffer_params;
        load_params.buffer_flags = m_mesh_buffer_flags;

        // forward OMM params; load_mesh adopts pre-baked bundle data if present, else live-bakes
        load_params.omm_params = omm_params;

        result = vierkant::model::load_mesh(load_params, *model_assets);
        result.mesh->id = mesh_id;

        // load_mesh keyed the OMM-cache on the mesh-id it assigned internally; re-stamp with the
        // final scene mesh-id so RayBuilder lookups (which use the scene mesh) hit, then accumulate
        for(auto &[omm_key, omm_entry]: result.omm_cache)
        {
            m_scene_omm_cache[{mesh_id, omm_key.entry_index, omm_key.color_texture_id}] = std::move(omm_entry);
        }

        // merge loaded materials/textures/samplers into the GPU runtime store
        m_scene->asset_provider()->populate(result);

        --m_num_loading;

        // populate stores the gpu-mesh only; attach the persist-able bundle for physics
        m_scene->asset_provider()->add_mesh(
                mesh_id,
                {.mesh = result.mesh, .bundle = std::get<vierkant::mesh_buffer_bundle_t>(model_assets->geometry_data)});

        if(bundle_created && m_settings.cache_mesh_bundles)
        {
            background_queue().post([this, mesh_assets = std::move(model_assets), bundle_path]() {
                save_asset_bundle(*mesh_assets, bundle_path);
            });
        }
    }

    // store mesh/path
    m_model_paths[result.mesh->id] = path;
    return result;
}

std::optional<scene_data_t> VierkantEd::load_scene_data(const std::filesystem::path &path)
{
    // create and open a character archive for input
    std::ifstream file_stream(path.string());

    if(file_stream.is_open())
    {
        spdlog::debug("loading scene: {}", path.string());
        return vierkant_ed::load_scene_data(file_stream);
    }
    return {};
}

std::optional<std::filesystem::path> VierkantEd::zip_archive_path() const
{
    if(m_settings.cache_zip_archive) { return m_project_root / g_zip_path; }
    return {};
}

void VierkantEd::save_asset_bundle(const vierkant::model::model_assets_t &mesh_assets,
                                  const std::filesystem::path &path) const
{ vierkant::save_bundle_file(mesh_assets, path, zip_archive_path()); }

std::optional<vierkant::model::model_assets_t> VierkantEd::load_asset_bundle(const std::filesystem::path &path) const
{ return vierkant::load_model_bundle_file(path, m_project_root / g_zip_path); }

void VierkantEd::save_material_bundle(const vierkant::material_data_t &material_data,
                                     const std::filesystem::path &path) const
{ vierkant::save_bundle_file(material_data, path, zip_archive_path()); }

std::optional<vierkant::material_data_t> VierkantEd::load_material_bundle(const std::filesystem::path &path) const
{ return vierkant::load_material_bundle_file(path, m_project_root / g_zip_path); }

bool VierkantEd::parse_override_settings(int argc, char *argv[])
{
    // available options
    cxxopts::Options options(argv[0], "3d-model viewer with rasterization and path-tracer backends\n");
    options.positional_help("[<model-file>] [<hdr-image>]");
    options.add_options()("help", "print this help message");
    options.add_options()("w,width", "window width in px", cxxopts::value<uint32_t>());
    options.add_options()("h,height", "window height in px", cxxopts::value<uint32_t>());
    options.add_options()("v,verbose", "verbose printing");
    options.add_options()("q,quiet", "minimal printing");
    options.add_options()("log-file", "enable logging to a file", cxxopts::value<std::string>());
    options.add_options()("f,fullscreen", "enable fullscreen");
    options.add_options()("no-fullscreen", "disable fullscreen");
    options.add_options()("vsync", "enable vsync");
    options.add_options()("no-vsync", "disable vsync");
    options.add_options()("hdr", "enable hdr");
    options.add_options()("no-hdr", "disable hdr");
    options.add_options()("font", "provide a font-file (.ttf | .otf)", cxxopts::value<std::string>());
    options.add_options()("font-size", "provide a font-size", cxxopts::value<float>());
    options.add_options()("validation", "enable vulkan validation");
    options.add_options()("no-validation", "disable vulkan validation");
    options.add_options()("l,labels", "enable vulkan debug-labels");
    options.add_options()("no-labels", "disable vulkan debug-labels");
    options.add_options()("raytracing", "enable vulkan raytracing extensions");
    options.add_options()("no-raytracing", "disable vulkan raytracing extensions");
    options.add_options()("mesh-shader", "enable vulkan mesh-shader extensions");
    options.add_options()("no-mesh-shader", "disable vulkan mesh-shader extensions");
    options.add_options()("project-root", "asset/project root all scene-paths resolve against",
                          cxxopts::value<std::string>());
    options.add_options()("files", "provided input files", cxxopts::value<std::vector<std::string>>());
    options.parse_positional("files");

    cxxopts::ParseResult result;

    try
    {
        result = options.parse(argc, argv);
    } catch(std::exception &e)
    {
        spdlog::error(e.what());
        return false;
    }

    // establish the project-root: an explicit --project-root wins; otherwise the first opened
    // scene-file's parent dir becomes the root; else it stays CWD (default). set once per session.
    if(result.count("project-root"))
    {
        m_project_root =
                std::filesystem::weakly_canonical(std::filesystem::absolute(result["project-root"].as<std::string>()));
        m_project_root_explicit = true;
        spdlog::debug("project root (explicit): {}", m_project_root.string());
    }

    if(result.count("files"))
    {
        const auto &files = result["files"].as<std::vector<std::string>>();

        // root = parent-dir of the first opened scene-file (unless --project-root was given)
        if(!m_project_root_explicit)
        {
            for(const auto &f: files)
            {
                if(std::filesystem::path(f).extension() == ".json")
                {
                    establish_project_root(f);
                    break;
                }
            }
        }

        for(const auto &f: files)
        {
            switch(crocore::filesystem::get_file_type(f))
            {
                case crocore::filesystem::FileType::IMAGE: m_scene_data.environment_path = project_key(f); break;

                case crocore::filesystem::FileType::MODEL:
                {
                    vierkant::MeshId mesh_id;
                    m_scene_data.model_paths = {{mesh_id, project_key(f)}};
                    scene_node_t node = {};
                    node.name = std::filesystem::path(f).filename().string();
                    node.mesh_state = {mesh_id};
                    m_scene_data.nodes = {node};
                    m_scene_data.scene_roots = {0};
                    break;
                }

                default:
                {
                    if(auto scene_data = load_scene_data(f))
                    {
                        m_scene_data = *scene_data;
                        // remember the real top-scene key so the derived bundle-path resolves (setup()
                        // otherwise defaults this to s_default_scene_path).
                        m_scene_paths[m_scene_id] = project_key(f);
                    }
                }
                break;
            }
        }
    }

    // print usage
    if(result.count("help"))
    {
        spdlog::set_pattern("%v");
        spdlog::info("\n{}", options.help());
        return false;
    }
    if(result.count("width")) { m_settings.window_info.size.x = (int) result["width"].as<uint32_t>(); }
    if(result.count("height")) { m_settings.window_info.size.y = (int) result["height"].as<uint32_t>(); }
    if(result.count("log-file")) { m_settings.log_file = result["log-file"].as<std::string>(); }
    if(result.count("fullscreen")) { m_settings.window_info.fullscreen = true; }
    if(result.count("no-fullscreen")) { m_settings.window_info.fullscreen = false; }
    if(result.count("vsync")) { m_settings.window_info.vsync = true; }
    if(result.count("no-vsync")) { m_settings.window_info.vsync = false; }
    if(result.count("hdr")) { m_settings.window_info.use_hdr = true; }
    if(result.count("no-hdr")) { m_settings.window_info.use_hdr = false; }
    if(result.count("font")) { m_settings.font_url = result["font"].as<std::string>(); }
    if(result.count("font-size")) { m_settings.ui_font_scale = result["font-size"].as<float>(); }
    if(result.count("validation")) { m_settings.use_validation = true; }
    if(result.count("no-validation")) { m_settings.use_validation = false; }
    if(result.count("labels")) { m_settings.use_debug_labels = true; }
    if(result.count("no-labels")) { m_settings.use_debug_labels = false; }
    if(result.count("verbose")) { m_settings.log_level = spdlog::level::debug; }
    if(result.count("quiet")) { m_settings.log_level = spdlog::level::info; }
    if(result.count("raytracing"))
    {
        m_settings.enable_ray_query_features = true;
        m_settings.enable_raytracing_pipeline_features = true;
        m_settings.path_tracing = true;
    }
    if(result.count("no-raytracing"))
    {
        m_settings.enable_ray_query_features = false;
        m_settings.enable_raytracing_pipeline_features = false;
        m_settings.path_tracing = false;
    }
    if(result.count("mesh-shader")) { m_settings.enable_mesh_shader_device_features = true; }
    if(result.count("no-mesh-shader")) { m_settings.enable_mesh_shader_device_features = false; }
    spdlog::set_level(m_settings.log_level);
    return true;
}