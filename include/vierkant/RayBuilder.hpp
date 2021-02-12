//
// Created by crocdialer on 1/26/21.
//

#include <crocore/Cache.hpp>

#include <vierkant/Device.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/descriptor.hpp>

namespace vierkant
{

//! define a shared handle for a VkQueryPool
using QueryPoolPtr = std::shared_ptr<VkQueryPool_T>;

QueryPoolPtr create_query_pool(const vierkant::DevicePtr &device,
                               uint32_t query_count,
                               VkQueryType query_type);

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
        // per mesh
        uint32_t buffer_index = 0;
        uint32_t material_index = 0;

        // per entry
        uint32_t base_vertex = 0;
        uint32_t base_index = 0;
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

        //! keep-alives, used during toplevel builds
        vierkant::BufferPtr instance_buffer = nullptr;
        vierkant::BufferPtr scratch_buffer = nullptr;
    };

    RayBuilder() = default;

    explicit RayBuilder(const vierkant::DevicePtr &device);

    /**
     * @brief   add_mesh will create create and cache a bottom-level acceleration structure
     *          for each mesh-entry.
     *          [if the mesh can be found in the cache, only the transformation will be updated.] idk
     *
     * @param   mesh        a provided vierkant::MeshConstPtr
     * @param   transform   a provided transformation-matrix
     */
    void add_mesh(const vierkant::MeshConstPtr &mesh, const glm::mat4 &transform = glm::mat4(1));

    /**
     * @brief   create_toplevel will create a toplevel acceleration structure,
     *          instancing all cached bottom-levels.
     *
     * @param   last    an optional, existing toplevel-structure to perform an update to
     */
    acceleration_asset_t create_toplevel(VkCommandBuffer commandbuffer = VK_NULL_HANDLE,
                                         const vierkant::AccelerationStructurePtr& last = nullptr);

private:

    void set_function_pointers();

    acceleration_asset_t create_acceleration_asset(VkAccelerationStructureCreateInfoKHR create_info,
                                                   const glm::mat4 &transform = glm::mat4(1));

    vierkant::DevicePtr m_device;

    std::unordered_map<vierkant::MeshConstPtr, std::vector<acceleration_asset_t>> m_acceleration_assets;

    vierkant::CommandPoolPtr m_command_pool;

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
