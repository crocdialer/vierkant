//
// Created by crocdialer on 3/11/19.
//

#pragma once

#include "vierkant/Device.hpp"

#define GLM_FORCE_CXX11
#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/matrix_inverse.hpp"

#define GLM_ENABLE_EXPERIMENTAL

#include "glm/gtx/norm.hpp"
#include <glm/gtx/hash.hpp>

namespace vierkant {

DEFINE_CLASS_PTR(Geometry)

/**
* @brief   Geometry groups vertex-information and provides factories for common geometries.
*/
class Geometry
{
public:

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    std::vector<uint32_t> indices;

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> tex_coords;
    std::vector<glm::vec4> colors;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> tangents;

    Geometry(const Geometry &) = delete;

    Geometry(Geometry &&) = delete;

    Geometry &operator=(Geometry other) = delete;

    /**
     * @brief   Factory to create an empty Geometry
     *
     * @return  the newly created Geometry
     */
    static GeometryPtr create();

    /**
    * @brief   Factory to create an indexed plane-geometry with vertices in the XY-plane
    *
    * @param   width            the desired width
    * @param   height           the desired height
    * @param   numSegments_W    number of width subdivisions
    * @param   numSegments_H    number of height subdivisions
    * @return  the newly created Geometry for a plane
    */
    static GeometryPtr
    Plane(float width = 1.f, float height = 1.f, uint32_t numSegments_W = 1, uint32_t numSegments_H = 1);

    /**
    * @brief   Factory to create a grid of lines in the XZ plane
    *
    * @param   width            the desired width
    * @param   depth            the desired depth
    * @param   numSegments_W    number of width subdivisions
    * @param   numSegments_D    number of height subdivisions
    * @return  the newly created Geometry for a plane
    */
    static GeometryPtr
    Grid(float width = 1.f, float height = 1.f, uint32_t numSegments_W = 10, uint32_t numSegments_D = 10);

    /**
     * @brief   Factory to create a colored box
     *
     * @param   half_extents    a glm::vec3 giving the half extent of the box
     * @return  the newly created Geometry for a box
     */
    static GeometryPtr Box(const glm::vec3 &half_extents = glm::vec3(.5f));

    /**
     * @brief   Factory to create the outlines of a box
     * @param   half_extents
     * @return  the newly created Geometry for a box-outline
     */
    static GeometryPtr BoxOutline(const glm::vec3 &half_extents = glm::vec3(.5f));

private:

    Geometry() = default;
};

}// namespace
