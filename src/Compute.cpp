#include <vierkant/Compute.hpp>

namespace vierkant
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Compute::Compute(Compute &&other) noexcept : Compute() { swap(*this, other); }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Compute &Compute::operator=(Compute other)
{
    swap(*this, other);
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void swap(Compute &lhs, Compute &rhs) noexcept
{
    if(&lhs == &rhs) { return; }

    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_descriptor_pool, rhs.m_descriptor_pool);
    std::swap(lhs.m_pipeline_cache, rhs.m_pipeline_cache);
    std::swap(lhs.m_compute_assets, rhs.m_compute_assets);
    std::swap(lhs.m_current_index, rhs.m_current_index);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Compute::Compute(const vierkant::DevicePtr &device, const create_info_t &create_info)
    : m_device(device), m_pipeline_cache(create_info.pipeline_cache)
{
    if(!m_pipeline_cache) { m_pipeline_cache = vierkant::PipelineCache::create(device); }

    m_compute_assets.resize(create_info.num_frames_in_flight);

    if(create_info.descriptor_pool) { m_descriptor_pool = create_info.descriptor_pool; }
    else
    {
        vierkant::descriptor_count_t descriptor_counts = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512},
                                                          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256},
                                                          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256}};
        m_descriptor_pool = vierkant::create_descriptor_pool(m_device, descriptor_counts, 128);
    }
}

void Compute::dispatch(std::vector<computable_t> computables, VkCommandBuffer commandbuffer)
{
    auto &compute_asset = m_compute_assets[m_current_index];
    m_current_index = (m_current_index + 1) % m_compute_assets.size();
    vierkant::descriptor_set_map_t next_descriptor_set_cache;
    std::unordered_map<descriptor_map_t, DescriptorSetLayoutPtr> next_layout_cache;

    struct item_t
    {
        computable_t computable;
        DescriptorSetLayoutPtr set_layout;
    };
    std::unordered_map<vierkant::compute_pipeline_info_t, std::vector<item_t>> pipelines;

    for(auto &computable: computables)
    {
        auto descriptor_set_layout = vierkant::find_or_create_set_layout(
                m_device, computable.descriptors, compute_asset.descriptor_layout_cache, next_layout_cache);
        computable.pipeline_info.descriptor_set_layouts = {descriptor_set_layout.get()};

        pipelines[computable.pipeline_info].push_back({computable, descriptor_set_layout});
    }

    for(auto &[fmt, items]: pipelines)
    {
        // create or retrieve an existing raytracing pipeline
        auto pipeline = m_pipeline_cache->pipeline(fmt);

        // bind compute pipeline
        pipeline->bind(commandbuffer);

        for(auto &item: items)
        {
            auto &[computable, set_layout] = item;

            if(!computable.push_constants.empty())
            {
                // push constant range
                VkPushConstantRange push_constant_range = {};
                push_constant_range.offset = 0;
                push_constant_range.size = computable.push_constants.size();
                push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
                computable.pipeline_info.push_constant_ranges = {push_constant_range};
            }

            // fetch descriptor set
            auto descriptor_set = vierkant::find_or_create_descriptor_set(
                    m_device, set_layout.get(), computable.descriptors, m_descriptor_pool,
                    compute_asset.descriptor_set_cache, next_descriptor_set_cache, false);

            // update descriptor-set with actual descriptors
            vierkant::update_descriptor_set(m_device, computable.descriptors, descriptor_set);

            VkDescriptorSet descriptor_set_handle = descriptor_set.get();

            // bind descriptor set (acceleration-structure, uniforms, storage-buffers, samplers, storage-image)
            vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->layout(), 0, 1,
                                    &descriptor_set_handle, 0, nullptr);

            if(!computable.push_constants.empty())
            {
                // update push_constants
                vkCmdPushConstants(commandbuffer, pipeline->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                   computable.push_constants.size(), computable.push_constants.data());
            }

            // dispatch compute-operation
            vkCmdDispatch(commandbuffer, computable.extent.width, computable.extent.height, computable.extent.depth);
        }
    }

    // keep the stuff in use
    compute_asset.computables = std::move(computables);
    compute_asset.descriptor_set_cache = std::move(next_descriptor_set_cache);
    compute_asset.descriptor_layout_cache = std::move(next_layout_cache);
}

}//namespace vierkant