#include <unordered_map>
#include <crocore/Timer.hpp>
#include "vierkant/Geometry.hpp"
#include "vierkant/intersection.hpp"

namespace vierkant
{

namespace
{
inline uint64_t pack(uint64_t a, uint64_t b){ return (a << 32U) | b; }

inline uint64_t swizzle(uint64_t a){ return ((a & 0xFFFFFFFFU) << 32U) | (a >> 32U); }
}

std::vector<HalfEdge> compute_half_edges(const vierkant::GeometryConstPtr &geom)
{
    if(geom->topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || geom->indices.size() < 3)
    {
        LOG_WARNING << "requested computation of half-edges for non-triangle mesh";
        return {};
    }

    crocore::Stopwatch timer;
    timer.start();

    std::vector<HalfEdge> ret(3 * geom->indices.size());
    std::unordered_map<uint64_t, HalfEdge *> edge_table;

    HalfEdge *edge = ret.data();

    for(size_t i = 0; i < geom->indices.size(); i += 3)
    {
        index_t a = geom->indices[i], b = geom->indices[i + 1], c = geom->indices[i + 2];

        // create the half-edge that goes from C to A:
        edge_table[pack(c, a)] = edge;
        edge->index = a;
        edge->next = edge + 1;
        ++edge;

        // create the half-edge that goes from A to B:
        edge_table[pack(a, b)] = edge;
        edge->index = b;
        edge->next = edge + 1;
        ++edge;

        // create the half-edge that goes from B to C:
        edge_table[pack(b, c)] = edge;
        edge->index = c;
        edge->next = edge - 2;
        ++edge;
    }

    // populate the twin pointers by iterating over the edge_table
    int boundaryCount = 0;

    for(const auto &[key, current_edge] : edge_table)
    {
        // try to find twin edge in map
        auto it = edge_table.find(swizzle(key));

        if(it != edge_table.end())
        {
            HalfEdge *twin_edge = it->second;
            twin_edge->twin = current_edge;
            current_edge->twin = twin_edge;
        }
        else{ ++boundaryCount; }
    }

    if(boundaryCount > 0)
    {
        LOG_DEBUG << "mesh is not watertight. contains " << boundaryCount << " boundary edges.";
    }
    LOG_TRACE << "half-edge computation took " << (int) std::round(timer.time_elapsed() * 1000.0) << " ms";
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Geometry::compute_face_normals()
{
    if(indices.empty()){ return; }

    normals.resize(vertices.size());

    for(size_t i = 0; i < indices.size(); i += 3)
    {
        index_t a = indices[i], b = indices[i + 1], c = indices[i + 2];

        const glm::vec3 &vA = vertices[a];
        const glm::vec3 &vB = vertices[b];
        const glm::vec3 &vC = vertices[c];
        glm::vec3 normal = glm::normalize(glm::cross(vB - vA, vC - vA));
        normals[a] = normals[b] = normals[c] = normal;
    }
}

void Geometry::compute_vertex_normals()
{
    if(indices.size() < 3){ return; }

    // assert correct size
    if(normals.size() != vertices.size())
    {
        normals.clear();
        normals.resize(vertices.size(), glm::vec3(0));
    }
    else{ std::fill(normals.begin(), normals.end(), glm::vec3(0)); }

    // iterate faces and sum normals for all vertices
    for(size_t i = 0; i < indices.size(); i += 3)
    {
        index_t a = indices[i], b = indices[i + 1], c = indices[i + 2];

        const glm::vec3 &vA = vertices[a];
        const glm::vec3 &vB = vertices[b];
        const glm::vec3 &vC = vertices[c];
        glm::vec3 normal = glm::normalize(glm::cross(vB - vA, vC - vA));
        normals[a] += normal;
        normals[b] += normal;
        normals[c] += normal;
    }

    // normalize vertexNormals
    for(auto &n : normals){ n = glm::normalize(n); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Geometry::compute_tangents()
{
    if(indices.size() % 3){ return; }
    if(tex_coords.size() != vertices.size()){ return; }

    std::vector<glm::vec3> tangents_tmp, bitangents_tmp;

    if(tangents.size() != vertices.size())
    {
        tangents.clear();
        tangents.resize(vertices.size(), glm::vec3(0));
        tangents_tmp.resize(vertices.size(), glm::vec3(0));
        bitangents_tmp.resize(vertices.size(), glm::vec3(0));
    }

    for(size_t i = 0; i < indices.size(); i += 3)
    {
        index_t a = indices[i], b = indices[i + 1], c = indices[i + 2];

        const glm::vec3 &v0 = vertices[a], &v1 = vertices[b], &v2 = vertices[c];
        const glm::vec2 &uv0 = tex_coords[a], &uv1 = tex_coords[b], &uv2 = tex_coords[c];

        float x1 = v1.x - v0.x;
        float x2 = v2.x - v0.x;
        float y1 = v1.y - v0.y;
        float y2 = v2.y - v0.y;
        float z1 = v1.z - v0.z;
        float z2 = v2.z - v0.z;
        float s1 = uv1.x - uv0.x;
        float s2 = uv2.x - uv0.x;
        float t1 = uv1.y - uv0.y;
        float t2 = uv2.y - uv0.y;

        float r = 1.f / (s1 * t2 - s2 * t1);
        auto sdir = glm::vec3(t2 * x1 - t1 * x2, t2 * y1 - t1 * y2, t2 * z1 - t1 * z2) * r;
        auto tdir = glm::vec3(s1 * x2 - s2 * x1, s1 * y2 - s2 * y1, s1 * z2 - s2 * z1) * r;

        tangents_tmp[a] += sdir;
        tangents_tmp[b] += sdir;
        tangents_tmp[c] += sdir;

        bitangents_tmp[a] += tdir;
        bitangents_tmp[b] += tdir;
        bitangents_tmp[c] += tdir;
    }

    for(uint32_t a = 0; a < vertices.size(); ++a)
    {
        const glm::vec3 &n = normals[a];
        const glm::vec3 &t = tangents_tmp[a];
        const glm::vec3 &b = bitangents_tmp[a];

        // Gram-Schmidt orthogonalize
        tangents[a] = glm::normalize(t - n * glm::dot(n, t));

        // correct handedness
        tangents[a] *= (glm::dot(glm::cross(n, t), b) < 0.f) ? 1.f : -1.f;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

GeometryPtr Geometry::create()
{
    return GeometryPtr(new Geometry());
}

///////////////////////////////////////////////////////////////////////////////////////////////////

GeometryPtr Geometry::Grid(float width, float depth, uint32_t numSegments_W, uint32_t numSegments_D)
{
    auto geom = Geometry::create();
    geom->topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    auto &vertices = geom->vertices;
    auto &colors = geom->colors;
    auto &tex_coords = geom->tex_coords;

    float stepX = width / numSegments_W, stepZ = depth / numSegments_D;
    float w2 = width / 2.f, h2 = depth / 2.f;

    const glm::vec4 color_red(1, 0, 0, 1), color_blue(0, 0, 1, 1), color_gray(.6, .6, .6, 1.);
    glm::vec4 color;
    for(uint32_t x = 0; x <= numSegments_W; ++x)
    {
        if(x == 0){ color = color_blue; }
        else{ color = color_gray; }

        // line Z
        vertices.emplace_back(-w2 + x * stepX, 0.f, -h2);
        vertices.emplace_back(-w2 + x * stepX, 0.f, h2);
        colors.push_back(color);
        colors.push_back(color);
        tex_coords.emplace_back(x / (float) numSegments_W, 0.f);
        tex_coords.emplace_back(x / (float) numSegments_W, 1.f);

    }
    for(uint32_t z = 0; z <= numSegments_D; ++z)
    {
        if(z == 0){ color = color_red; }
        else{ color = color_gray; }

        // line X
        vertices.emplace_back(-w2, 0.f, -h2 + z * stepZ);
        vertices.emplace_back(w2, 0.f, -h2 + z * stepZ);
        colors.push_back(color);
        colors.push_back(color);
        tex_coords.emplace_back(0.f, z / (float) numSegments_D);
        tex_coords.emplace_back(1.f, z / (float) numSegments_D);
    }
    return geom;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

GeometryPtr Geometry::Plane(float width, float height, uint32_t numSegments_W, uint32_t numSegments_H)
{
    auto geom = Geometry::create();

    auto &vertices = geom->vertices;
    auto &normals = geom->normals;
    auto &colors = geom->colors;
    auto &tex_coords = geom->tex_coords;
    auto &indices = geom->indices;

    float width_half = width / 2, height_half = height / 2;
    float segment_width = width / numSegments_W, segment_height = height / numSegments_H;

    uint32_t gridX = numSegments_W, gridZ = numSegments_H, gridX1 = gridX + 1, gridZ1 = gridZ + 1;

    glm::vec3 normal(0, 0, 1);

    // create vertices
    for(uint32_t iz = 0; iz < gridZ1; ++iz)
    {
        for(uint32_t ix = 0; ix < gridX1; ++ix)
        {
            float x = ix * segment_width - width_half;
            float y = iz * segment_height - height_half;
            vertices.emplace_back(x, -y, 0);
            normals.push_back(normal);
            tex_coords.emplace_back(ix / (float) gridX, iz / (float) gridZ);
        }
    }

    // fill in colors
    colors.resize(vertices.size(), glm::vec4(1.f));

    // create faces and texcoords
    for(uint32_t iz = 0; iz < gridZ; ++iz)
    {
        for(uint32_t ix = 0; ix < gridX; ++ix)
        {
            uint32_t a = ix + gridX1 * iz;
            uint32_t b = ix + gridX1 * (iz + 1);
            uint32_t c = (ix + 1) + gridX1 * (iz + 1);
            uint32_t d = (ix + 1) + gridX1 * iz;

            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(c);
            indices.push_back(d);
            indices.push_back(a);
        }
    }
    geom->compute_tangents();
    return geom;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

GeometryPtr Geometry::Box(const glm::vec3 &half_extents)
{
    auto geom = Geometry::create();

    auto &vertices = geom->vertices;
    auto &normals = geom->normals;
    auto &colors = geom->colors;
    auto &tex_coords = geom->tex_coords;
    auto &indices = geom->indices;

    glm::vec3 base_vertices[8] =
            {
                    glm::vec3(-half_extents.x, -half_extents.y, half_extents.z),// bottom left front
                    glm::vec3(half_extents.x, -half_extents.y, half_extents.z),// bottom right front
                    glm::vec3(half_extents.x, -half_extents.y, -half_extents.z),// bottom right back
                    glm::vec3(-half_extents.x, -half_extents.y, -half_extents.z),// bottom left back
                    glm::vec3(-half_extents.x, half_extents.y, half_extents.z),// top left front
                    glm::vec3(half_extents.x, half_extents.y, half_extents.z),// top right front
                    glm::vec3(half_extents.x, half_extents.y, -half_extents.z),// top right back
                    glm::vec3(-half_extents.x, half_extents.y, -half_extents.z),// top left back
            };
    glm::vec4 base_colors[6] = {glm::vec4(1, 0, 0, 1), glm::vec4(0, 1, 0, 1), glm::vec4(0, 0, 1, 1),
                                glm::vec4(1, 1, 0, 1), glm::vec4(0, 1, 1, 1), glm::vec4(1, 0, 1, 1)};

    glm::vec2 base_tex_coords[4] = {glm::vec2(0, 1), glm::vec2(1, 1), glm::vec2(1, 0), glm::vec2(0, 0)};

    glm::vec3 base_normals[6] = {glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 0, -1),
                                 glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0), glm::vec3(0, 1, 0)};

    //front - bottom left - 0
    vertices.push_back(base_vertices[0]);
    normals.push_back(base_normals[0]);
    tex_coords.push_back(base_tex_coords[0]);
    colors.push_back(base_colors[0]);
    //front - bottom right - 1
    vertices.push_back(base_vertices[1]);
    normals.push_back(base_normals[0]);
    tex_coords.push_back(base_tex_coords[1]);
    colors.push_back(base_colors[0]);
    //front - top right - 2
    vertices.push_back(base_vertices[5]);
    normals.push_back(base_normals[0]);
    tex_coords.push_back(base_tex_coords[2]);
    colors.push_back(base_colors[0]);
    //front - top left - 3
    vertices.push_back(base_vertices[4]);
    normals.push_back(base_normals[0]);
    tex_coords.push_back(base_tex_coords[3]);
    colors.push_back(base_colors[0]);
    //right - bottom left - 4
    vertices.push_back(base_vertices[1]);
    normals.push_back(base_normals[1]);
    tex_coords.push_back(base_tex_coords[0]);
    colors.push_back(base_colors[1]);
    //right - bottom right - 5
    vertices.push_back(base_vertices[2]);
    normals.push_back(base_normals[1]);
    tex_coords.push_back(base_tex_coords[1]);
    colors.push_back(base_colors[1]);
    //right - top right - 6
    vertices.push_back(base_vertices[6]);
    normals.push_back(base_normals[1]);
    tex_coords.push_back(base_tex_coords[2]);
    colors.push_back(base_colors[1]);
    //right - top left - 7
    vertices.push_back(base_vertices[5]);
    normals.push_back(base_normals[1]);
    tex_coords.push_back(base_tex_coords[3]);
    colors.push_back(base_colors[1]);
    //back - bottom left - 8
    vertices.push_back(base_vertices[2]);
    normals.push_back(base_normals[2]);
    tex_coords.push_back(base_tex_coords[0]);
    colors.push_back(base_colors[2]);
    //back - bottom right - 9
    vertices.push_back(base_vertices[3]);
    normals.push_back(base_normals[2]);
    tex_coords.push_back(base_tex_coords[1]);
    colors.push_back(base_colors[2]);
    //back - top right - 10
    vertices.push_back(base_vertices[7]);
    normals.push_back(base_normals[2]);
    tex_coords.push_back(base_tex_coords[2]);
    colors.push_back(base_colors[2]);
    //back - top left - 11
    vertices.push_back(base_vertices[6]);
    normals.push_back(base_normals[2]);
    tex_coords.push_back(base_tex_coords[3]);
    colors.push_back(base_colors[2]);
    //left - bottom left - 12
    vertices.push_back(base_vertices[3]);
    normals.push_back(base_normals[3]);
    tex_coords.push_back(base_tex_coords[0]);
    colors.push_back(base_colors[3]);
    //left - bottom right - 13
    vertices.push_back(base_vertices[0]);
    normals.push_back(base_normals[3]);
    tex_coords.push_back(base_tex_coords[1]);
    colors.push_back(base_colors[3]);
    //left - top right - 14
    vertices.push_back(base_vertices[4]);
    normals.push_back(base_normals[3]);
    tex_coords.push_back(base_tex_coords[2]);
    colors.push_back(base_colors[3]);
    //left - top left - 15
    vertices.push_back(base_vertices[7]);
    normals.push_back(base_normals[3]);
    tex_coords.push_back(base_tex_coords[3]);
    colors.push_back(base_colors[3]);
    //bottom - bottom left - 16
    vertices.push_back(base_vertices[3]);
    normals.push_back(base_normals[4]);
    tex_coords.push_back(base_tex_coords[0]);
    colors.push_back(base_colors[4]);
    //bottom - bottom right - 17
    vertices.push_back(base_vertices[2]);
    normals.push_back(base_normals[4]);
    tex_coords.push_back(base_tex_coords[1]);
    colors.push_back(base_colors[4]);
    //bottom - top right - 18
    vertices.push_back(base_vertices[1]);
    normals.push_back(base_normals[4]);
    tex_coords.push_back(base_tex_coords[2]);
    colors.push_back(base_colors[4]);
    //bottom - top left - 19
    vertices.push_back(base_vertices[0]);
    normals.push_back(base_normals[4]);
    tex_coords.push_back(base_tex_coords[3]);
    colors.push_back(base_colors[4]);
    //top - bottom left - 20
    vertices.push_back(base_vertices[4]);
    normals.push_back(base_normals[5]);
    tex_coords.push_back(base_tex_coords[0]);
    colors.push_back(base_colors[5]);
    //top - bottom right - 21
    vertices.push_back(base_vertices[5]);
    normals.push_back(base_normals[5]);
    tex_coords.push_back(base_tex_coords[1]);
    colors.push_back(base_colors[5]);
    //top - top right - 22
    vertices.push_back(base_vertices[6]);
    normals.push_back(base_normals[5]);
    tex_coords.push_back(base_tex_coords[2]);
    colors.push_back(base_colors[5]);
    //top - top left - 23
    vertices.push_back(base_vertices[7]);
    normals.push_back(base_normals[5]);
    tex_coords.push_back(base_tex_coords[3]);
    colors.push_back(base_colors[5]);

    for(uint32_t i = 0; i < 6; i++)
    {
        indices.push_back(i * 4 + 0);
        indices.push_back(i * 4 + 1);
        indices.push_back(i * 4 + 2);
        indices.push_back(i * 4 + 2);
        indices.push_back(i * 4 + 3);
        indices.push_back(i * 4 + 0);
    }
    geom->compute_tangents();
    return geom;
}

GeometryPtr Geometry::BoxOutline(const glm::vec3 &half_extents)
{
    auto geom = Geometry::create();
    geom->topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    auto &vertices = geom->vertices;
    auto &colors = geom->colors;

    auto bb = vierkant::AABB(-half_extents, half_extents);

    vertices =
            {
                    // botton
                    bb.min, glm::vec3(bb.min.x, bb.min.y, bb.max.z),
                    glm::vec3(bb.min.x, bb.min.y, bb.max.z), glm::vec3(bb.max.x, bb.min.y, bb.max.z),
                    glm::vec3(bb.max.x, bb.min.y, bb.max.z), glm::vec3(bb.max.x, bb.min.y, bb.min.z),
                    glm::vec3(bb.max.x, bb.min.y, bb.min.z), bb.min,

                    // top
                    glm::vec3(bb.min.x, bb.max.y, bb.min.z), glm::vec3(bb.min.x, bb.max.y, bb.max.z),
                    glm::vec3(bb.min.x, bb.max.y, bb.max.z), glm::vec3(bb.max.x, bb.max.y, bb.max.z),
                    glm::vec3(bb.max.x, bb.max.y, bb.max.z), glm::vec3(bb.max.x, bb.max.y, bb.min.z),
                    glm::vec3(bb.max.x, bb.max.y, bb.min.z), glm::vec3(bb.min.x, bb.max.y, bb.min.z),

                    //sides
                    glm::vec3(bb.min.x, bb.min.y, bb.min.z), glm::vec3(bb.min.x, bb.max.y, bb.min.z),
                    glm::vec3(bb.min.x, bb.min.y, bb.max.z), glm::vec3(bb.min.x, bb.max.y, bb.max.z),
                    glm::vec3(bb.max.x, bb.min.y, bb.max.z), glm::vec3(bb.max.x, bb.max.y, bb.max.z),
                    glm::vec3(bb.max.x, bb.min.y, bb.min.z), glm::vec3(bb.max.x, bb.max.y, bb.min.z)
            };
    colors.resize(vertices.size(), glm::vec4(1.f));
    return geom;
}

}// namespace