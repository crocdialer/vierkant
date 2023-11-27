//
// Created by crocdialer on 27.11.23.
//

#include <vierkant/micromap_compute.hpp>

namespace vierkant
{

struct micromap_compute_context_t
{
    vierkant::DevicePtr device;
    vierkant::Compute compute;
    glm::uvec3 micromap_compute_local_size{};
    vierkant::Compute::computable_t micromap_computable;
    vierkant::BufferPtr staging_buffer;
    vierkant::BufferPtr result_buffer;
    uint64_t run_id = 0;
};

micromap_compute_context_handle create_micromap_compute_context(const DevicePtr &/*device*/,
                                                                const PipelineCachePtr &/*pipeline_cache*/)
{
    return {};
}

}// namespace vierkant