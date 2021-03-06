//
// Created by crocdialer on 11/15/20.
//

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

QueryPoolPtr create_query_pool(const vierkant::DevicePtr &device, uint32_t query_count, VkQueryType query_type)
{
    VkQueryPoolCreateInfo pool_create_info = {};
    pool_create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;

    pool_create_info.queryCount = query_count;
    pool_create_info.queryType = query_type;

    VkQueryPool handle = VK_NULL_HANDLE;
    vkCheck(vkCreateQueryPool(device->handle(), &pool_create_info, nullptr, &handle),
            "could not create VkQueryPool");
    return QueryPoolPtr(handle, [device](VkQueryPool p){ vkDestroyQueryPool(device->handle(), p, nullptr); });
}

RayBuilder::RayBuilder(const vierkant::DevicePtr &device) :
        m_device(device)
{
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
}

void RayBuilder::add_mesh(const vierkant::MeshConstPtr &mesh, const glm::mat4 &transform)
{
    auto search_it = m_acceleration_assets.find(mesh);

    if(search_it != m_acceleration_assets.end())
    {
        for(auto &asset : search_it->second){ asset.transform = transform; }
        return;
    }

    const auto &vertex_attrib = mesh->vertex_attribs.at(vierkant::Mesh::AttribLocation::ATTRIB_POSITION);
    VkDeviceAddress vertex_base_address = vertex_attrib.buffer->device_address() + vertex_attrib.offset;
    VkDeviceAddress index_base_address = mesh->index_buffer->device_address();

    // raytracing flags
    VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                                                 VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;

    // compaction requested?
    bool enable_compaction = (flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR)
                             == VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;

    std::vector<VkAccelerationStructureGeometryKHR> geometries(mesh->entries.size());
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> offsets(mesh->entries.size());
    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> build_infos(mesh->entries.size());

    // all-in, one scratch buffer per entry is memory-intense but fast
    std::vector<vierkant::BufferPtr> scratch_buffers(mesh->entries.size());

    // one per bottom-lvl-build
    std::vector<vierkant::CommandBuffer> command_buffers(mesh->entries.size());

    // used to query compaction sizes after building
    auto query_pool = create_query_pool(m_device, mesh->entries.size(),
                                        VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR);

    // those will be stored
    std::vector<acceleration_asset_t> entry_assets(mesh->entries.size());

    for(uint32_t i = 0; i < mesh->entries.size(); ++i)
    {
        const auto &entry = mesh->entries[i];

        const auto &material = mesh->materials[entry.material_index];

        // throw on non-triangle entries
        if(entry.primitive_type != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        {
            throw std::runtime_error("RaytracingPipeline::add_mesh: provided non-triangle entry");
        }

        VkAccelerationStructureGeometryTrianglesDataKHR triangles = {};
        triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        triangles.indexType = mesh->index_type;
        triangles.indexData.deviceAddress = index_base_address + entry.base_index * sizeof(index_t);
        triangles.vertexFormat = vertex_attrib.format;
        triangles.vertexData.deviceAddress = vertex_base_address + entry.base_vertex * vertex_attrib.stride;
        triangles.vertexStride = vertex_attrib.stride;
        triangles.maxVertex = entry.num_vertices;
        triangles.transformData = {};

        auto &geometry = geometries[i];
        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.flags = material->blending ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.geometry.triangles = triangles;

        // offsets
        auto &offset = offsets[i];
        offset.firstVertex = 0;
        offset.primitiveOffset = 0;
        offset.primitiveCount = entry.num_indices / 3;

        auto &build_info = build_infos[i];
        build_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        build_info.flags = flags;
        build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        build_info.geometryCount = 1;
        build_info.pGeometries = &geometry;
        build_info.srcAccelerationStructure = VK_NULL_HANDLE;

        // query memory requirements
        VkAccelerationStructureBuildSizesInfoKHR size_info = {};
        size_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

        vkGetAccelerationStructureBuildSizesKHR(m_device->handle(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                &build_infos[i], &offsets[i].primitiveCount, &size_info);

        auto &acceleration_asset = entry_assets[i];
        VkAccelerationStructureCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        create_info.size = size_info.accelerationStructureSize;
        acceleration_asset = create_acceleration_asset(create_info);

        // Allocate the scratch buffers holding the temporary data of the
        // acceleration structure builder
        scratch_buffers[i] = vierkant::Buffer::create(m_device, nullptr, size_info.buildScratchSize,
                                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        // assign acceleration structure and scratch_buffer
        build_info.dstAccelerationStructure = acceleration_asset.structure.get();
        build_info.scratchData.deviceAddress = scratch_buffers[i]->device_address();

        // create commandbuffer for building the bottomlevel-structure
        auto &cmd_buffer = command_buffers[i];
        cmd_buffer = vierkant::CommandBuffer(m_device, m_command_pool.get());
        cmd_buffer.begin();

        // build the AS
        const VkAccelerationStructureBuildRangeInfoKHR *offset_ptr = &offset;
        vkCmdBuildAccelerationStructuresKHR(cmd_buffer.handle(), 1, &build_info, &offset_ptr);

        // Write compacted size to query number idx.
        if(enable_compaction)
        {
            // Since the scratch buffer is reused across builds, we need a barrier to ensure one build
            // is finished before starting the next one
            VkMemoryBarrier barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
            vkCmdPipelineBarrier(cmd_buffer.handle(),
                                 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                 0, 1, &barrier, 0, nullptr, 0, nullptr);

            VkAccelerationStructureKHR accel_structure = acceleration_asset.structure.get();
            vkCmdWriteAccelerationStructuresPropertiesKHR(cmd_buffer.handle(), 1, &accel_structure,
                                                          VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                                                          query_pool.get(), i);
        }
        cmd_buffer.end();
    }

    std::vector<VkCommandBuffer> cmd_handles(command_buffers.size());
    for(uint32_t i = 0; i < command_buffers.size(); ++i){ cmd_handles[i] = command_buffers[i].handle(); }

    VkQueue queue = m_device->queue();
    vierkant::submit(m_device, queue, cmd_handles, VK_NULL_HANDLE, true);

    // free scratchbuffer here
    scratch_buffers.clear();

    // memory-compaction for bottom-lvl-structures
    if(enable_compaction)
    {
        auto cmd_buffer = vierkant::CommandBuffer(m_device, m_command_pool.get());
        cmd_buffer.begin();

        // Get the size result back
        std::vector<VkDeviceSize> compact_sizes(mesh->entries.size());
        vkGetQueryPoolResults(m_device->handle(), query_pool.get(), 0,
                              (uint32_t) compact_sizes.size(), compact_sizes.size() * sizeof(VkDeviceSize),
                              compact_sizes.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT);


        // compacting
        std::vector<acceleration_asset_t> entry_assets_compact(entry_assets.size());

        for(uint32_t i = 0; i < entry_assets.size(); i++)
        {
            LOG_DEBUG << crocore::format("reducing bottom-lvl-size (%d), from %d to %d \n", i,
                                         (uint32_t) entry_assets[i].buffer->num_bytes(),
                                         compact_sizes[i]);

            // Creating a compact version of the AS
            VkAccelerationStructureCreateInfoKHR create_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
            create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            create_info.size = compact_sizes[i];
            auto &acceleration_asset = entry_assets_compact[i];
            acceleration_asset = create_acceleration_asset(create_info);
            acceleration_asset.transform = entry_assets[i].transform;

            // copy the original BLAS to a compact version
            VkCopyAccelerationStructureInfoKHR copy_info{VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
            copy_info.src = entry_assets[i].structure.get();
            copy_info.dst = acceleration_asset.structure.get();
            copy_info.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
            vkCmdCopyAccelerationStructureKHR(cmd_buffer.handle(), &copy_info);
        }
        cmd_buffer.submit(m_device->queue(), true);

        // keep compacted versions
        entry_assets = std::move(entry_assets_compact);
    }

    // store bottom-level entries
    if(!entry_assets.empty()){ m_acceleration_assets[mesh] = std::move(entry_assets); }
}

void RayBuilder::set_function_pointers()
{
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(
            m_device->handle(), "vkCmdBuildAccelerationStructuresKHR"));
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(
            m_device->handle(), "vkCreateAccelerationStructureKHR"));
    vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(
            m_device->handle(), "vkDestroyAccelerationStructureKHR"));
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(
            m_device->handle(), "vkGetAccelerationStructureBuildSizesKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(
            m_device->handle(), "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCmdWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(vkGetDeviceProcAddr(
            m_device->handle(), "vkCmdWriteAccelerationStructuresPropertiesKHR"));
    vkCmdCopyAccelerationStructureKHR = reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(vkGetDeviceProcAddr(
            m_device->handle(), "vkCmdCopyAccelerationStructureKHR"));
}

