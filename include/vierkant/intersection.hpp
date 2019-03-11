// __ ___ ____ _____ ______ _______ ________ _______ ______ _____ ____ ___ __
//
// Copyright (C) 2012-2016, Fabian Schmidt <crocdialer@googlemail.com>
//
// It is distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
// __ ___ ____ _____ ______ _______ ________ _______ ______ _____ ____ ___ __

/*
 * geometry_types.hpp
 *
 *  Created on: Sep 21, 2008
 *      Author: Fabian
 */

#pragma once

#include "vierkant/Geometry.hpp"

namespace vierkant {

enum intersection_type
{
    REJECT = 0, INTERSECT = 1, INSIDE = 2
};

struct Geometry;
struct Ray;
struct Plane;
struct Triangle;
struct Sphere;
struct AABB;
struct OBB;
struct Frustum;
struct ray_intersection;
struct ray_triangle_intersection;

/********************************** Ray intersection tests ****************************************/

ray_intersection intersect(const Plane &plane, const Ray &ray);

ray_triangle_intersection intersect(const Triangle &theTri, const Ray &theRay);

ray_intersection intersect(const Sphere &theSphere, const Ray &theRay);

ray_intersection intersect(const AABB &theAABB, const Ray &theRay);

ray_intersection intersect(const OBB &theOBB, const Ray &theRay);

AABB compute_aabb(const std::vector<glm::vec3> &theVertices);

glm::vec3 calculate_centroid(const std::vector<glm::vec3> &theVertices);

/**
 *    A  +-------------+  B
 *      /               \
 *     /                 \
 *    /                   \
 * D +-------------------- +  C
 */
glm::mat4 calculate_homography(const glm::vec2 src[4], const glm::vec2 dst[4]);

struct Ray
{
    glm::vec3 origin;
    glm::vec3 direction;

    Ray(const glm::vec3 &theOrigin, const glm::vec3 &theDir) :
            origin(theOrigin), direction(normalize(theDir)) {}

    inline Ray &transform(const glm::mat4 &t)
    {
        origin = (t * glm::vec4(origin, 1.0f)).xyz();
        direction = normalize(glm::mat3(t) * direction);
        return *this;
    };

    inline Ray transform(const glm::mat4 &t) const
    {
        Ray ret = *this;
        return ret.transform(t);
    };

    inline friend glm::vec3 operator*(const Ray &theRay, float t) { return theRay.origin + t * theRay.direction; }

    inline friend glm::vec3 operator*(float t, const Ray &theRay) { return theRay.origin + t * theRay.direction; }
};

/*!
 * Encapsulates type of intersection and distance along the ray.
 */
struct ray_intersection
{
    intersection_type type;
    float distance;

    ray_intersection(intersection_type theType, float theDistance = 0.0f) :
            type(theType), distance(theDistance) {}

    operator intersection_type() const { return type; }
};

/*!
 * Encapsulates type of intersection and distance along the ray.
 * Additionally adds the coordinates within the triangle.
 */
struct ray_triangle_intersection : public ray_intersection
{
    float u, v;

    ray_triangle_intersection(intersection_type theType, float theDistance = 0.0f,
                              float theU = 0.0f, float theV = 0.0f)
            : ray_intersection(theType, theDistance), u(theU), v(theV) {}
};

struct Plane
{
    // Ax + By + Cz + D = 0
    glm::vec4 coefficients;

    Plane();

    Plane(const glm::vec4 &theCoefficients);

    Plane(float theA, float theB, float theC, float theD);

    Plane(const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2);

    Plane(const glm::vec3 &f, const glm::vec3 &n);

    inline const glm::vec3 &normal() const { return *((glm::vec3 *)(&coefficients[0])); };

    inline float distance(const glm::vec3 &p) const
    {
        return dot(p, coefficients.xyz()) + coefficients.w;
    };

    inline Plane &transform(const glm::mat4 &t)
    {
        coefficients = glm::inverseTranspose(t) * coefficients;
        return *this;
    };

    inline Plane transform(const glm::mat4 &t) const
    {
        Plane ret = *this;
        return ret.transform(t);
    };

    inline ray_intersection intersect(const Ray &ray)
    {
        return vierkant::intersect(*this, ray);
    }
};

struct Triangle
{
    glm::vec3 v0;
    glm::vec3 v1;
    glm::vec3 v2;

    Triangle(const glm::vec3 &_v0, const glm::vec3 &_v1, const glm::vec3 &_v2)
            : v0(_v0), v1(_v1), v2(_v2) {}

    inline Triangle &transform(const glm::mat4 &t)
    {
        v0 = (t * glm::vec4(v0, 1.0f)).xyz();
        v1 = (t * glm::vec4(v1, 1.0f)).xyz();
        v2 = (t * glm::vec4(v2, 1.0f)).xyz();
        return *this;
    };

    inline Triangle transform(const glm::mat4 &t) const
    {
        Triangle ret = *this;
        return ret.transform(t);
    };

    inline ray_triangle_intersection intersect(const Ray &theRay) const
    {
        return vierkant::intersect(*this, theRay);
    };

    inline glm::vec3 normal() const
    {
        return normalize(cross(v1 - v0, v2 - v0));
    };
};

struct Sphere
{

    glm::vec3 center;
    float radius;

    Sphere(const glm::vec3 &c, float r)
    {
        center = c;
        radius = r;
    };

    inline Sphere &transform(const glm::mat4 &t)
    {
        center = (t * glm::vec4(center, 1.0f)).xyz();
        float max_scale = std::max(std::max(t[0][0], t[1][1]), t[2][2]);
        radius *= max_scale;
        return *this;
    };

    inline Sphere transform(const glm::mat4 &t) const
    {
        Sphere ret = *this;
        return ret.transform(t);
    };

