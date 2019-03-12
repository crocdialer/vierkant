//
// Created by crocdialer on 3/11/19.
//

#include "vierkant/Geometry.hpp"
#include "vierkant/intersection.hpp"

namespace vierkant {

///////////////////////////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////////////////////////

Geometry Geometry::Box(const glm::vec3 &half_extents)
{
    Geometry geom;

    glm::vec3 vertices[8] =
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
    glm::vec4 colors[6] = {glm::vec4(1, 0, 0, 1), glm::vec4(0, 1, 0, 1), glm::vec4(0, 0, 1, 1),
                           glm::vec4(1, 1, 0, 1), glm::vec4(0, 1, 1, 1), glm::vec4(1, 0, 1, 1)};

    glm::vec2 texCoords[4] = {glm::vec2(0, 1), glm::vec2(1, 1), glm::vec2(1, 0), glm::vec2(0, 0)};

    glm::vec3 normals[6] = {glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 0, -1),
                            glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0), glm::vec3(0, 1, 0)};

    //front - bottom left - 0
    geom.vertices.push_back(vertices[0]);
    geom.normals.push_back(normals[0]);
    geom.tex_coords.push_back(texCoords[0]);
    geom.colors.push_back(colors[0]);
    //front - bottom right - 1
    geom.vertices.push_back(vertices[1]);
    geom.normals.push_back(normals[0]);
    geom.tex_coords.push_back(texCoords[1]);
    geom.colors.push_back(colors[0]);
    //front - top right - 2
    geom.vertices.push_back(vertices[5]);
    geom.normals.push_back(normals[0]);
    geom.tex_coords.push_back(texCoords[2]);
    geom.colors.push_back(colors[0]);
    //front - top left - 3
    geom.vertices.push_back(vertices[4]);
    geom.normals.push_back(normals[0]);
    geom.tex_coords.push_back(texCoords[3]);
    geom.colors.push_back(colors[0]);
    //right - bottom left - 4
    geom.vertices.push_back(vertices[1]);
    geom.normals.push_back(normals[1]);
    geom.tex_coords.push_back(texCoords[0]);
    geom.colors.push_back(colors[1]);
    //right - bottom right - 5
    geom.vertices.push_back(vertices[2]);
    geom.normals.push_back(normals[1]);
    geom.tex_coords.push_back(texCoords[1]);
    geom.colors.push_back(colors[1]);
    //right - top right - 6
    geom.vertices.push_back(vertices[6]);
    geom.normals.push_back(normals[1]);
    geom.tex_coords.push_back(texCoords[2]);
    geom.colors.push_back(colors[1]);
    //right - top left - 7
    geom.vertices.push_back(vertices[5]);
    geom.normals.push_back(normals[1]);
    geom.tex_coords.push_back(texCoords[3]);
    geom.colors.push_back(colors[1]);
    //back - bottom left - 8
    geom.vertices.push_back(vertices[2]);
    geom.normals.push_back(normals[2]);
    geom.tex_coords.push_back(texCoords[0]);
    geom.colors.push_back(colors[2]);
    //back - bottom right - 9
    geom.vertices.push_back(vertices[3]);
    geom.normals.push_back(normals[2]);
    geom.tex_coords.push_back(texCoords[1]);
    geom.colors.push_back(colors[2]);
    //back - top right - 10
    geom.vertices.push_back(vertices[7]);
    geom.normals.push_back(normals[2]);
    geom.tex_coords.push_back(texCoords[2]);
    geom.colors.push_back(colors[2]);
    //back - top left - 11
    geom.vertices.push_back(vertices[6]);
    geom.normals.push_back(normals[2]);
    geom.tex_coords.push_back(texCoords[3]);
    geom.colors.push_back(colors[2]);
    //left - bottom left - 12
    geom.vertices.push_back(vertices[3]);
    geom.normals.push_back(normals[3]);
    geom.tex_coords.push_back(texCoords[0]);
    geom.colors.push_back(colors[3]);
    //left - bottom right - 13
    geom.vertices.push_back(vertices[0]);
    geom.normals.push_back(normals[3]);
    geom.tex_coords.push_back(texCoords[1]);
    geom.colors.push_back(colors[3]);
    //left - top right - 14
    geom.vertices.push_back(vertices[4]);
    geom.normals.push_back(normals[3]);
    geom.tex_coords.push_back(texCoords[2]);
    geom.colors.push_back(colors[3]);
    //left - top left - 15
    geom.vertices.push_back(vertices[7]);
    geom.normals.push_back(normals[3]);
    geom.tex_coords.push_back(texCoords[3]);
    geom.colors.push_back(colors[3]);
    //bottom - bottom left - 16
    geom.vertices.push_back(vertices[3]);
    geom.normals.push_back(normals[4]);
    geom.tex_coords.push_back(texCoords[0]);
    geom.colors.push_back(colors[4]);
    //bottom - bottom right - 17
    geom.vertices.push_back(vertices[2]);
    geom.normals.push_back(normals[4]);
    geom.tex_coords.push_back(texCoords[1]);
    geom.colors.push_back(colors[4]);
    //bottom - top right - 18
    geom.vertices.push_back(vertices[1]);
    geom.normals.push_back(normals[4]);
    geom.tex_coords.push_back(texCoords[2]);
    geom.colors.push_back(colors[4]);
    //bottom - top left - 19
    geom.vertices.push_back(vertices[0]);
    geom.normals.push_back(normals[4]);
    geom.tex_coords.push_back(texCoords[3]);
    geom.colors.push_back(colors[4]);
    //top - bottom left - 20
    geom.vertices.push_back(vertices[4]);
    geom.normals.push_back(normals[5]);
    geom.tex_coords.push_back(texCoords[0]);
    geom.colors.push_back(colors[5]);
    //top - bottom right - 21
    geom.vertices.push_back(vertices[5]);
    geom.normals.push_back(normals[5]);
    geom.tex_coords.push_back(texCoords[1]);
    geom.colors.push_back(colors[5]);
    //top - top right - 22
    geom.vertices.push_back(vertices[6]);
    geom.normals.push_back(normals[5]);
    geom.tex_coords.push_back(texCoords[2]);
    geom.colors.push_back(colors[5]);
    //top - top left - 23
    geom.vertices.push_back(vertices[7]);
    geom.normals.push_back(normals[5]);
    geom.tex_coords.push_back(texCoords[3]);
    geom.colors.push_back(colors[5]);

    for(uint32_t i = 0; i < 6; i++)
    {
        geom.indices.push_back(i * 4 + 0);
        geom.indices.push_back(i * 4 + 1);
        geom.indices.push_back(i * 4 + 2);
        geom.indices.push_back(i * 4 + 2);
        geom.indices.push_back(i * 4 + 3);
        geom.indices.push_back(i * 4 + 0);
    }
//    geom->compute_tangents();
//    geom->compute_aabb();
    return geom;
}

Geometry Geometry::BoxOutline(const glm::vec3 &half_extents)
{
    Geometry geom;
    geom.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    auto bb = vierkant::AABB(-half_extents, half_extents);
    geom.vertices =
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
    geom.colors.resize(geom.vertices.size(), glm::vec4(1.f));
    return geom;
}

}// namespace