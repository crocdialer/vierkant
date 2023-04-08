//
// Created by crocdialer on 11/15/20.
//

#include <unordered_set>
#include <vierkant/RayBuilder.hpp>

namespace vierkant
{

inline VkTransformMatrixKHR vk_transform_matrix(const glm::mat4 &m)
{
    VkTransformMatrixKHR ret;
    auto transpose = glm::transpose(m);
    memcpy(&ret, glm::value_ptr(transpose), sizeof(VkTransformMatrixKHR));
    return ret;
}

std::vector<const char *> RayBuilder::required_extensions()
{
    return {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};
}

RayBuilder::RayBuilder(const vierkant::DevicePtr &device, VkQueue queue, vierkant::VmaPoolPtr pool)
    : m_device(device), m_queue(queue), m_memory_pool(std::move(pool))
{
    m_properties = {};
    m_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 device_properties = {};
    device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    device_properties.pNext = &m_properties;
    vkGetPhysicalDeviceProperties2(device->physical_device(), &device_properties);


    // fallback to first device queue
    m_queue = queue ? queue : m_device->queue();

    // get the ray tracing and acceleration-structure related function pointers
    set_function_pointers();

    m_command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                   VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                           VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // solid white color
    uint32_t v = 0xFFFFFFFF;
    vierkant::Image::Format fmt;
    fmt.extent = {1, 1, 1};
    fmt.format = VK_FORMAT_R8G8B8A8_UNORM;
    m_placeholder_solid_white = vierkant::Image::create(m_device, &v, fmt);

    // normal: vec3(0, 0, 1)
    v = 0xFFFF7F7F;
    m_placeholder_normalmap = vierkant::Image::create(m_device, &v, fmt);

    // emission: vec3(0)
    v = 0xFF000000;
    m_placeholder_emission = vierkant::Image::create(m_device, &v, fmt);

    // ao/rough/metal: vec3(1, 1, 1)
    v = 0xFFFFFFFF;
    m_placeholder_ao_rough_metal = vierkant::Image::create(m_device, &v, fmt);

    m_placeholder_buffer = vierkant::Buffer::create(
            m_device, nullptr, 1, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);
}

RayBuilder::build_result_t RayBuilder::create_mesh_structures(const create_mesh_structures_params_t &params) const
{
    // raytracing flags
    VkBuildAccelerationStructureFlagsKHR flags = 0;
    if(params.enable_compaction) { flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR; }

    // vertex-skinned meshes need to update their AABBs
    if(params.mesh->root_bone || params.mesh->morph_buffer)
    {
        flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
                 VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    }
    else { flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR; }

    // optionally override mesh-vertexbuffer
    const auto &vertex_attrib = params.mesh->vertex_attribs.at(vierkant::Mesh::AttribLocation::ATTRIB_POSITION);

    const auto &vertex_buffer = params.vertex_buffer ? params.vertex_buffer : vertex_attrib.buffer;
    size_t vertex_buffer_offset = params.vertex_buffer ? params.vertex_buffer_offset : 0;

    VkDeviceAddress vertex_base_address = vertex_buffer->device_address() + vertex_buffer_offset + vertex_attrib.offset;
    VkDeviceAddress index_base_address = params.mesh->index_buffer->device_address();

    size_t num_entries = params.mesh->entries.size();
    std::vector<VkAccelerationStructureGeometryKHR> geometries(num_entries);
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> offsets(num_entries);
    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> build_infos(num_entries);

    // timelinesemaphore to track builds
    build_result_t ret = {};
    ret.semaphore = vierkant::Semaphore(m_device);
    ret.compact = params.enable_compaction;
    ret.build_command = vierkant::CommandBuffer(m_device, m_command_pool.get());
    ret.build_command.begin();

    // used to query compaction sizes after building
    ret.query_pool = create_query_pool(m_device, num_entries, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR);

    // those will be stored
    std::vector<acceleration_asset_ptr> entry_assets(num_entries);
    bool update = params.update_assets.size() == num_entries;

    for(uint32_t i = 0; i < num_entries; ++i)
    {
        update = update && params.update_assets[i] && params.update_assets[i]->structure;

        const auto &entry = params.mesh->entries[i];
        const auto &lod = entry.lods.front();
        const auto &material = params.mesh->materials[entry.material_index];

        // throw on non-triangle entries
        if(entry.primitive_type != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        {
            throw std::runtime_error("RaytracingPipeline::add_mesh: provided non-triangle entry");
        }

        VkAccelerationStructureGeometryTrianglesDataKHR triangles = {};
        triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        triangles.indexType = params.mesh->index_type;
        triangles.indexData.deviceAddress = index_base_address + lod.base_index * sizeof(index_t);
        triangles.vertexFormat = vertex_attrib.format;
        triangles.vertexData.deviceAddress = vertex_base_address + entry.vertex_offset * vertex_attrib.stride;
        triangles.vertexStride = vertex_attrib.stride;
        triangles.maxVertex = entry.num_vertices;
        triangles.transformData = {};

        auto &geometry = geometries[i];
        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.flags = material->blend_mode == vierkant::Material::BlendMode::Opaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.geometry.triangles = triangles;

        // offsets
        auto &offset = offsets[i];
        offset.firstVertex = 0;
        offset.primitiveOffset = 0;
        offset.primitiveCount = lod.num_indices / 3;

        auto &build_info = build_infos[i];
        build_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        build_info.flags = flags;
        build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        build_info.geometryCount = 1;
        build_info.pGeometries = &geometry;

        // optionally use existing structure for update
        build_info.srcAccelerationStructure = update ? params.update_assets[i]->structure.get() : VK_NULL_HANDLE;

        // query memory requirements
        VkAccelerationStructureBuildSizesInfoKHR size_info = {};
        size_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

        vkGetAccelerationStructureBuildSizesKHR(m_device->handle(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                &build_infos[i], &offsets[i].primitiveCount, &size_info);

        // create new asset
        entry_assets[i] = std::make_shared<acceleration_asset_t>();
        VkAccelerationStructureCreateInfoKHR create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        create_info.size = size_info.accelerationStructureSize;
        *entry_assets[i] = create_acceleration_asset(create_info);

        auto &acceleration_asset = *entry_assets[i];

        // track vertex-buffer + offset used
        acceleration_asset.vertex_buffers = {vertex_buffer};
        acceleration_asset.vertex_buffer_offsets = {vertex_buffer_offset};

        // Allocate the scratch buffers holding the temporary data of the
        // acceleration structure builder
        acceleration_asset.scratch_buffer =
                vierkant::Buffer::create(m_device, nullptr, size_info.buildScratchSize,
                                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY, m_memory_pool);

        // assign acceleration structure and scratch_buffer
        build_info.dstAccelerationStructure = acceleration_asset.structure.get();
        build_info.scratchData.deviceAddress = acceleration_asset.scratch_buffer->device_address();

        // build the AS
        const VkAccelerationStructureBuildRangeInfoKHR *offset_ptr = &offset;
        vkCmdBuildAccelerationStructuresKHR(ret.build_command.handle(), 1, &build_info, &offset_ptr);

        // Write compacted size to query number idx.
        if(params.enable_compaction)
        {
            // barrier before reading back size
            VkMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
            vkCmdPipelineBarrier(ret.build_command.handle(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0,
                                 nullptr);

            VkAccelerationStructureKHR accel_structure = acceleration_asset.structure.get();
            vkCmdWriteAccelerationStructuresPropertiesKHR(ret.build_command.handle(), 1, &accel_structure,
                                                          VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                                                          ret.query_pool.get(), i);
        }
    }

    vierkant::semaphore_submit_info_t semaphore_build_info = {};
    semaphore_build_info.semaphore = ret.semaphore.handle();
    semaphore_build_info.signal_value = SemaphoreValue::BUILD;
    ret.build_command.submit(m_queue, false, VK_NULL_HANDLE, {params.semaphore_info, semaphore_build_info});

    ret.acceleration_assets = std::move(entry_assets);
    ret.update_assets = params.update_assets;
    return ret;
}

void RayBuilder::compact(build_result_t &build_result) const
{
    // memory-compaction for bottom-lvl-structures
    std::vector<acceleration_asset_ptr> entry_assets_compact(build_result.acceleration_assets.size());

    build_result.semaphore.wait(SemaphoreValue::BUILD);

    // Get the size result back
    std::vector<VkDeviceSize> compact_sizes(build_result.acceleration_assets.size());
    vkCheck(vkGetQueryPoolResults(m_device->handle(), build_result.query_pool.get(), 0, (uint32_t) compact_sizes.size(),
                                  compact_sizes.size() * sizeof(VkDeviceSize), compact_sizes.data(),
                                  sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT),
            "RayBuilder::add_mesh: could not query compacted acceleration-structure sizes");

    build_result.compact_command = vierkant::CommandBuffer(m_device, m_command_pool.get());
    build_result.compact_command.begin();

    // compacting
    for(uint32_t i = 0; i < entry_assets_compact.size(); i++)
    {
        spdlog::trace("reducing bottom-lvl-size ({}), from {}kB to {}kB", i,
                      (uint32_t) build_result.acceleration_assets[i]->buffer->num_bytes() / 1024,
                      compact_sizes[i] / 1024);

        // Creating a compact version of the AS
        VkAccelerationStructureCreateInfoKHR create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        create_info.size = compact_sizes[i];

        entry_assets_compact[i] = std::make_shared<acceleration_asset_t>();
        auto &acceleration_asset = *entry_assets_compact[i];
        acceleration_asset = create_acceleration_asset(create_info);

        // copy the original BLAS to a compact version
        VkCopyAccelerationStructureInfoKHR copy_info = {};
        copy_info.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
        copy_info.src = build_result.acceleration_assets[i]->structure.get();
        copy_info.dst = acceleration_asset.structure.get();
        copy_info.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
        vkCmdCopyAccelerationStructureKHR(build_result.compact_command.handle(), &copy_info);
    }

    vierkant::semaphore_submit_info_t semaphore_compact_info = {};
    semaphore_compact_info.semaphore = build_result.semaphore.handle();
    semaphore_compact_info.signal_value = SemaphoreValue::COMPACTED;
    build_result.compact_command.submit(m_queue, false, VK_NULL_HANDLE, {semaphore_compact_info});

    build_result.compacted_assets = std::move(entry_assets_compact);
}

void RayBuilder::set_function_pointers()
{
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
            vkGetDeviceProcAddr(m_device->handle(), "vkCmdBuildAccelerationStructuresKHR"));
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
            vkGetDeviceProcAddr(m_device->handle(), "vkCreateAccelerationStructureKHR"));
    vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
            vkGetDeviceProcAddr(m_device->handle(), "vkDestroyAccelerationStructureKHR"));
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
            vkGetDeviceProcAddr(m_device->handle(), "vkGetAccelerationStructureBuildSizesKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
            vkGetDeviceProcAddr(m_device->handle(), "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCmdWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(
            vkGetDeviceProcAddr(m_device->handle(), "vkCmdWriteAccelerationStructuresPropertiesKHR"));
    vkCmdCopyAccelerationStructureKHR = reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(
            vkGetDeviceProcAddr(m_device->handle(), "vkCmdCopyAccelerationStructureKHR"));
}

RayBuilder::acceleration_asset_t
RayBuilder::create_acceleration_asset(VkAccelerationStructureCreateInfoKHR create_info) const
{
    RayBuilder::acceleration_asset_t acceleration_asset = {};
    acceleration_asset.buffer = vierkant::Buffer::create(m_device, nullptr, create_info.size,
                                                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                         VMA_MEMORY_USAGE_GPU_ONLY, m_memory_pool);

    create_info.buffer = acceleration_asset.buffer->handle();

    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;

    // create acceleration structure
    vkCheck(vkCreateAccelerationStructureKHR(m_device->handle(), &create_info, nullptr, &handle),
            "could not create acceleration structure");

    // get device address
    VkAccelerationStructureDeviceAddressInfoKHR address_info = {};
    address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    address_info.accelerationStructure = handle;
    acceleration_asset.device_address = vkGetAccelerationStructureDeviceAddressKHR(m_device->handle(), &address_info);

    acceleration_asset.structure = AccelerationStructurePtr(
            handle, [device = m_device, buffer = acceleration_asset.buffer, pFn = vkDestroyAccelerationStructureKHR](
                            VkAccelerationStructureKHR s) { pFn(device->handle(), s, nullptr); });
    return acceleration_asset;
}

RayBuilder::acceleration_asset_t RayBuilder::create_toplevel(const vierkant::SceneConstPtr &scene,
                                                             const entity_asset_map_t &asset_map,
                                                             VkCommandBuffer commandbuffer,
                                                             const vierkant::AccelerationStructurePtr &last) const
{
    std::vector<VkAccelerationStructureInstanceKHR> instances;

    std::vector<entry_t> entries;
    std::vector<material_struct_t> materials;
    std::vector<vierkant::ImagePtr> textures = {m_placeholder_solid_white};
    std::vector<vierkant::ImagePtr> normalmaps = {m_placeholder_normalmap};
    std::vector<vierkant::ImagePtr> emissions = {m_placeholder_emission};
    std::vector<vierkant::ImagePtr> ao_rough_metal_maps = {m_placeholder_ao_rough_metal};

    //! vertex- and index-buffers for the entire scene
    std::vector<vierkant::BufferPtr> vertex_buffers;
    std::vector<vierkant::BufferPtr> index_buffers;
    std::vector<VkDeviceSize> vertex_buffer_offsets;
    std::vector<VkDeviceSize> index_buffer_offsets;

    // build flags
    VkBuildAccelerationStructureFlagsKHR build_flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                                                       VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;

    // vertex-buffer address -> index
    std::unordered_map<VkDeviceAddress, size_t> mesh_buffer_indices;

    std::unordered_map<MaterialConstPtr, size_t> material_indices;
    auto view = scene->registry()->view<vierkant::Object3D *, vierkant::MeshPtr>();

    for(const auto &[entity, object, mesh]: view.each())
    {
        assert(mesh);

        if(!asset_map.contains(object->id()))
        {
            spdlog::warn("could not find required bottom-lvl structure, skipping ...");
            continue;
        }

        const auto &acceleration_assets = asset_map.at(object->id());
        assert(mesh->entries.size() == acceleration_assets.size());

        const auto &vertex_attrib = mesh->vertex_attribs.at(vierkant::Mesh::AttribLocation::ATTRIB_POSITION);
        const auto &vertex_buffer = acceleration_assets[0]->vertex_buffers.empty()
                                            ? vertex_attrib.buffer
                                            : acceleration_assets[0]->vertex_buffers[0];
        size_t vertex_buffer_offset = acceleration_assets[0]->vertex_buffer_offsets.empty()
                                              ? vertex_attrib.buffer_offset
                                              : acceleration_assets[0]->vertex_buffer_offsets[0];

        VkDeviceAddress vertex_buffer_address = vertex_buffer->device_address() + vertex_buffer_offset;

        if(!mesh_buffer_indices.contains(vertex_buffer_address))
        {
            mesh_buffer_indices[vertex_buffer_address] = vertex_buffers.size();

            vertex_buffers.push_back(vertex_buffer);
            vertex_buffer_offsets.push_back(vertex_buffer_offset);
            index_buffers.push_back(mesh->index_buffer);
            index_buffer_offsets.push_back(mesh->index_buffer_offset);
        }

        // entry animation transforms
        // NOTE: vertex-skin/morph animations use baked vertex-buffers and new bottom-level assets per frame instead
        std::vector<vierkant::transform_t> node_transforms;

        if(!(mesh->root_bone || mesh->morph_buffer) && object->has_component<animation_state_t>())
        {
            auto &animation_state = object->get_component<animation_state_t>();
            const auto &anim_state = animation_state;

            if(anim_state.index < mesh->node_animations.size())
            {
                const auto &animation = mesh->node_animations[anim_state.index];
                vierkant::nodes::build_node_matrices_bfs(
                        mesh->root_node, animation, static_cast<float>(animation_state.current_time), node_transforms);
            }
        }

        for(uint32_t i = 0; i < acceleration_assets.size(); ++i)
        {
            const auto &mesh_entry = mesh->entries[i];
            const auto &lod = mesh_entry.lods.front();
            const auto &mesh_material = mesh->materials[mesh_entry.material_index];

            const auto &asset = acceleration_assets[i];

            // skip disabled entries
            if(!mesh_entry.enabled) { continue; }

            // apply node-animation transform, if any
            auto transform = object->transform *
                             (node_transforms.empty() ? mesh_entry.transform : node_transforms[mesh_entry.node_index]);

            // per bottom-lvl instance
            VkAccelerationStructureInstanceKHR instance{};
            instance.transform = vk_transform_matrix(mat4_cast(transform));

            // instance flags
            VkGeometryInstanceFlagsKHR instance_flags =
                    mesh_material->two_sided ? VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR : 0;

            // store next entry-index
            instance.instanceCustomIndex = entries.size();
            instance.mask = 0xFF;
            instance.instanceShaderBindingTableRecordOffset = 0;
            instance.flags = instance_flags;
            instance.accelerationStructureReference = asset->device_address;
            instances.push_back(instance);

            if(!material_indices.contains(mesh_material))
            {
                material_indices[mesh_material] = materials.size();
                RayBuilder::material_struct_t material = {};
                material.color = mesh_material->color;
                material.emission = mesh_material->emission;
                material.roughness = mesh_material->roughness;
                material.metalness = mesh_material->metalness;
                material.transmission = mesh_material->transmission;
                material.ior = mesh_material->ior;
                material.attenuation_distance = mesh_material->attenuation_distance;
                material.attenuation_color = {mesh_material->attenuation_color, 0.f};
                material.clearcoat_factor = mesh_material->clearcoat_factor;
                material.clearcoat_roughness_factor = mesh_material->clearcoat_roughness_factor;
                material.sheen_color = {mesh_material->sheen_color, 0.f};
                material.sheen_roughness = mesh_material->sheen_roughness;

                material.blend_mode = static_cast<uint32_t>(mesh_material->blend_mode);
                material.alpha_cutoff = mesh_material->alpha_cutoff;

                material.iridescence_strength = mesh_material->iridescence_factor;
                material.iridescence_ior = mesh_material->iridescence_ior;
                material.iridescence_thickness_range = mesh_material->iridescence_thickness_range;

                for(auto &[type_flag, tex]: mesh_material->textures)
                {
                    material.texture_type_flags |= type_flag;

                    if(type_flag == vierkant::Material::TextureType::Color)
                    {
                        material.texture_index = textures.size();
                        textures.push_back(tex);
                    }
                    else if(type_flag == vierkant::Material::TextureType::Normal)
                    {
                        material.normalmap_index = normalmaps.size();
                        normalmaps.push_back(tex);
                    }
                    else if(type_flag == vierkant::Material::TextureType::Emission)
                    {
                        material.emission_index = emissions.size();
                        emissions.push_back(tex);
                    }
                    else if(type_flag == vierkant::Material::TextureType::Ao_rough_metal)
                    {
                        material.ao_rough_metal_index = ao_rough_metal_maps.size();
                        ao_rough_metal_maps.push_back(tex);
                    }
                }
                materials.push_back(material);
            }

            RayBuilder::entry_t top_level_entry = {};
            top_level_entry.transform = transform;
            top_level_entry.texture_matrix = mesh_material->texture_transform;
            top_level_entry.buffer_index = mesh_buffer_indices[vertex_buffer_address];
            top_level_entry.material_index = material_indices[mesh_material];
            top_level_entry.vertex_offset = mesh_entry.vertex_offset;
            top_level_entry.base_index = lod.base_index;
            entries.push_back(top_level_entry);
        }
    }

    // avoid passing zero-length buffer-arrays
    if(entries.empty())
    {
        entries.resize(1);
        materials.resize(1);
        vertex_buffers.push_back(m_placeholder_buffer);
        index_buffers.push_back(m_placeholder_buffer);
    }

    // put instances into host-visible gpu-buffer
    auto instance_buffer =
            vierkant::Buffer::create(m_device, instances,
                                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                             VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                     VMA_MEMORY_USAGE_CPU_TO_GPU);

    VkDeviceOrHostAddressConstKHR instance_data_device_address{};
    instance_data_device_address.deviceAddress = instance_buffer->device_address();

    VkAccelerationStructureGeometryKHR acceleration_structure_geometry = {};
    acceleration_structure_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    acceleration_structure_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    acceleration_structure_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    acceleration_structure_geometry.geometry.instances.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    acceleration_structure_geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    acceleration_structure_geometry.geometry.instances.data = instance_data_device_address;

    uint32_t num_primitives = instances.size();

    // The pSrcAccelerationStructure, dstAccelerationStructure, and mode members of pBuildInfo are ignored.
    // Any VkDeviceOrHostAddressKHR members of pBuildInfo are ignored by this command,
    // except that the hostAddress member of VkAccelerationStructureGeometryTrianglesDataKHR::transformData
    // will be examined to check if it is NULL.*
    VkAccelerationStructureBuildGeometryInfoKHR acceleration_structure_build_geometry_info{};
    acceleration_structure_build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    acceleration_structure_build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    acceleration_structure_build_geometry_info.flags = build_flags;
    acceleration_structure_build_geometry_info.geometryCount = 1;
    acceleration_structure_build_geometry_info.pGeometries = &acceleration_structure_geometry;

    VkAccelerationStructureBuildSizesInfoKHR acceleration_structure_build_sizes_info{};
    acceleration_structure_build_sizes_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(m_device->handle(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &acceleration_structure_build_geometry_info, &num_primitives,
                                            &acceleration_structure_build_sizes_info);

    // create the top-level structure
    VkAccelerationStructureCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    create_info.size = acceleration_structure_build_sizes_info.accelerationStructureSize;

    // create/collect our stuff
    auto top_level = create_acceleration_asset(create_info);

    // needed to access buffer/vertex/index/material in closest-hit shader
    top_level.entry_buffer = vierkant::Buffer::create(m_device, entries, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                      VMA_MEMORY_USAGE_CPU_TO_GPU);

    // material information for all entries
    top_level.material_buffer = vierkant::Buffer::create(m_device, materials, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                         VMA_MEMORY_USAGE_CPU_TO_GPU);

    // move texture-assets
    top_level.textures = std::move(textures);
    top_level.normalmaps = std::move(normalmaps);
    top_level.emissions = std::move(emissions);
    top_level.ao_rough_metal_maps = std::move(ao_rough_metal_maps);

    // move buffers
    top_level.vertex_buffers = std::move(vertex_buffers);
    top_level.vertex_buffer_offsets = std::move(vertex_buffer_offsets);
    top_level.index_buffers = std::move(index_buffers);
    top_level.index_buffer_offsets = std::move(index_buffer_offsets);

    // Create a small scratch buffer used during build of the top level acceleration structure
    auto scratch_buffer =
            vierkant::Buffer::create(m_device, nullptr, acceleration_structure_build_sizes_info.buildScratchSize,
                                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VMA_MEMORY_USAGE_GPU_ONLY, m_memory_pool);

    VkAccelerationStructureBuildGeometryInfoKHR acceleration_build_geometry_info{};
    acceleration_build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    acceleration_build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    acceleration_build_geometry_info.flags = build_flags;
    acceleration_build_geometry_info.mode =
            last ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    acceleration_build_geometry_info.srcAccelerationStructure = last.get();
    acceleration_build_geometry_info.dstAccelerationStructure = top_level.structure.get();
    acceleration_build_geometry_info.geometryCount = 1;
    acceleration_build_geometry_info.pGeometries = &acceleration_structure_geometry;
    acceleration_build_geometry_info.scratchData.deviceAddress = scratch_buffer->device_address();

    VkAccelerationStructureBuildRangeInfoKHR acceleration_structure_build_range_info{};
    acceleration_structure_build_range_info.primitiveCount = instances.size();
    acceleration_structure_build_range_info.primitiveOffset = 0;
    acceleration_structure_build_range_info.firstVertex = 0;
    acceleration_structure_build_range_info.transformOffset = 0;

    // keep-alives
    top_level.update_structure = last;
    top_level.instance_buffer = instance_buffer;
    top_level.scratch_buffer = scratch_buffer;

    vierkant::CommandBuffer local_commandbuffer;

    if(commandbuffer == VK_NULL_HANDLE)
    {
        local_commandbuffer = vierkant::CommandBuffer(m_device, m_command_pool.get());
        local_commandbuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        commandbuffer = local_commandbuffer.handle();
    }

    // build the AS
    const VkAccelerationStructureBuildRangeInfoKHR *offset_ptr = &acceleration_structure_build_range_info;
    vkCmdBuildAccelerationStructuresKHR(commandbuffer, 1, &acceleration_build_geometry_info, &offset_ptr);

    // submit only if we created the command buffer
    if(local_commandbuffer) { local_commandbuffer.submit(m_queue, true); }

    return top_level;
}

}//namespace vierkant
