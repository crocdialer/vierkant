//
// Created by crocdialer on 3/20/21.
//

#include <vierkant/PBRPathTracer.hpp>
#include <vierkant/Visitor.hpp>

namespace vierkant
{

PBRPathTracerPtr PBRPathTracer::create(const DevicePtr &device, const PBRPathTracer::create_info_t &create_info)
{
    return vierkant::PBRPathTracerPtr(new PBRPathTracer(device, create_info));
}

PBRPathTracer::PBRPathTracer(const DevicePtr &device, const PBRPathTracer::create_info_t &create_info) :
        m_device(device)
{
    // create our raytracing-thingies
    vierkant::RayTracer::create_info_t ray_tracer_create_info = {};
    ray_tracer_create_info.num_frames_in_flight = create_info.num_frames_in_flight;
    m_ray_tracer = vierkant::RayTracer(device, ray_tracer_create_info);
    m_ray_builder = vierkant::RayBuilder(device);

    m_ray_assets.resize(create_info.num_frames_in_flight);

    m_tracable.extent = create_info.size;

    for(auto &ray_asset : m_ray_assets)
    {
        // create a storage image
        vierkant::Image::Format img_format = {};
        img_format.extent = create_info.size;
        img_format.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        img_format.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        img_format.initial_layout = VK_IMAGE_LAYOUT_GENERAL;
        ray_asset.storage_image = vierkant::Image::create(m_device, img_format);
    }
}

uint32_t PBRPathTracer::render_scene(Renderer &renderer, const SceneConstPtr &scene, const CameraPtr &cam,
                                     const std::set<std::string> &tags)
{
    // TODO: culling, no culling, which volume to use!?
    vierkant::SelectVisitor<vierkant::MeshNode> mesh_selector(tags);
    scene->root()->accept(mesh_selector);

    for(auto node: mesh_selector.objects){ m_ray_builder.add_mesh(node->mesh, node->global_transform()); }

    auto &ray_asset = m_ray_assets[0];

    // similar to a fence wait
    ray_asset.semaphore.wait(RENDER_FINISHED);

    ray_asset.semaphore = vierkant::Semaphore(m_device, 0);

    ray_asset.command_buffer.begin();

    // keep-alive workaround
    auto tmp = ray_asset.acceleration_asset;

    // update top-level structure
    ray_asset.acceleration_asset = m_ray_builder.create_toplevel(ray_asset.command_buffer.handle());

    update_trace_descriptors(cam);
//
//    // transition storage image
//    m_storage_image->transition_layout(VK_IMAGE_LAYOUT_GENERAL, ray_asset.command_buffer.handle());

    // tada
    m_ray_tracer.trace_rays(ray_asset.tracable, ray_asset.command_buffer.handle());
    ray_asset.tracable.batch_index++;

    uint32_t num_drawables = mesh_selector.objects.size();
    return num_drawables;
}

void PBRPathTracer::update_trace_descriptors(const CameraPtr &cam)
{
    auto &ray_asset = m_ray_assets[0];

    ray_asset.tracable.pipeline_info = m_tracable.pipeline_info;

    constexpr size_t max_num_maps = 256;
    ray_asset.acceleration_asset.textures.resize(max_num_maps);
    ray_asset.acceleration_asset.normalmaps.resize(max_num_maps);
    ray_asset.acceleration_asset.emissions.resize(max_num_maps);
    ray_asset.acceleration_asset.ao_rough_metal_maps.resize(max_num_maps);

    // descriptors
    vierkant::descriptor_t desc_acceleration_structure = {};
    desc_acceleration_structure.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    desc_acceleration_structure.stage_flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    desc_acceleration_structure.acceleration_structure = ray_asset.acceleration_asset.structure;
    ray_asset.tracable.descriptors[0] = desc_acceleration_structure;

    vierkant::descriptor_t desc_storage_image = {};
    desc_storage_image.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    desc_storage_image.stage_flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    desc_storage_image.image_samplers = {ray_asset.storage_image};
    ray_asset.tracable.descriptors[1] = desc_storage_image;

    // provide inverse modelview and projection matrices
    std::vector<glm::mat4> matrices = {glm::inverse(cam->view_matrix()),
                                       glm::inverse(cam->projection_matrix())};

    vierkant::descriptor_t desc_matrices = {};
    desc_matrices.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_matrices.stage_flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    desc_matrices.buffers = {vierkant::Buffer::create(m_device, matrices,
                                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                      VMA_MEMORY_USAGE_CPU_TO_GPU)};
    ray_asset.tracable.descriptors[2] = desc_matrices;

    vierkant::descriptor_t desc_vertex_buffers = {};
    desc_vertex_buffers.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    desc_vertex_buffers.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_vertex_buffers.buffers = ray_asset.acceleration_asset.vertex_buffers;
    desc_vertex_buffers.buffer_offsets = ray_asset.acceleration_asset.vertex_buffer_offsets;
    ray_asset.tracable.descriptors[3] = desc_vertex_buffers;

    vierkant::descriptor_t desc_index_buffers = {};
    desc_index_buffers.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    desc_index_buffers.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_index_buffers.buffers = ray_asset.acceleration_asset.index_buffers;
    desc_index_buffers.buffer_offsets = ray_asset.acceleration_asset.index_buffer_offsets;
    ray_asset.tracable.descriptors[4] = desc_index_buffers;

    vierkant::descriptor_t desc_entries = {};
    desc_entries.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_entries.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_entries.buffers = {ray_asset.acceleration_asset.entry_buffer};
    ray_asset.tracable.descriptors[5] = desc_entries;

    vierkant::descriptor_t desc_materials = {};
    desc_materials.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_materials.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_materials.buffers = {ray_asset.acceleration_asset.material_buffer};
    ray_asset.tracable.descriptors[6] = desc_materials;

    vierkant::descriptor_t desc_textures = {};
    desc_textures.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_textures.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_textures.image_samplers = ray_asset.acceleration_asset.textures;
    ray_asset.tracable.descriptors[7] = desc_textures;

    vierkant::descriptor_t desc_normalmaps = {};
    desc_normalmaps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_normalmaps.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_normalmaps.image_samplers = ray_asset.acceleration_asset.normalmaps;
    ray_asset.tracable.descriptors[8] = desc_normalmaps;

    vierkant::descriptor_t desc_emissions = {};
    desc_emissions.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_emissions.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_emissions.image_samplers = ray_asset.acceleration_asset.emissions;
    ray_asset.tracable.descriptors[9] = desc_emissions;

    vierkant::descriptor_t desc_ao_rough_metal_maps = {};
    desc_ao_rough_metal_maps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_ao_rough_metal_maps.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_ao_rough_metal_maps.image_samplers = ray_asset.acceleration_asset.ao_rough_metal_maps;
    ray_asset.tracable.descriptors[10] = desc_ao_rough_metal_maps;

    vierkant::descriptor_t desc_environment = {};
    desc_environment.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_environment.stage_flags = VK_SHADER_STAGE_MISS_BIT_KHR;
    desc_environment.image_samplers = {m_environment};
    ray_asset.tracable.descriptors[11] = desc_environment;

    if(!ray_asset.tracable.descriptor_set_layout)
    {
        ray_asset.tracable.descriptor_set_layout = vierkant::create_descriptor_set_layout(m_device,
                                                                                          ray_asset.tracable.descriptors);
    }
    ray_asset.tracable.pipeline_info.descriptor_set_layouts = {ray_asset.tracable.descriptor_set_layout.get()};
}

void PBRPathTracer::set_environment(const ImagePtr &cubemap)
{
    m_environment = cubemap;
}

}// namespace vierkant

