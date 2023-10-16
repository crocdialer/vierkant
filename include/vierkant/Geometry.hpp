//
// Created by crocdialer on 3/11/19.
//

#pragma once

#include <vierkant/Device.hpp>
#include <vierkant/math.hpp>
#include <vierkant/nodes.hpp>

namespace vierkant
{

using index_t = uint32_t;

DEFINE_CLASS_PTR(Geometry)

struct HalfEdge
{
    //! Vertex index at the end of this half-edge
    index_t index = std::numeric_limits<index_t>::max();

    //! Oppositely oriented adjacent half-edge
    HalfEdge *twin = nullptr;

    //! Next half-edge around the face
    HalfEdge *next = nullptr;
};

/**
 * @brief   Compute the half-edges for a provided Geometry
 *
 * @param   geom    the geometry to compute the half-edges for
 * @return  an array containing the half-edges
 */
[[maybe_unused]] std::vector<HalfEdge> compute_half_edges(const vierkant::GeometryConstPtr &geom);

/**
 * @brief   signature for a tessellation-control function.
 *
 * can be passed to 'tesselation'-routine which will invoke it passing old & new triangle-indices,
 * allowing to control the newly generated vertex-values.
 *
 * used tesselation schema:
 *            /\ c/2
 *           /  \
 *     ac/3 /----\ bc/5
 *         /\    /\
 *        /  \  /  \
 *   a/0 /----\/----\ b/1
 *           ab/4
 */
using tessellation_control_fn_t =
        std::function<void(index_t a, index_t b, index_t c, index_t ac, index_t ab, index_t bc)>;

/**
 * @brief   tessellate a provided Geometry
 *
 * @param   geom    the geometry to tessellate
 * @param   count   number of iterations
 */
[[maybe_unused]] void tessellate(const vierkant::GeometryPtr &geom, uint32_t count,
                                 const tessellation_control_fn_t& tessellation_control_fn = {});

/**
* @brief   Geometry groups vertex-information and provides factories for common geometries.
*/
class Geometry
{
public:
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec4> colors;
    std::vector<glm::vec2> tex_coords;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> tangents;

    //! each vertex can reference up to 4 bones
    std::vector<glm::vec<4, uint16_t>> bone_indices;
    std::vector<glm::vec4> bone_weights;

    std::vector<index_t> indices;

    // only used by serialization-access
    Geometry() = default;

    Geometry(const Geometry &) = delete;

    Geometry(Geometry &&) = delete;

    Geometry &operator=(const Geometry &other) = default;

    void compute_face_normals();

    void compute_vertex_normals();

    void compute_tangents();

    /**
     * @brief   Factory to create an empty Geometry
     *
     * @return  the newly created Geometry
     */
    static GeometryPtr create();

    /**
    * @brief   Factory to create an indexed plane-geometry with positions in the XY-plane
    *
    * @param   width            the desired width
    * @param   height           the desired height
    * @param   numSegments_W    number of width subdivisions
    * @param   numSegments_H    number of height subdivisions
    * @return  the newly created Geometry for a plane
    */
    static GeometryPtr Plane(float width = 1.f, float height = 1.f, uint32_t numSegments_W = 1,
                             uint32_t numSegments_H = 1);

    /**
    * @brief   Factory to create a grid of lines in the XZ plane
    *
    * @param   width            the desired width
    * @param   depth            the desired depth
    * @param   numSegments_W    number of width subdivisions
    * @param   numSegments_D    number of height subdivisions
    * @return  the newly created Geometry for a plane
    */
    static GeometryPtr Grid(float width = 1.f, float height = 1.f, uint32_t numSegments_W = 10,
                            uint32_t numSegments_D = 10);

    /**
     * @brief   Factory to create a colored box
     *
     * @param   half_extents    a glm::vec3 giving the half extent of the box
     * @return  the newly created Geometry for a box
     */
    static GeometryPtr Box(const glm::vec3 &half_extents = glm::vec3(.5f));

    /**
     * @brief   Factory for an icosahedron, i.e. optionally further tessellated ico-sphere
     *
     * Note: UV-mapping suffers from the naive approach taken here and exhibits visual seems along the UV-coord-wrap.
     *
     * @param   radius              radius for the sphere
     * @param   tesselation_count   number of tessellation-iterations
     * @return  the newly created Geometry for an ico-sphere
     */
    static GeometryPtr IcoSphere(float radius = 1.f, size_t tesselation_count = 0);

    /**
     * @brief   Factory for a sphere with equi-rectangular mapping, a.k.a. UV-sphere
     *
     * @param   radius              radius for the sphere
     * @param   tesselation_count   number of num_segments
     * @return  the newly created Geometry for a UV-sphere
     */
    static GeometryPtr UVSphere(float radius = 1.f, size_t num_segments = 16);

    /**
     * @brief   Factory to create the outlines of a box
     *
     * @param   half_extents
     * @return  the newly created Geometry for a box-outline
     */
    static GeometryPtr BoxOutline(const glm::vec3 &half_extents = glm::vec3(.5f));
};

}// namespace vierkant
