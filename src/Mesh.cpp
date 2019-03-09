//
// Created by crocdialer on 2/28/19.
//

#include <map>
#include <set>

#include "../include/vierkant/Mesh.hpp"

namespace vierkant {

///////////////////////////////////////////////////////////////////////////////////////////////////

template<>
VkIndexType index_type<uint16_t>() { return VK_INDEX_TYPE_UINT16; }

template<>
VkIndexType index_type<uint32_t>() { return VK_INDEX_TYPE_UINT32; }

///////////////////////////////////////////////////////////////////////////////////////////////////

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

void add_descriptor_counts(const MeshConstPtr &mesh, descriptor_count_map_t &counts)
{
    for(const auto &desc : mesh->descriptors){ counts[desc.type]++; }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorPoolPtr create_descriptor_pool(const vierkant::DevicePtr &device, const descriptor_count_map_t &counts)
{
    std::vector<VkDescriptorPoolSize> pool_sizes;
    for(const auto &pair : counts){ pool_sizes.push_back({pair.first, pair.second}); }

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = static_cast<uint32_t>(10);
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

DescriptorSetLayoutPtr create_descriptor_set_layout(const vierkant::DevicePtr &device, const MeshConstPtr &mesh)
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for(const auto &desc : mesh->descriptors)
    {
        VkDescriptorSetLayoutBinding ubo_layout_binding = {};
        ubo_layout_binding.binding = desc.binding;
        ubo_layout_binding.descriptorCount = 1;
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

std::vector<DescriptorSetPtr> create_descriptor_sets(const vierkant::DevicePtr &device,
                                                     const DescriptorPoolPtr &pool,
                                                     const MeshConstPtr &mesh)
{
    size_t num_sets = 1;
    for(const auto &desc : mesh->descriptors){ num_sets = std::max(num_sets, desc.buffers.size()); }

    std::vector<VkDescriptorSet> descriptor_sets(num_sets);
    std::vector<VkDescriptorSetLayout> layouts(num_sets, mesh->descriptor_set_layout.get());

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = pool.get();
    alloc_info.descriptorSetCount = static_cast<uint32_t>(num_sets);
    alloc_info.pSetLayouts = layouts.data();

    vkCheck(vkAllocateDescriptorSets(device->handle(), &alloc_info, descriptor_sets.data()),
            "failed to allocate descriptor sets!");

    size_t num_writes = num_sets * mesh->descriptors.size();

    std::vector<VkWriteDescriptorSet> descriptor_writes;
    std::vector<VkDescriptorBufferInfo> buffer_infos;
    buffer_infos.reserve(num_writes);
    std::vector<VkDescriptorImageInfo> image_infos;
    image_infos.reserve(num_writes);

    for(size_t i = 0; i < num_sets; i++)
    {
        for(const auto &desc : mesh->descriptors)
        {
            VkWriteDescriptorSet desc_write = {};
            desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            desc_write.descriptorType = desc.type;
            desc_write.dstSet = descriptor_sets[i];
            desc_write.dstBinding = desc.binding;
            desc_write.dstArrayElement = 0;
            desc_write.descriptorCount = 1;

            if(desc.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            {
                VkDescriptorBufferInfo buffer_info = {};
                buffer_info.buffer = desc.buffers[i]->handle();
                buffer_info.offset = desc.buffer_offset;
                buffer_info.range = desc.buffers[i]->num_bytes();
                buffer_infos.push_back(buffer_info);
                desc_write.pBufferInfo = &buffer_infos.back();
            }else if(desc.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            {
                VkDescriptorImageInfo image_info = {};
                image_info.imageLayout = desc.image->image_layout();
                image_info.imageView = desc.image->image_view();
                image_info.sampler = desc.image->sampler();
                image_infos.push_back(image_info);
                desc_write.pImageInfo = &image_infos.back();
            }
            descriptor_writes.push_back(desc_write);
        }
    }
    // write all descriptors
    vkUpdateDescriptorSets(device->handle(), descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);

    std::vector<DescriptorSetPtr> ret;
    for(auto ds : descriptor_sets)
    {
        DescriptorSetPtr ptr(ds, [device, pool](VkDescriptorSet s)
        {
            vkFreeDescriptorSets(device->handle(), pool.get(), 1, &s);
        });
        ret.push_back(std::move(ptr));
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void bind_buffers(VkCommandBuffer command_buffer, const MeshConstPtr &mesh)
{
    std::map<vierkant::BufferPtr, VkDeviceSize> buf_map;
    for(auto &att : mesh->vertex_attribs){ buf_map[att.buffer] = att.buffer_offset; }

    std::vector<VkBuffer> buf_handles;
    std::vector<VkDeviceSize> offsets;

    for(const auto &pair : buf_map)
    {
        buf_handles.push_back(pair.first->handle());
        offsets.push_back(pair.second);
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

    std::set<vierkant::BufferPtr> bufs;
    for(auto &att : mesh->vertex_attribs){ bufs.insert(att.buffer); }

    auto binding_index = [](const Mesh::VertexAttrib &attrib, const std::set<vierkant::BufferPtr> &bufs) -> int32_t
    {
        uint32_t i = 0;
        for(const auto &b : bufs){ if(attrib.buffer == b){ return i; }}
        return -1;
    };

    std::vector<VkVertexInputAttributeDescription> ret;

    for(const auto &att_in : mesh->vertex_attribs)
    {
        auto binding = binding_index(att_in, bufs);

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
    std::map<vierkant::BufferPtr, uint32_t> buf_strides;
    for(auto &att : mesh->vertex_attribs){ buf_strides.insert(std::make_pair(att.buffer, att.stride)); }
    std::vector<VkVertexInputBindingDescription> ret;
    uint32_t i = 0;

    for(const auto &pair : buf_strides)
    {
        VkVertexInputBindingDescription desc;
        desc.binding = i++;;
        desc.stride = pair.second;
        desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        ret.push_back(desc);
    }
    return ret;
}

vierkant::MeshPtr create_mesh_from_geometry(const vierkant::DevicePtr &device, const Geometry &geom)
{
    // sanity check array sizes
    auto is_sane = [](const Geometry &g) -> bool
    {
        if(g.vertices.empty()){ return false; }
        auto num_vertices = g.vertices.size();
        if(!g.tex_coords.empty() && g.tex_coords.size() != num_vertices){ return false; }
        if(!g.colors.empty() && g.colors.size() != num_vertices){ return false; }
        if(!g.normals.empty() && g.normals.size() != num_vertices){ return false; }
        if(!g.tangents.empty() && g.tangents.size() != num_vertices){ return false; }
        return true;
    };
    if(!is_sane(geom))
    {
        LOG_WARNING << "create_mesh_from_geometry: array size do not match";
        return nullptr;
    }

    auto mesh = vierkant::Mesh::create();

    auto num_vertex_bytes = [](const Geometry &g) -> size_t
    {
        size_t num_bytes = 0;
        num_bytes += g.vertices.size() * sizeof(decltype(g.vertices)::value_type);
        num_bytes += g.tex_coords.size() * sizeof(decltype(g.tex_coords)::value_type);
        num_bytes += g.colors.size() * sizeof(decltype(g.colors)::value_type);
        num_bytes += g.normals.size() * sizeof(decltype(g.normals)::value_type);
        num_bytes += g.tangents.size() * sizeof(decltype(g.tangents)::value_type);
        return num_bytes;
    };

    // TODO: create vertexbuffer
    size_t num_bytes = num_vertex_bytes(geom);

    // TODO: combine buffers
    // vertex attributes
    auto vertex_buffer = vierkant::Buffer::create(device, nullptr, num_bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

//    vierkant::Mesh::VertexAttrib position, color, tex_coord;
//    position.location = 0;
//    position.offset = offsetof(Vertex, position);
//    position.stride = sizeof(Vertex);
//    position.buffer = vertex_buffer;
//    position.format = vk::format<decltype(Vertex::position)>();
//    mesh->vertex_attribs.push_back(position);
//
//    color.location = 1;
//    color.offset = offsetof(Vertex, color);
//    color.stride = sizeof(Vertex);
//    color.buffer = vertex_buffer;
//    color.format = vk::format<decltype(Vertex::color)>();
//    mesh->vertex_attribs.push_back(color);
//
//    tex_coord.location = 2;
//    tex_coord.offset = offsetof(Vertex, tex_coord);
//    tex_coord.stride = sizeof(Vertex);
//    tex_coord.buffer = vertex_buffer;
//    tex_coord.format = vk::format<decltype(Vertex::tex_coord)>();
//    mesh->vertex_attribs.push_back(tex_coord);

    mesh->index_buffer = vierkant::Buffer::create(device, geom.indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    return mesh;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

MeshPtr Mesh::create() { return MeshPtr(new Mesh()); }

///////////////////////////////////////////////////////////////////////////////////////////////////

}//namespace vierkant