    inline uint32_t intersect(const glm::vec3 &thePoint) const
    {
        if(glm::length2(center - thePoint) > radius * radius)
            return REJECT;

        return INSIDE;
    }

    inline ray_intersection intersect(const Ray &theRay) const
    {
        return vierkant::intersect(*this, theRay);
    }
};

/*
 *simple Axis aligned bounding box (AABB) structure
 */
struct AABB
{
    glm::vec3 min;
    glm::vec3 max;

    AABB() : min(glm::vec3(0)), max(glm::vec3(0)) {};

    AABB(const glm::vec3 &theMin,
         const glm::vec3 &theMax) :
            min(theMin),
            max(theMax) {}

    inline float width() const { return max.x - min.x; }

    inline float height() const { return max.y - min.y; }

    inline float depth() const { return max.z - min.z; }

    inline glm::vec3 halfExtents() const { return (max - min) / 2.f; }

    inline glm::vec3 size() const { return (max - min); }

    inline glm::vec3 center() const { return max - halfExtents(); }

    const AABB operator+(const AABB &theAABB) const
    {
        AABB ret(*this);
        ret += theAABB;
        return ret;
    }

    AABB &operator+=(const AABB &theAABB)
    {
        min.x = std::min(min.x, theAABB.min.x);
        min.y = std::min(min.y, theAABB.min.y);
        min.z = std::min(min.z, theAABB.min.z);
        max.x = std::max(max.x, theAABB.max.x);
        max.y = std::max(max.y, theAABB.max.y);
        max.z = std::max(max.z, theAABB.max.z);
        return *this;
    }

    inline bool operator==(const AABB &theAABB) const
    {
        return min == theAABB.min && max == theAABB.max;
    }

    /* used for fast AABB <-> Plane intersection test */
    inline glm::vec3 pos_vertex(const glm::vec3 &dir) const
    {
        glm::vec3 ret = min;
        if(dir.x >= 0){ ret.x = max.x; }
        if(dir.y >= 0){ ret.y = max.y; }
        if(dir.z >= 0){ ret.z = max.z; }
        return ret;
    }

    /* used for fast AABB <-> Plane intersection test */
    inline glm::vec3 neg_vertex(const glm::vec3 &dir) const
    {
        glm::vec3 ret = max;
        if(dir.x >= 0){ ret.x = min.x; }
        if(dir.y >= 0){ ret.y = min.y; }
        if(dir.z >= 0){ ret.z = min.z; }
        return ret;
    }

    AABB &transform(const glm::mat4 &t);

    inline AABB transform(const glm::mat4 &t) const
    {
        AABB ret = *this;
        return ret.transform(t);
    }

    inline uint32_t intersect(const glm::vec3 &thePoint)
    {
        if(thePoint.x < min.x || thePoint.x > max.x){ return REJECT; }
        if(thePoint.y < min.y || thePoint.y > max.y){ return REJECT; }
        if(thePoint.z < min.z || thePoint.z > max.z){ return REJECT; }
        return INSIDE;
    }

    ray_intersection intersect(const Ray &theRay) const;

    uint32_t intersect(const Triangle &t) const;
};

struct OBB
{
    glm::vec3 center;
    glm::vec3 axis[3];
    glm::vec3 half_lengths;

    OBB(const AABB &theAABB, const glm::mat4 &t);

    OBB &transform(const glm::mat4 &t);

    inline OBB transform(const glm::mat4 &t) const
    {
        OBB ret = *this;
        return ret.transform(t);
    }

    inline ray_intersection intersect(const Ray &theRay) const
    {
        return vierkant::intersect(*this, theRay);
    }

    inline bool contains(const glm::vec3 &p) const
    {
        // point in axis space
        const glm::mat3 &mat = *reinterpret_cast<const glm::mat3 *>(&axis[0][0]);
        glm::vec3 p_in_axis_space = mat * (p - center);
        return std::abs(p_in_axis_space.x) < half_lengths.x &&
               std::abs(p_in_axis_space.y) < half_lengths.y &&
               std::abs(p_in_axis_space.z) < half_lengths.z;
    };
};

struct Frustum
{
    Plane planes[6];

    Frustum(const glm::mat4 &the_VP_martix);

    Frustum(float aspect, float fov, float near, float far);

    Frustum(float left, float right, float bottom, float top,
            float near, float far);

    inline Frustum &transform(const glm::mat4 &t)
    {
        Plane *end = planes + 6;
        for(Plane *p = planes; p < end; p++){ p->transform(t); }
        return *this;
    }

    inline Frustum transform(const glm::mat4 &t) const
    {
        Frustum ret = *this;
        return ret.transform(t);
    };

    inline uint32_t intersect(const glm::vec3 &v)
    {
        Plane *end = planes + 6;
        for(Plane *p = planes; p < end; p++)
        {
            if(p->distance(v) < 0){ return REJECT; }
        }
        return INSIDE;
    };

    inline uint32_t intersect(const Sphere &s)
    {
        Plane *end = planes + 6;
        for(Plane *p = planes; p < end; p++)
        {
            if(-p->distance(s.center) > s.radius){ return REJECT; }
        }
        return INSIDE;
    };

    inline uint32_t intersect(const AABB &aabb)
    {
        uint32_t ret = INSIDE;

        Plane *end = planes + 6;
        for(Plane *p = planes; p < end; p++)
        {
            //positive vertex outside ?
            if(p->distance(aabb.pos_vertex(p->normal())) < 0){ return REJECT; }

                //negative vertex outside ?
            else if(p->distance(aabb.neg_vertex(p->normal())) < 0){ ret = INTERSECT; }
        }
        return ret;
    };
};

}//namespace
