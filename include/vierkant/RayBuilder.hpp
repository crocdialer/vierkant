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
     * @brief   build_toplevel will create a toplevel acceleration structures,
     *          instancing all cached bottom-levels.
     */
    void build_toplevel();

    vierkant::AccelerationStructurePtr acceleration_structure() const{ return m_top_level.structure; }

private:

    //! used for both bottom and toplevel acceleration-structures
    struct acceleration_asset_t
    {
        vierkant::AccelerationStructurePtr structure = nullptr;
        VkDeviceAddress device_address = 0;
        vierkant::BufferPtr buffer = nullptr;
        glm::mat4 transform = glm::mat4(1);
    };

    void set_function_pointers();

    acceleration_asset_t create_acceleration_asset(VkAccelerationStructureCreateInfoKHR create_info,
                                                   const glm::mat4 &transform = glm::mat4(1));

    void create_toplevel_structure();

    vierkant::DevicePtr m_device;

    acceleration_asset_t m_top_level = {};

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
