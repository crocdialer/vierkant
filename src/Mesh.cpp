//
// Created by crocdialer on 2/28/19.
//

#include <map>
#include <set>

#include "vierkant/Mesh.hpp"

namespace vierkant {

///////////////////////////////////////////////////////////////////////////////////////////////////

template<>
VkIndexType index_type<uint16_t>() { return VK_INDEX_TYPE_UINT16; }

template<>
VkIndexType index_type<uint32_t>() { return VK_INDEX_TYPE_UINT32; }

///////////////////////////////////////////////////////////////////////////////////////////////////

template<>
VkFormat format<uint8_t>() { return VK_FORMAT_R8_UNORM; }

template<>
VkFormat format<float>() { return VK_FORMAT_R32_SFLOAT; }

template<>
VkFormat format<glm::vec2>() { return VK_FORMAT_R32G32_SFLOAT; }

template<>
VkFormat format<glm::vec3>() { return VK_FORMAT_R32G32B32_SFLOAT; }

template<>
VkFormat format<glm::vec4>() { return VK_FORMAT_R32G32B32A32_SFLOAT; }

template<>
VkFormat format<int32_t>() { return VK_FORMAT_R32_SINT; }

template<>
VkFormat format<glm::ivec2>() { return VK_FORMAT_R32G32_SINT; }

template<>
VkFormat format<glm::ivec3>() { return VK_FORMAT_R32G32B32_SINT; }

template<>
VkFormat format<glm::ivec4>() { return VK_FORMAT_R32G32B32A32_SINT; }

template<>
VkFormat format<uint32_t>() { return VK_FORMAT_R32_UINT; }

template<>
VkFormat format<glm::uvec2>() { return VK_FORMAT_R32G32_UINT; }

template<>
VkFormat format<glm::uvec3>() { return VK_FORMAT_R32G32B32_UINT; }

template<>
VkFormat format<glm::uvec4>() { return VK_FORMAT_R32G32B32A32_UINT; }

///////////////////////////////////////////////////////////////////////////////////////////////////

void add_descriptor_counts(const std::vector<descriptor_t> &descriptors, descriptor_count_t &counts)
{
    std::map<VkDescriptorType, uint32_t> mesh_counts;
    for(const auto &desc : descriptors){ mesh_counts[desc.type]++; }
    for(const auto &pair : mesh_counts){ counts.push_back(pair); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorPoolPtr create_descriptor_pool(const vierkant::DevicePtr &device,
                                         const descriptor_count_t &counts,
                                         uint32_t max_sets)
{
    std::vector<VkDescriptorPoolSize> pool_sizes;
    for(const auto &pair : counts){ pool_sizes.push_back({pair.first, pair.second}); }

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = max_sets;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    vkCheck(vkCreateDescriptorPool(device->handle(), &pool_info, nullptr, &descriptor_pool),
            "failed to create descriptor pool!");
    return DescriptorPoolPtr(descriptor_pool, [device](VkDescriptorPool p)
    {
        vkDestroyDescriptorPool(device->handle(), p, nullptr);
    });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorSetLayoutPtr create_descriptor_set_layout(const vierkant::DevicePtr &device,
                                                    const std::vector<descriptor_t> &descriptors)
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for(const auto &desc : descriptors)
    {
        VkDescriptorSetLayoutBinding ubo_layout_binding = {};
        ubo_layout_binding.binding = desc.binding;
        ubo_layout_binding.descriptorCount = std::max<uint32_t>(1, static_cast<uint32_t>(desc.image_samplers.size()));
        ubo_layout_binding.descriptorType = desc.type;
        ubo_layout_binding.pImmutableSamplers = nullptr;
        ubo_layout_binding.stageFlags = desc.stage_flags;
        bindings.push_back(ubo_layout_binding);
    }
    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    vkCheck(vkCreateDescriptorSetLayout(device->handle(), &layout_info, nullptr, &descriptor_set_layout),
            "failed to create descriptor set layout!");

    return DescriptorSetLayoutPtr(descriptor_set_layout, [device](VkDescriptorSetLayout dl)
    {
        vkDestroyDescriptorSetLayout(device->handle(), dl, nullptr);
    });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorSetPtr create_descriptor_set(const vierkant::DevicePtr &device,
                                       const DescriptorPoolPtr &pool,
                                       const DescriptorSetLayoutPtr &layout,
                                       const std::vector<descriptor_t> &descriptors)
{
    size_t num_writes = 0;

    for(const auto &desc : descriptors)
    {
        num_writes += std::max<size_t>(1, desc.image_samplers.size());
    }

    VkDescriptorSet descriptor_set;
    VkDescriptorSetLayout layout_handle = layout.get();

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = pool.get();
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout_handle;

    vkCheck(vkAllocateDescriptorSets(device->handle(), &alloc_info, &descriptor_set),
            "failed to allocate descriptor sets!");

    std::vector<VkWriteDescriptorSet> descriptor_writes;

    std::vector<VkDescriptorBufferInfo> buffer_infos;
    buffer_infos.reserve(num_writes);

    // keep all VkDescriptorImageInfo structs around until vkUpdateDescriptorSets has processed them
    std::vector<std::vector<VkDescriptorImageInfo>> image_infos_collection;

    for(const auto &desc : descriptors)
    {
        VkWriteDescriptorSet desc_write = {};
        desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_write.descriptorType = desc.type;
        desc_write.dstSet = descriptor_set;
        desc_write.dstBinding = desc.binding;
        desc_write.dstArrayElement = 0;
        desc_write.descriptorCount = 1;

        if(desc.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        {
            VkDescriptorBufferInfo buffer_info = {};
            buffer_info.buffer = desc.buffer->handle();
            buffer_info.offset = desc.buffer_offset;
            buffer_info.range = desc.buffer->num_bytes();
            buffer_infos.push_back(buffer_info);
            desc_write.pBufferInfo = &buffer_infos.back();
        }else if(desc.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            std::vector<VkDescriptorImageInfo> image_infos(desc.image_samplers.size());

            for(uint32_t j = 0; j < desc.image_samplers.size(); ++j)
            {
                const auto &img = desc.image_samplers[j];
                image_infos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;//img->image_layout();
                image_infos[j].imageView = img->image_view();
                image_infos[j].sampler = img->sampler();
            }
            desc_write.descriptorCount = static_cast<uint32_t>(image_infos.size());
            desc_write.pImageInfo = image_infos.data();
            image_infos_collection.push_back(std::move(image_infos));
        }
        descriptor_writes.push_back(desc_write);
    }

    // write all descriptors
    vkUpdateDescriptorSets(device->handle(), descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);

    return DescriptorSetPtr(descriptor_set, [device, pool](VkDescriptorSet s)
    {
        vkFreeDescriptorSets(device->handle(), pool.get(), 1, &s);
    });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void bind_buffers(VkCommandBuffer command_buffer, const MeshConstPtr &mesh)
{
    buffer_binding_set_t buf_tuples;
    for(auto &att : mesh->vertex_attribs)
    {
        buf_tuples.insert(std::make_tuple(att.buffer, att.buffer_offset, att.stride, att.input_rate));
    }

    std::vector<VkBuffer> buf_handles;
    std::vector<VkDeviceSize> offsets;

    for(const auto &tuple : buf_tuples)
    {
        buf_handles.push_back(std::get<0>(tuple)->handle());
        offsets.push_back(std::get<1>(tuple));
    }

    // bind vertex buffer
    vkCmdBindVertexBuffers(command_buffer, 0, static_cast<uint32_t>(buf_handles.size()), buf_handles.data(),
                           offsets.data());

    // bind index buffer
    if(mesh->index_buffer)
    {
        vkCmdBindIndexBuffer(command_buffer, mesh->index_buffer->handle(), mesh->index_buffer_offset, mesh->index_type);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<VkVertexInputAttributeDescription> attribute_descriptions(const MeshConstPtr &mesh)
{
    buffer_binding_set_t buf_tuples;
    for(auto &att : mesh->vertex_attribs)
    {
        buf_tuples.insert(std::make_tuple(att.buffer, att.buffer_offset, att.stride, att.input_rate));
    }

    auto binding_index = [](const Mesh::VertexAttrib &a, const buffer_binding_set_t &bufs) -> int32_t
    {
        uint32_t i = 0;
        for(const auto &t : bufs)
        {
            if(t == std::make_tuple(a.buffer, a.buffer_offset, a.stride, a.input_rate)){ return i; }
            i++;
        }
        return -1;
    };

    std::vector<VkVertexInputAttributeDescription> ret;

    for(const auto &att_in : mesh->vertex_attribs)
    {
        auto binding = binding_index(att_in, buf_tuples);

        if(binding >= 0 && att_in.location >= 0)
        {
            VkVertexInputAttributeDescription att;
            att.offset = att_in.offset;
            att.binding = static_cast<uint32_t>(binding);
            att.location = static_cast<uint32_t>(att_in.location);
            att.format = att_in.format;
            ret.push_back(att);
        }
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<VkVertexInputBindingDescription> binding_descriptions(const MeshConstPtr &mesh)
{
    buffer_binding_set_t buf_tuples;
    for(auto &att : mesh->vertex_attribs)
    {
        buf_tuples.insert(std::make_tuple(att.buffer, att.buffer_offset, att.stride, att.input_rate));
    }
    std::vector<VkVertexInputBindingDescription> ret;
    uint32_t i = 0;

    for(const auto &tuple : buf_tuples)
    {
        VkVertexInputBindingDescription desc;
        desc.binding = i++;
        desc.stride = std::get<2>(tuple);
        desc.inputRate = std::get<3>(tuple);
        ret.push_back(desc);
    }
    return ret;
}

vierkant::MeshPtr
create_mesh_from_geometry(const vierkant::DevicePtr &device, const GeometryConstPtr &geom, bool interleave_data)
{
    struct vertex_data_t
    {
        const uint8_t *data;
        size_t offset;
        size_t elem_size;
        size_t num_elems;
        VkFormat format;
    };
    std::vector<vertex_data_t> vertex_data;

    auto add_offset = [](const auto &array, std::vector<vertex_data_t> &vertex_data, size_t &offset, size_t &stride,
                         size_t &num_bytes)
    {
        if(!array.empty())
        {
            using elem_t = typename std::decay<decltype(array)>::type::value_type;
            size_t elem_size = sizeof(elem_t);
            vertex_data.push_back(
                    {(uint8_t *)array.data(), offset, static_cast<uint32_t>(elem_size), array.size(),
                     vierkant::format<elem_t>()});
            offset += elem_size;
            stride += elem_size;
            num_bytes += array.size() * elem_size;
        }
    };

    size_t stride = 0, num_bytes = 0;

    // sanity check array sizes
    auto is_sane = [add_offset, &stride, &num_bytes](const GeometryConstPtr &g,
                                                     std::vector<vertex_data_t> &vertex_data) -> bool
    {
        size_t offset = 0;
        auto num_vertices = g->vertices.size();

        if(g->vertices.empty()){ return false; }
        add_offset(g->vertices, vertex_data, offset, stride, num_bytes);

        if(!g->colors.empty() && g->colors.size() != num_vertices){ return false; }
        add_offset(g->colors, vertex_data, offset, stride, num_bytes);

        if(!g->tex_coords.empty() && g->tex_coords.size() != num_vertices){ return false; }
        add_offset(g->tex_coords, vertex_data, offset, stride, num_bytes);

        if(!g->normals.empty() && g->normals.size() != num_vertices){ return false; }
        add_offset(g->normals, vertex_data, offset, stride, num_bytes);

        if(!g->tangents.empty() && g->tangents.size() != num_vertices){ return false; }
        add_offset(g->tangents, vertex_data, offset, stride, num_bytes);

        return true;
    };
    if(!is_sane(geom, vertex_data))
    {
        LOG_WARNING << "create_mesh_from_geometry: array sizes do not match";
        return nullptr;
    }

    auto mesh = vierkant::Mesh::create();

    // combine buffers into staging buffer
    auto stage_buffer = vierkant::Buffer::create(device, nullptr, num_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                 VMA_MEMORY_USAGE_CPU_ONLY);

    // create vertexbuffer
    auto vertex_buffer = vierkant::Buffer::create(device, nullptr, num_bytes,
                                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                  VMA_MEMORY_USAGE_GPU_ONLY);
    auto staging_data = (uint8_t *)stage_buffer->map();

    if(interleave_data)
    {
        for(uint32_t i = 0; i < vertex_data.size(); ++i)
        {
            const auto &v = vertex_data[i];

            for(uint32_t j = 0; j < v.num_elems; ++j)
            {
                memcpy(staging_data + v.offset + j * stride, v.data + j * v.elem_size, v.elem_size);
            }

            vierkant::Mesh::VertexAttrib attrib;
            attrib.location = i;
            attrib.offset = v.offset;
            attrib.stride = static_cast<uint32_t>(stride);
            attrib.buffer = vertex_buffer;
            attrib.buffer_offset = 0;
            attrib.format = v.format;
            mesh->vertex_attribs.push_back(attrib);
        }
    }else
    {
        size_t offset = 0;
        auto insert_data = [&mesh, &staging_data, &offset, &vertex_buffer](const auto &array, uint32_t location)
        {
            using elem_t = typename std::decay<decltype(array)>::type::value_type;

            if(!array.empty())
            {
                size_t value_size = sizeof(elem_t);
                size_t num_bytes = array.size() * value_size;
                memcpy(staging_data + offset, array.data(), num_bytes);

                vierkant::Mesh::VertexAttrib attrib;
                attrib.location = location;
                attrib.offset = 0;
                attrib.stride = static_cast<uint32_t>(value_size);
                attrib.buffer = vertex_buffer;
                attrib.buffer_offset = offset;
                attrib.format = vierkant::format<elem_t>();
                mesh->vertex_attribs.push_back(attrib);
                offset += num_bytes;
            }
        };

        // vertex attributes
        insert_data(geom->vertices, 0);
        insert_data(geom->colors, 1);
        insert_data(geom->tex_coords, 2);
        insert_data(geom->normals, 3);
        insert_data(geom->tangents, 4);
    }

    // copy combined vertex data to device-buffer
    stage_buffer->copy_to(vertex_buffer);

    // use indices
    if(!geom->indices.empty())
    {
        mesh->index_buffer = vierkant::Buffer::create(device, geom->indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                      VMA_MEMORY_USAGE_GPU_ONLY);
        mesh->num_elements = static_cast<uint32_t>(geom->indices.size());
    }else{ mesh->num_elements = static_cast<uint32_t>(geom->vertices.size()); }

    // set topology
    mesh->topology = geom->topology;
    return mesh;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

MeshPtr Mesh::create()
{
    auto ret = MeshPtr(new Mesh());
    ret->set_name("mesh_" + std::to_string(ret->id()));
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

}//namespace vierkant
