//
// Created by crocdialer on 4/22/21.
//

#include <vierkant/Compute.hpp>

namespace vierkant
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Compute::Compute(Compute &&other) noexcept:
        Compute()
{
    swap(*this, other);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Compute &Compute::operator=(Compute other)
{
    swap(*this, other);
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void swap(Compute &lhs, Compute &rhs) noexcept
{
    if(&lhs == &rhs){ return; }

    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_command_pool, rhs.m_command_pool);
    std::swap(lhs.m_descriptor_pool, rhs.m_descriptor_pool);
    std::swap(lhs.m_pipeline_cache, rhs.m_pipeline_cache);
    std::swap(lhs.m_compute_assets, rhs.m_compute_assets);
    std::swap(lhs.m_current_index, rhs.m_current_index);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Compute::Compute(const vierkant::DevicePtr &device, const create_info_t &create_info) :
        m_device(device),
        m_pipeline_cache(create_info.pipeline_cache)
{
    if(!m_pipeline_cache){ m_pipeline_cache = vierkant::PipelineCache::create(device); }

    m_compute_assets.resize(create_info.num_frames_in_flight);

    m_command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                   VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    // we also need a DescriptorPool ...
    vierkant::descriptor_count_t descriptor_counts = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          256},
                                                      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         128},
                                                      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         256},
                                                      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256}};
    m_descriptor_pool = vierkant::create_descriptor_pool(m_device, descriptor_counts, 512);
}

void Compute::dispatch(computable_t computable, VkCommandBuffer commandbuffer)
{
    auto &compute_asset = m_compute_assets[m_current_index];
    m_current_index = (m_current_index + 1) % m_compute_assets.size();

    auto descriptor_set_layout = vierkant::find_set_layout(m_device, computable.descriptors, m_descriptor_set_layouts);
    computable.pipeline_info.descriptor_set_layouts = {descriptor_set_layout.get()};

    if(!computable.push_constants.empty())
    {
        // push constant range
        VkPushConstantRange push_constant_range = {};
        push_constant_range.offset = 0;
        push_constant_range.size = computable.push_constants.size();
        push_constant_range.stageFlags = VK_SHADER_STAGE_ALL;
        computable.pipeline_info.push_constant_ranges = {push_constant_range};
    }

    // create or retrieve an existing raytracing pipeline
    auto pipeline = m_pipeline_cache->pipeline(computable.pipeline_info);

    // fetch descriptor set
    DescriptorSetPtr descriptor_set;
    try{ descriptor_set = compute_asset.descriptor_sets.get(descriptor_set_layout); }
    catch(std::out_of_range &e)
    {
        descriptor_set = compute_asset.descriptor_sets.put(descriptor_set_layout,
                                                           vierkant::create_descriptor_set(m_device, m_descriptor_pool,
                                                                                           descriptor_set_layout,
                                                                                           false));
    }
    // update descriptor-set with actual descriptors
    vierkant::update_descriptor_set(m_device, descriptor_set, computable.descriptors);

    VkDescriptorSet descriptor_set_handle = descriptor_set.get();

    vierkant::CommandBuffer local_commandbuffer;

    if(commandbuffer == VK_NULL_HANDLE)
    {
        local_commandbuffer = vierkant::CommandBuffer(m_device, m_command_pool.get());
        local_commandbuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        commandbuffer = local_commandbuffer.handle();
    }

    // bind compute pipeline
    pipeline->bind(commandbuffer);

    // bind descriptor set (acceleration-structure, uniforms, storage-buffers, samplers, storage-image)
    vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->layout(),
                            0, 1, &descriptor_set_handle, 0, nullptr);

    if(!computable.push_constants.empty())
    {
        // update push_constants
        vkCmdPushConstants(commandbuffer, pipeline->layout(), VK_SHADER_STAGE_ALL, 0,
                           computable.push_constants.size(), computable.push_constants.data());
    }

    // dispatch compute-operation
    vkCmdDispatch(commandbuffer, computable.extent.width, computable.extent.height, computable.extent.depth);

    // keep-alive copy of computable
    compute_asset.computable = std::move(computable);

    // submit only if we created the command buffer
    if(local_commandbuffer){ local_commandbuffer.submit(m_device->queue(), true); }
}

}//namespace vierkant