RayBuilder::acceleration_asset_t
RayBuilder::create_acceleration_asset(VkAccelerationStructureCreateInfoKHR create_info,
                                      const glm::mat4 &transform)
{
    RayBuilder::acceleration_asset_t acceleration_asset = {};
    acceleration_asset.buffer = vierkant::Buffer::create(m_device, nullptr, create_info.size,
                                                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                         VMA_MEMORY_USAGE_GPU_ONLY);

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

    // pass transform
    acceleration_asset.transform = transform;

    acceleration_asset.structure = AccelerationStructurePtr(handle, [device = m_device,
            buffer = acceleration_asset.buffer,
            pFn = vkDestroyAccelerationStructureKHR](VkAccelerationStructureKHR s)
    {
        pFn(device->handle(), s, nullptr);
    });
    return acceleration_asset;
}

RayBuilder::acceleration_asset_t RayBuilder::create_toplevel(VkCommandBuffer commandbuffer,
                                                             const vierkant::AccelerationStructurePtr &last)
{
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    std::vector<entry_t> entries;
    std::vector<material_struct_t> materials;

    std::vector<vierkant::ImagePtr> textures = {m_placeholder_solid_white};
    std::vector<vierkant::ImagePtr> normalmaps = {m_placeholder_normalmap};
    std::vector<vierkant::ImagePtr> emissions = {m_placeholder_emission};
    std::vector<vierkant::ImagePtr> ao_rough_metal_maps = {m_placeholder_ao_rough_metal};

    // build flags
    VkBuildAccelerationStructureFlagsKHR build_flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
                                                       VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    // instance flags
    VkGeometryInstanceFlagsKHR instance_flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

    uint32_t mesh_index = 0;

    for(const auto &[mesh, acceleration_assets] : m_acceleration_assets)
    {
        assert(mesh->entries.size() == acceleration_assets.size());

        for(uint i = 0; i < acceleration_assets.size(); ++i)
        {
            const auto &mesh_entry = mesh->entries[i];
            const auto &asset = acceleration_assets[i];

            // skip disabled entries
            if(!mesh_entry.enabled){ continue; }

            auto modelview = asset.transform * mesh_entry.transform;

            // per bottom-lvl instance
            VkAccelerationStructureInstanceKHR instance{};
            instance.transform = vk_transform_matrix(modelview);

            // store next entry-index
            instance.instanceCustomIndex = entries.size();
            instance.mask = 0xFF;
            instance.instanceShaderBindingTableRecordOffset = 0;
            instance.flags = instance_flags;
            instance.accelerationStructureReference = asset.device_address;

            instances.push_back(instance);

            RayBuilder::entry_t top_level_entry = {};
            top_level_entry.modelview = modelview;
            top_level_entry.normal_matrix = glm::inverseTranspose(modelview);
            top_level_entry.buffer_index = mesh_index;
            top_level_entry.material_index = materials.size();
            top_level_entry.base_vertex = mesh_entry.base_vertex;
            top_level_entry.base_index = mesh_entry.base_index;
            entries.push_back(top_level_entry);

            const auto &mesh_material = mesh->materials[mesh_entry.material_index];

            RayBuilder::material_struct_t material = {};
            material.color = mesh_material->color;
            material.emission = mesh_material->emission;
            material.roughness = mesh_material->roughness;
            material.metalness = mesh_material->metalness;

            if(mesh_material->textures.count(vierkant::Material::TextureType::Color))
            {
                material.texture_index = textures.size();
                textures.push_back(mesh_material->textures.at(vierkant::Material::TextureType::Color));
            }
            if(mesh_material->textures.count(vierkant::Material::TextureType::Normal))
            {
                material.normalmap_index = normalmaps.size();
                normalmaps.push_back(mesh_material->textures.at(vierkant::Material::TextureType::Normal));
            }
            if(mesh_material->textures.count(vierkant::Material::TextureType::Emission))
            {
                material.emission_index = emissions.size();
                emissions.push_back(mesh_material->textures.at(vierkant::Material::TextureType::Emission));
            }
            if(mesh_material->textures.count(vierkant::Material::TextureType::Ao_rough_metal))
            {
                material.ao_rough_metal_index = ao_rough_metal_maps.size();
                ao_rough_metal_maps.push_back(mesh_material->textures.at(vierkant::Material::TextureType::Ao_rough_metal));
            }
            materials.push_back(material);
        }
        mesh_index++;
    }

    // put instances into host-visible gpu-buffer
    auto instance_buffer = vierkant::Buffer::create(m_device, instances, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                                    VMA_MEMORY_USAGE_CPU_TO_GPU);

    VkDeviceOrHostAddressConstKHR instance_data_device_address{};
    instance_data_device_address.deviceAddress = instance_buffer->device_address();

    VkAccelerationStructureGeometryKHR acceleration_structure_geometry{};
    acceleration_structure_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    acceleration_structure_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    acceleration_structure_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    acceleration_structure_geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
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
    vkGetAccelerationStructureBuildSizesKHR(m_device->handle(),
                                            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &acceleration_structure_build_geometry_info,
                                            &num_primitives,
                                            &acceleration_structure_build_sizes_info);

    // create the top-level structure
    VkAccelerationStructureCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    create_info.size = acceleration_structure_build_sizes_info.accelerationStructureSize;

    // create/collect our stuff
    auto top_level = create_acceleration_asset(create_info);

    // needed to access buffer/vertex/index/material in closest-hit shader
    top_level.entry_buffer = vierkant::Buffer::create(m_device, entries,
                                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                      VMA_MEMORY_USAGE_CPU_TO_GPU);

    // material information for all entries
    top_level.material_buffer = vierkant::Buffer::create(m_device, materials,
                                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                         VMA_MEMORY_USAGE_CPU_TO_GPU);

    constexpr size_t max_num_textures = 256;
    top_level.textures = std::move(textures);
    top_level.normalmaps = std::move(normalmaps);
    top_level.emissions = std::move(emissions);
    top_level.ao_rough_metal_maps = std::move(ao_rough_metal_maps);

    top_level.textures.resize(max_num_textures, m_placeholder_solid_white);
    top_level.normalmaps.resize(max_num_textures, m_placeholder_normalmap);
    top_level.emissions.resize(max_num_textures, m_placeholder_emission);
    top_level.ao_rough_metal_maps.resize(max_num_textures, m_placeholder_ao_rough_metal);

    //    LOG_DEBUG << top_level.buffer->num_bytes() << " bytes in toplevel";

    // Create a small scratch buffer used during build of the top level acceleration structure
    auto scratch_buffer = vierkant::Buffer::create(m_device, nullptr,
                                                   acceleration_structure_build_sizes_info.buildScratchSize,
                                                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    VkAccelerationStructureBuildGeometryInfoKHR acceleration_build_geometry_info{};
    acceleration_build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    acceleration_build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    acceleration_build_geometry_info.flags = build_flags;
    acceleration_build_geometry_info.mode = last ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
                                                 : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
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
    if(local_commandbuffer){ local_commandbuffer.submit(m_device->queue(), true); }

    return top_level;
}

}//namespace vierkant
