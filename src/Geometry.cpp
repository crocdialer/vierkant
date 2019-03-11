//
// Created by crocdialer on 3/11/19.
//

#include "vierkant/Geometry.hpp"

namespace vierkant {

Geometry Geometry::Grid(float width, float depth, uint32_t numSegments_W, uint32_t numSegments_D)
{
    Geometry geom;
    geom.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    auto &points = geom.vertices;
    auto &colors = geom.colors;
    auto &tex_coords = geom.tex_coords;

    float stepX = width / numSegments_W, stepZ = depth / numSegments_D;
    float w2 = width / 2.f, h2 = depth / 2.f;

    const glm::vec4 color_red(1, 0, 0, 1), color_blue(0, 0, 1, 1), color_gray(.6, .6, .6, 1.);
    glm::vec4 color;
    for(uint32_t x = 0; x <= numSegments_W; ++x)
    {
        if(x == 0){ color = color_blue; }
        else{ color = color_gray; }

        // line Z
        points.emplace_back(-w2 + x * stepX, 0.f, -h2);
        points.emplace_back(-w2 + x * stepX, 0.f, h2);
        colors.push_back(color);
        colors.push_back(color);
        tex_coords.emplace_back(x / (float)numSegments_W, 0.f);
        tex_coords.emplace_back(x / (float)numSegments_W, 1.f);

    }
    for(uint32_t z = 0; z <= numSegments_D; ++z)
    {
        if(z == 0){ color = color_red; }
        else{ color = color_gray; }

        // line X
        points.emplace_back(-w2, 0.f, -h2 + z * stepZ);
        points.emplace_back(w2, 0.f, -h2 + z * stepZ);
        colors.push_back(color);
        colors.push_back(color);
        tex_coords.emplace_back(0.f, z / (float)numSegments_D);
        tex_coords.emplace_back(1.f, z / (float)numSegments_D);
    }
    return geom;
}

Geometry Geometry::Plane(float width, float height, uint32_t numSegments_W, uint32_t numSegments_H)
{
    Geometry geom;

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
            geom.vertices.emplace_back(x, -y, 0);
            geom.normals.push_back(normal);
            geom.tex_coords.emplace_back(ix / (float)gridX, iz / (float)gridZ);
        }
    }

    // fill in colors
    geom.colors.resize(geom.vertices.size(), glm::vec4(1.f));

    // create faces and texcoords
    for(uint32_t iz = 0; iz < gridZ; ++iz)
    {
        for(uint32_t ix = 0; ix < gridX; ++ix)
        {
            uint32_t a = ix + gridX1 * iz;
            uint32_t b = ix + gridX1 * (iz + 1);
            uint32_t c = (ix + 1) + gridX1 * (iz + 1);
            uint32_t d = (ix + 1) + gridX1 * iz;

            geom.indices.push_back(a);
            geom.indices.push_back(b);
            geom.indices.push_back(c);
            geom.indices.push_back(c);
            geom.indices.push_back(d);
            geom.indices.push_back(a);
        }
    }
//    geom->compute_tangents();
//    geom->compute_aabb();
    return geom;
}

}// namespace