#include <cmath>
#include <numbers>
#include <unordered_map>

#include <vierkant/intersection.hpp>
#include <glm/gtx/polar_coordinates.hpp>
#include <vierkant/Geometry.hpp>

namespace vierkant
{

namespace
{
inline uint64_t pack(uint64_t a, uint64_t b) { return (a << 32U) | b; }

inline uint64_t swizzle(uint64_t a) { return ((a & 0xFFFFFFFFU) << 32U) | (a >> 32U); }
}// namespace

[[maybe_unused]] std::vector<HalfEdge> compute_half_edges(const vierkant::GeometryConstPtr &geom)
{
    if(geom->topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || geom->indices.size() < 3)
    {
        spdlog::warn("requested computation of half-edges for non-triangle mesh");
        return {};
    }

    spdlog::stopwatch timer;

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

    for(const auto &[key, current_edge]: edge_table)
    {
        // try to find twin edge in map
        auto it = edge_table.find(swizzle(key));

        if(it != edge_table.end())
        {
            HalfEdge *twin_edge = it->second;
            twin_edge->twin = current_edge;
            current_edge->twin = twin_edge;
        }
        else { ++boundaryCount; }
    }

    if(boundaryCount > 0) { spdlog::debug("mesh is not watertight. contains {} boundary edges.", boundaryCount); }
    spdlog::trace("half-edge computation took {} ms",
                  std::chrono::duration_cast<std::chrono::milliseconds>(timer.elapsed()).count());
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Geometry::compute_face_normals()
{
    if(indices.empty()) { return; }

    normals.resize(positions.size());

    for(size_t i = 0; i < indices.size(); i += 3)
    {
        index_t a = indices[i], b = indices[i + 1], c = indices[i + 2];

        const glm::vec3 &vA = positions[a];
        const glm::vec3 &vB = positions[b];
        const glm::vec3 &vC = positions[c];
        glm::vec3 normal = glm::normalize(glm::cross(vB - vA, vC - vA));
        normals[a] = normals[b] = normals[c] = normal;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Geometry::compute_vertex_normals()
{
    if(indices.size() < 3) { return; }

    // assert correct size
    if(normals.size() != positions.size())
    {
        normals.clear();
        normals.resize(positions.size(), glm::vec3(0));
    }
    else { std::fill(normals.begin(), normals.end(), glm::vec3(0)); }

    // iterate faces and sum normals for all positions
    for(size_t i = 0; i < indices.size(); i += 3)
    {
        index_t a = indices[i], b = indices[i + 1], c = indices[i + 2];

        const glm::vec3 &vA = positions[a];
        const glm::vec3 &vB = positions[b];
        const glm::vec3 &vC = positions[c];
        glm::vec3 normal = glm::normalize(glm::cross(vB - vA, vC - vA));
        normals[a] += normal;
        normals[b] += normal;
        normals[c] += normal;
    }

    // normalize vertexNormals
    for(auto &n: normals) { n = glm::normalize(n); }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void tessellate(const GeometryPtr &geom, uint32_t count, const tessellation_control_fn_t &tessellation_control_fn)
{
    auto &verts = geom->positions;
    auto &indices = geom->indices;

    // iterate
    for(uint32_t i = 0; i < count; ++i)
    {
        verts.reserve(3 * verts.size());
        std::vector<vierkant::index_t> new_indices;
        new_indices.reserve(indices.size() * 4);

        // for each triangle
        for(uint32_t j = 0; j < indices.size(); j += 3)
        {
            // generate 3 new vertices on edges
            uint32_t k = verts.size();

            // generate 4 triangles
            new_indices.insert(new_indices.end(), {indices[j], k + 1, k, k + 1, indices[j + 1], k + 2, k, k + 2,
                                                   indices[j + 2], k, k + 1, k + 2});

            // generate new triangle-vertices, lerp values
            auto interpolate_new_verts_fn = [k, j, &indices](auto &array) {
                const auto &v0 = array[indices[j]], &v1 = array[indices[j + 1]], &v2 = array[indices[j + 2]];

                array.resize(k + 3);
                array[k] = 0.5f * (v0 + v2);
                array[k + 1] = 0.5f * (v0 + v1);
                array[k + 2] = 0.5f * (v1 + v2);
            };

            if(!geom->positions.empty()) { interpolate_new_verts_fn(geom->positions); }
            if(!geom->colors.empty()) { interpolate_new_verts_fn(geom->colors); }
            if(!geom->tex_coords.empty()) { interpolate_new_verts_fn(geom->tex_coords); }

            // this is certainly wrong
            if(!geom->normals.empty()) { interpolate_new_verts_fn(geom->normals); }
            if(!geom->tangents.empty()) { interpolate_new_verts_fn(geom->tangents); }

            // also not elegant
            if(!geom->bone_weights.empty()) { interpolate_new_verts_fn(geom->bone_weights); }
            if(!geom->bone_indices.empty()) { geom->bone_indices.resize(geom->bone_indices.size() + 3); }

            if(tessellation_control_fn)
            {
                tessellation_control_fn(indices[j], indices[j + 1], indices[j + 2], k, k + 1, k + 2);
            }
        }
        indices = std::move(new_indices);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Geometry::compute_tangents()
{
    if(indices.size() % 3) { return; }
    if(tex_coords.size() != positions.size()) { return; }

    std::vector<glm::vec3> tangents_tmp, bitangents_tmp;

    if(tangents.size() != positions.size())
    {
        tangents.clear();
        tangents.resize(positions.size(), glm::vec3(0));
    }
    tangents_tmp.resize(positions.size(), glm::vec3(0));
    bitangents_tmp.resize(positions.size(), glm::vec3(0));

    for(size_t i = 0; i < indices.size(); i += 3)
    {
        index_t a = indices[i], b = indices[i + 1], c = indices[i + 2];

        const glm::vec3 &v0 = positions[a], &v1 = positions[b], &v2 = positions[c];
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

    for(uint32_t a = 0; a < positions.size(); ++a)
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

GeometryPtr Geometry::create() { return GeometryPtr(new Geometry()); }

///////////////////////////////////////////////////////////////////////////////////////////////////

GeometryPtr Geometry::Grid(float width, float depth, uint32_t numSegments_W, uint32_t numSegments_D)
{
    auto geom = Geometry::create();
    geom->topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    auto &vertices = geom->positions;
    auto &colors = geom->colors;
    auto &tex_coords = geom->tex_coords;

    float stepX = width / numSegments_W, stepZ = depth / numSegments_D;
    float w2 = width / 2.f, h2 = depth / 2.f;

    const glm::vec4 color_red(1, 0, 0, 1), color_blue(0, 0, 1, 1), color_gray(.6, .6, .6, 1.);
    glm::vec4 color;
    for(uint32_t x = 0; x <= numSegments_W; ++x)
    {
        if(x == 0) { color = color_blue; }
        else { color = color_gray; }

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
        if(z == 0) { color = color_red; }
        else { color = color_gray; }

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

    auto &vertices = geom->positions;
    auto &normals = geom->normals;
    auto &colors = geom->colors;
    auto &tex_coords = geom->tex_coords;
    auto &indices = geom->indices;

    float width_half = width / 2, height_half = height / 2;
    float segment_width = width / numSegments_W, segment_height = height / numSegments_H;

    uint32_t gridX = numSegments_W, gridZ = numSegments_H, gridX1 = gridX + 1, gridZ1 = gridZ + 1;

    glm::vec3 normal(0, 0, 1);

    // create positions
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

    auto &vertices = geom->positions;
    auto &normals = geom->normals;
    auto &colors = geom->colors;
    auto &tex_coords = geom->tex_coords;
    auto &indices = geom->indices;

    glm::vec3 base_vertices[8] = {
            glm::vec3(-half_extents.x, -half_extents.y, half_extents.z), // bottom left front
            glm::vec3(half_extents.x, -half_extents.y, half_extents.z),  // bottom right front
            glm::vec3(half_extents.x, -half_extents.y, -half_extents.z), // bottom right back
            glm::vec3(-half_extents.x, -half_extents.y, -half_extents.z),// bottom left back
            glm::vec3(-half_extents.x, half_extents.y, half_extents.z),  // top left front
            glm::vec3(half_extents.x, half_extents.y, half_extents.z),   // top right front
            glm::vec3(half_extents.x, half_extents.y, -half_extents.z),  // top right back
            glm::vec3(-half_extents.x, half_extents.y, -half_extents.z), // top left back
    };
    glm::vec4 base_colors[6] = {glm::vec4(1, 0, 0, 1), glm::vec4(0, 1, 0, 1), glm::vec4(0, 0, 1, 1),
                                glm::vec4(1, 1, 0, 1), glm::vec4(0, 1, 1, 1), glm::vec4(1, 0, 1, 1)};

    glm::vec2 base_tex_coords[4] = {glm::vec2(0, 1), glm::vec2(1, 1), glm::vec2(1, 0), glm::vec2(0, 0)};

    glm::vec3 base_normals[6] = {glm::vec3(0, 0, 1),  glm::vec3(1, 0, 0),  glm::vec3(0, 0, -1),
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
    auto &vertices = geom->positions;
    auto &colors = geom->colors;

    auto bb = vierkant::AABB(-half_extents, half_extents);

    vertices = {// botton
                bb.min, glm::vec3(bb.min.x, bb.min.y, bb.max.z), glm::vec3(bb.min.x, bb.min.y, bb.max.z),
                glm::vec3(bb.max.x, bb.min.y, bb.max.z), glm::vec3(bb.max.x, bb.min.y, bb.max.z),
                glm::vec3(bb.max.x, bb.min.y, bb.min.z), glm::vec3(bb.max.x, bb.min.y, bb.min.z), bb.min,

                // top
                glm::vec3(bb.min.x, bb.max.y, bb.min.z), glm::vec3(bb.min.x, bb.max.y, bb.max.z),
                glm::vec3(bb.min.x, bb.max.y, bb.max.z), glm::vec3(bb.max.x, bb.max.y, bb.max.z),
                glm::vec3(bb.max.x, bb.max.y, bb.max.z), glm::vec3(bb.max.x, bb.max.y, bb.min.z),
                glm::vec3(bb.max.x, bb.max.y, bb.min.z), glm::vec3(bb.min.x, bb.max.y, bb.min.z),

                //sides
                glm::vec3(bb.min.x, bb.min.y, bb.min.z), glm::vec3(bb.min.x, bb.max.y, bb.min.z),
                glm::vec3(bb.min.x, bb.min.y, bb.max.z), glm::vec3(bb.min.x, bb.max.y, bb.max.z),
                glm::vec3(bb.max.x, bb.min.y, bb.max.z), glm::vec3(bb.max.x, bb.max.y, bb.max.z),
                glm::vec3(bb.max.x, bb.min.y, bb.min.z), glm::vec3(bb.max.x, bb.max.y, bb.min.z)};
    colors.resize(vertices.size(), glm::vec4(1.f));
    return geom;
}

GeometryPtr Geometry::IcoSphere(float radius, size_t tesselation_count)
{
    auto geom = Geometry::create();

    auto &verts = geom->positions;
    auto &normals = geom->normals;
    auto &tex_coords = geom->tex_coords;
    auto &indices = geom->indices;

    // create 12 vertices of an icosahedron, normalized magnitude
    constexpr float phi = std::numbers::phi_v<float>;
    const float e1 = radius / std::sqrt(1.f + phi * phi);
    const float e2 = phi * e1;

    verts = {{-e1, e2, 0},  {e1, e2, 0},  {-e1, -e2, 0}, {e1, -e2, 0}, {0, -e1, e2},  {0, e1, e2},
             {0, -e1, -e2}, {0, e1, -e2}, {e2, 0, -e1},  {e2, 0, e1},  {-e2, 0, -e1}, {-e2, 0, e1}};

    // create 20 triangles for an icosahedron
    indices = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4,  11, 10, 2,  10, 7, 6, 7, 1, 8,
               3, 9,  4, 3, 4, 2, 3, 2, 6, 3, 6, 8,  3, 8,  9,  4, 9, 5, 2, 4,  11, 6,  2,  10, 8,  6, 7, 9, 8, 1};

    // tessellate
    tessellation_control_fn_t tess_fn = [radius, &verts = geom->positions](index_t a, index_t b, index_t c, index_t ac,
                                                                           index_t ab, index_t bc) {
        const auto &va = verts[a];
        const auto &vb = verts[b];
        const auto &vc = verts[c];
        verts[ab] = radius * glm::normalize(va + vb);
        verts[ac] = radius * glm::normalize(va + vc);
        verts[bc] = radius * glm::normalize(vb + vc);
    };
    tessellate(geom, tesselation_count, tess_fn);

    normals.resize(verts.size());
    tex_coords.resize(verts.size());

    for(uint32_t i = 0; i < verts.size(); ++i)
    {
        const auto &v = verts[i];
        normals[i] = glm::normalize(v);

        auto [latitude, longitude, xz_dist] = glm::polar(v);
        tex_coords[i] = {0.5f + 0.5f * longitude * std::numbers::inv_pi_v<float>,
                         0.5f - 0.5f * latitude / std::numbers::pi_v<float>};
    }
    geom->compute_tangents();
    return geom;
}

GeometryPtr Geometry::UVSphere(float radius, size_t num_segments)
{
    uint32_t rings = num_segments, sectors = num_segments;
    GeometryPtr geom = Geometry::create();
    float const R = 1.f / (float) (rings - 1);
    float const S = 1.f / (float) (sectors - 1);
    uint32_t r, s;

    geom->positions.resize(rings * sectors);
    geom->normals.resize(rings * sectors);
    geom->tex_coords.resize(rings * sectors);

    auto v = geom->positions.begin();
    auto n = geom->normals.begin();
    auto t = geom->tex_coords.begin();

    for(r = 0; r < rings; r++)
    {
        for(s = 0; s < sectors; s++, ++v, ++n, ++t)
        {
            float const x = std::cos(2.f * std::numbers::pi_v<float> * (float) s * S) *
                            std::sin(std::numbers::pi_v<float> * (float) r * R);
            float const y = std::sin(-std::numbers::pi_v<float> / 2.f + std::numbers::pi_v<float> * (float) r * R);
            float const z = std::sin(2.f * std::numbers::pi_v<float> * (float) s * S) *
                            std::sin(std::numbers::pi_v<float> * (float) r * R);

            *t = glm::clamp(glm::vec2(1 - (float) s * S, 1 - (float) r * R), glm::vec2(0), glm::vec2(1));
            *v = glm::vec3(x, y, z) * radius;
            *n = glm::vec3(x, y, z);
        }
    }

    geom->indices.reserve(rings * sectors * 6);

    // create faces
    for(r = 0; r < rings - 1; r++)
    {
        for(s = 0; s < sectors - 1; s++)
        {
            geom->indices.insert(geom->indices.end(),
                                 {r * sectors + s, (r + 1) * sectors + (s + 1), r * sectors + (s + 1), r * sectors + s,
                                  (r + 1) * sectors + s, (r + 1) * sectors + (s + 1)});
        }
    }
    geom->compute_tangents();
    return geom;
}

}// namespace vierkant