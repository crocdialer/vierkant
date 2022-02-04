//
// Created by crocdialer on 1/26/21.
//

#include <crocore/Cache.hpp>

#include <vierkant/Device.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/descriptor.hpp>

namespace vierkant
{

/**
 * @brief   RaytBuilder can be used to create bottom and toplevel acceleration-structures
 *          used by raytracing pipelines.
 *
 */
class RayBuilder
{
public:

    struct entry_t
    {
        // per entry
        glm::mat4 modelview = glm::mat4(1);
        glm::mat4 normal_matrix = glm::mat4(1);
        uint32_t material_index = 0;

        int32_t vertex_offset = 0;
        uint32_t base_index = 0;

        // per mesh
        uint32_t buffer_index = 0;
    };

    struct material_struct_t
    {
        glm::vec4 color = glm::vec4(1);

        glm::vec4 emission = glm::vec4(0);

        float metalness = 0.f;

        float roughness = 1.f;

        float transmission = 0.f;

        float attenuation_distance = std::numeric_limits<float>::infinity();

        glm::vec4 attenuation_color = glm::vec4(1.f);

        float ior = 1.5f;

        float clearcoat_factor = 0.f;

        float clearcoat_roughness_factor = 0.f;

        float sheen_roughness = 0.f;

        glm::vec4 sheen_color = glm::vec4(0.f);

        uint32_t texture_index = 0;

        uint32_t normalmap_index = 0;

        uint32_t emission_index = 0;

        uint32_t ao_rough_metal_index = 0;

//        int padding[0];
    };

    //! used for both bottom and toplevel acceleration-structures
    struct acceleration_asset_t
    {
        vierkant::AccelerationStructurePtr structure = nullptr;
        VkDeviceAddress device_address = 0;
        vierkant::BufferPtr buffer = nullptr;
        glm::mat4 transform = glm::mat4(1);

        //! buffer containing entry-information
        vierkant::BufferPtr entry_buffer = nullptr;

        //! buffer containing material-information
        vierkant::BufferPtr material_buffer = nullptr;

        std::vector<vierkant::ImagePtr> textures, normalmaps, emissions, ao_rough_metal_maps;

        //! vertex- and index-buffers for the entire scene
        std::vector<vierkant::BufferPtr> vertex_buffers;
        std::vector<vierkant::BufferPtr> index_buffers;
        std::vector<VkDeviceSize> vertex_buffer_offsets;
        std::vector<VkDeviceSize> index_buffer_offsets;

        //! keep-alives, used during toplevel builds
        vierkant::BufferPtr instance_buffer = nullptr;
        vierkant::BufferPtr scratch_buffer = nullptr;
    };

    //! shared acceleration_asset_t
    using acceleration_asset_ptr = std::shared_ptr<acceleration_asset_t>;

    //! can be used to used to cache an array of shared (bottom-lvl) acceleration-structures per mesh
    using acceleration_asset_map_t = std::unordered_map<vierkant::MeshConstPtr, std::vector<acceleration_asset_ptr>>;

    enum SemaphoreValue : uint64_t
    {
        BUILD = 1,
        COMPACTED,
    };

    struct build_result_t
    {
        std::vector<acceleration_asset_ptr> acceleration_assets;
        std::vector<acceleration_asset_ptr> compacted_assets;
        vierkant::Semaphore semaphore;
        vierkant::QueryPoolPtr query_pool;

        //! bottom-lvl-build
        vierkant::CommandBuffer build_command;

        //! copy/compaction
        vierkant::CommandBuffer compact_command;
    };

    RayBuilder() = default;

    explicit RayBuilder(const vierkant::DevicePtr &device, VkQueue queue, vierkant::VmaPoolPtr pool = nullptr);

    /**
     * @brief   add_mesh will create and cache a bottom-level acceleration structure for each mesh-entry.
     *          if the mesh can be found in the cache, only the transformation will be updated.
     *          TODO: update buffers for vertex-skinned meshes
     *
     * @param   mesh        a provided vierkant::MeshConstPtr
     * @param   transform   a provided transformation-matrix
     */
    [[nodiscard]] build_result_t create_mesh_structures(const vierkant::MeshConstPtr &mesh,
                                                        const glm::mat4 &transform = glm::mat4(1)) const;

    void compact(build_result_t &build_result) const;


    /**
     * @brief   create_toplevel will create a toplevel acceleration structure,
     *          instancing all cached bottom-levels.
     *
     * @param   last    an optional, existing toplevel-structure to perform an update to
     */
    acceleration_asset_t create_toplevel(const acceleration_asset_map_t &asset_map,
                                         VkCommandBuffer commandbuffer = VK_NULL_HANDLE,
                                         const vierkant::AccelerationStructurePtr &last = nullptr) const;

private:

    void set_function_pointers();

    [[nodiscard]] acceleration_asset_t create_acceleration_asset(VkAccelerationStructureCreateInfoKHR create_info,
                                                                 const glm::mat4 &transform = glm::mat4(1)) const;

    vierkant::DevicePtr m_device;

    VkQueue m_queue = VK_NULL_HANDLE;

    vierkant::VmaPoolPtr m_memory_pool = nullptr;

    vierkant::CommandPoolPtr m_command_pool;

    vierkant::ImagePtr m_placeholder_solid_white, m_placeholder_normalmap, m_placeholder_emission,
            m_placeholder_ao_rough_metal;

    vierkant::BufferPtr m_placeholder_buffer;

    // process-addresses for raytracing related functions
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR = nullptr;
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR = nullptr;
};

}// namespace vierkant
