/*
 *  intersection.hpp
 *
 *  Created on: Sep 21, 2008
 *  Author: Fabian
 */

#pragma once

#include <array>
#include <vector>
#include <vierkant/math.hpp>
#include <vierkant/transform.hpp>

namespace vierkant
{

enum intersection_type
{
    REJECT = 0,
    INTERSECT = 1,
    INSIDE = 2
};

/*!
 * Encapsulates type of intersection and distance along a ray.
 */
struct ray_intersection
{
    intersection_type type = REJECT;
    float distance = 0.f;

    ray_intersection(intersection_type theType, float theDistance = 0.0f) : type(theType), distance(theDistance) {}

    explicit operator intersection_type() const { return type; }

    explicit operator bool() const { return type; }
};

/*!
 * Encapsulates type of intersection and distance along a ray.
 * Additionally return barycentric coordiantes within the triangle.
 */
struct ray_triangle_intersection : public ray_intersection
{
    float u = 0.f, v = 0.f;

    ray_triangle_intersection(intersection_type theType, float theDistance = 0.0f, float theU = 0.0f, float theV = 0.0f)
        : ray_intersection(theType, theDistance), u(theU), v(theV)
    {}
};

struct Ray;
struct Plane;
struct Triangle;
struct Sphere;
struct AABB;
struct OBB;
struct Frustum;

/**
 * @brief   Plane <-> AABB intersection.
 *
 * @param   plane   a vierkant::Plane
 * @param   aabb    a vierkant::AABB
 * @return  true if provided plane and aabb intersect.
 */
uint32_t intersect(const Plane &plane, const AABB &aabb);

inline uint32_t intersect(const AABB &aabb, const Plane &plane) { return intersect(plane, aabb); }

/********************************** Ray intersection tests ****************************************/

ray_intersection intersect(const Plane &plane, const Ray &ray);

inline ray_intersection intersect(const Ray &ray, const Plane &plane) { return intersect(plane, ray); }


ray_triangle_intersection intersect(const Triangle &triangle, const Ray &ray);

inline ray_triangle_intersection intersect(const Ray &ray, const Triangle &triangle)
{
    return intersect(triangle, ray);
}


ray_intersection intersect(const Sphere &sphere, const Ray &ray);

inline ray_intersection intersect(const Ray &ray, const Sphere &sphere) { return intersect(sphere, ray); }


ray_intersection intersect(const AABB &aabb, const Ray &ray);

inline ray_intersection intersect(const Ray &ray, const AABB &aabb) { return intersect(aabb, ray); }


ray_intersection intersect(const OBB &obb, const Ray &ray);

inline ray_intersection intersect(const Ray &ray, const OBB &obb) { return intersect(obb, ray); }

/********************************** Triangle intersection tests ****************************************/

uint32_t intersect(const Triangle &t, const AABB &b);

inline uint32_t intersect(const AABB &b, const Triangle &t) { return intersect(t, b); };

uint32_t intersect(const Triangle &t1, const Triangle &t2);

/********************************** Frustum intersection tests ****************************************/

uint32_t intersect(const Frustum &frustum, const glm::vec3 &p);

inline uint32_t intersect(const glm::vec3 &p, const Frustum &frustum) { return intersect(frustum, p); }

uint32_t intersect(const Frustum &frustum, const Sphere &s);

inline uint32_t intersect(const Sphere &s, const Frustum &frustum) { return intersect(frustum, s); }

uint32_t intersect(const Frustum &frustum, const AABB &aabb);

inline uint32_t intersect(const AABB &aabb, const Frustum &frustum) { return intersect(frustum, aabb); }

/**
 * @brief   compute_bounding_sphere can be used to compute a bounding sphere for an array of points.
 *
 * @param   vertices    array of 3d-points
 * @return  a bounding-sphere with its origin at the center of mass for provided points
 */
vierkant::Sphere compute_bounding_sphere(const std::vector<glm::vec3> &vertices);

/**
 * @brief   compute_aabb can be used to compute an axis-aligned bounding box for an array of points.
 *
 * @param   vertices    array of 3d-points
 * @return  an aabb for provided points
 */
vierkant::AABB compute_aabb(const std::vector<glm::vec3> &vertices);

/**
 * @brief   compute_centroid can be used to compute the center of mass for an array of points.
 *
 * @param   vertices    array of 3d-points
 * @return  the centroid / center of mass for provided points
 */
glm::vec3 compute_centroid(const std::vector<glm::vec3> &vertices);

struct Ray
{
    glm::vec3 origin;
    glm::vec3 direction;

    Ray(const glm::vec3 &origin_, const glm::vec3 &direction_) : origin(origin_), direction(normalize(direction_)) {}

    inline Ray &transform(const glm::mat4 &t)
    {
        origin = (t * glm::vec4(origin, 1.0f)).xyz();
        direction = normalize(glm::mat3(t) * direction);
        return *this;
    };

    [[nodiscard]] inline Ray transform(const glm::mat4 &t) const
    {
        Ray ret = *this;
        return ret.transform(t);
    };

    inline friend glm::vec3 operator*(const Ray &theRay, float t) { return theRay.origin + t * theRay.direction; }

    inline friend glm::vec3 operator*(float t, const Ray &theRay) { return theRay.origin + t * theRay.direction; }
};

struct Plane
{
    // Ax + By + Cz + D = 0
    glm::vec4 coefficients = glm::vec4(0, 1, 0, 0);

    Plane() = default;

    explicit Plane(const glm::vec4 &theCoefficients);

    Plane(float a, float b, float c, float d);

    Plane(const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2);

    Plane(const glm::vec3 &f, const glm::vec3 &n);

    [[nodiscard]] inline const glm::vec3 &normal() const { return *((glm::vec3 *) (&coefficients[0])); };

    [[nodiscard]] inline float distance(const glm::vec3 &p) const
    {
        return dot(p, coefficients.xyz()) + coefficients.w;
    };

    inline Plane &transform(const glm::mat4 &t)
    {
        coefficients = glm::inverseTranspose(t) * coefficients;
        return *this;
    };

    [[nodiscard]] inline Plane transform(const glm::mat4 &t) const
    {
        Plane ret = *this;
        return ret.transform(t);
    };
};

struct Triangle
{
    glm::vec3 v0, v1, v2;

    inline Triangle &transform(const glm::mat4 &t)
    {
        v0 = (t * glm::vec4(v0, 1.0f)).xyz();
        v1 = (t * glm::vec4(v1, 1.0f)).xyz();
        v2 = (t * glm::vec4(v2, 1.0f)).xyz();
        return *this;
    }

    [[nodiscard]] inline Triangle transform(const glm::mat4 &t) const
    {
        Triangle ret = *this;
        return ret.transform(t);
    }

    [[nodiscard]] inline glm::vec3 normal() const { return normalize(cross(v1 - v0, v2 - v0)); }

    inline const glm::vec3 &operator[](int i) const { return (&v0)[i]; }

    inline glm::vec3 &operator[](int i) { return (&v0)[i]; }
};

struct Sphere
{
    glm::vec3 center = {};
    float radius = 0.f;

    Sphere() = default;

    Sphere(const glm::vec3 &c, float r) : center(c), radius(r) {}

    inline Sphere &transform(const glm::mat4 &t)
    {
        center = (t * glm::vec4(center, 1.0f)).xyz();
        float max_len2 = std::max(std::max(glm::length2(t[0]), glm::length2(t[1])), glm::length2(t[2]));
        radius *= std::sqrt(max_len2);
        return *this;
    }

    [[nodiscard]] inline Sphere transform(const glm::mat4 &t) const
    {
        Sphere ret = *this;
        return ret.transform(t);
    }

    [[nodiscard]] inline uint32_t intersect(const glm::vec3 &thePoint) const
    {
        if(glm::length2(center - thePoint) > radius * radius) { return REJECT; }
        return INSIDE;
    }
};

/**
 * @brief   Cone defines a normal-cone, useful for backface culling.
 */
struct Cone
{
    //! cone-axis
    glm::vec3 axis = {0.f, 0.f, -1.f};

    //! cos(angle/2)
    float cutoff = 0.f;

    [[nodiscard]] inline Cone transform(const glm::mat4 &t) const
    {
        Cone ret = *this;
        ret.axis = glm::mat3(t) * ret.axis;
        return ret;
    }
};

/*
 *simple Axis aligned bounding box (AABB) structure
 */
struct AABB
{
    glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());

    AABB() = default;

    AABB(const glm::vec3 &theMin, const glm::vec3 &theMax) : min(theMin), max(theMax) {}

    [[nodiscard]] inline float width() const { return max.x - min.x; }

    [[nodiscard]] inline float height() const { return max.y - min.y; }

    [[nodiscard]] inline float depth() const { return max.z - min.z; }

    [[nodiscard]] inline glm::vec3 half_extents() const { return (max - min) / 2.f; }

    [[nodiscard]] inline glm::vec3 size() const { return (max - min); }

    [[nodiscard]] inline glm::vec3 center() const { return (max + min) / 2.f; }

    [[nodiscard]] inline bool valid() const { return glm::all(glm::greaterThanEqual(max, min)); }

    AABB operator+(const AABB &aabb) const
    {
        AABB ret(*this);
        ret += aabb;
        return ret;
    }

    AABB &operator+=(const AABB &aabb)
    {
        min = glm::min(min, aabb.min);
        max = glm::max(max, aabb.max);
        return *this;
    }

    AABB operator*(float grow_factor) const
    {
        AABB ret(*this);
        ret *= grow_factor;
        return ret;
    }

    AABB &operator*=(float grow_factor)
    {
        auto c = center();
        min = c + (min - c) * grow_factor;
        max = c + (max - c) * grow_factor;
        return *this;
    }

    inline explicit operator bool() const { return valid(); }

    inline bool operator==(const AABB &aabb) const { return min == aabb.min && max == aabb.max; }

    [[nodiscard]] AABB transform(const vierkant::transform_t &t) const;
    [[nodiscard]] AABB transform(const glm::mat4 &t) const;

    [[nodiscard]] inline uint32_t intersect(const glm::vec3 &point) const
    {
        if(point.x < min.x || point.x > max.x) { return REJECT; }
        if(point.y < min.y || point.y > max.y) { return REJECT; }
        if(point.z < min.z || point.z > max.z) { return REJECT; }
        return INSIDE;
    }

    [[nodiscard]] ray_intersection intersect(const Ray &ray) const;
};

struct OBB
{
    glm::vec3 center{};
    glm::mat3 axis{};
    glm::vec3 half_lengths{};

    OBB(const AABB &aabb, const glm::mat4 &t);

    OBB &transform(const glm::mat4 &t);

    [[nodiscard]] inline OBB transform(const glm::mat4 &t) const
    {
        OBB ret = *this;
        return ret.transform(t);
    }

    [[nodiscard]] inline bool contains(const glm::vec3 &p) const
    {
        // point in axis space
        glm::vec3 p_in_axis_space = axis * (p - center);
        return std::abs(p_in_axis_space.x) < half_lengths.x && std::abs(p_in_axis_space.y) < half_lengths.y &&
               std::abs(p_in_axis_space.z) < half_lengths.z;
    };
};

//! see https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
[[maybe_unused]] static std::array<glm::vec4, 6> get_view_planes(const glm::mat4x4 &mat)
{
    std::array<glm::vec4, 6> out{};
    for(auto i = 0; i < 3; ++i)
    {
        for(size_t j = 0; j < 2; ++j)
        {
            const float sign = j ? 1.f : -1.f;
            for(auto k = 0; k < 4; ++k) { out[2 * i + j][k] = mat[k][3] + sign * mat[k][i]; }
        }
    }

    // normalize plane; see Appendix A.2
    for(auto &&plane: out) { plane /= static_cast<float>(length(glm::vec3(plane.xyz()))); }
    return out;
}

struct Frustum
{
    //enum CLippingPlane
    //{
    //NEAR = 0,
    //FAR = 1,
    //LEFT = 2,
    //RIGHT = 3,
    //TOP = 4,
    //BOTTOM = 5,
    //NUM_PLANES = 6
    //};
    Plane planes[6];

    explicit Frustum(const glm::mat4 &view_projection);

    Frustum(float aspect, float fov, float near, float far);

    Frustum(float left, float right, float bottom, float top, float near, float far);

    inline Frustum &transform(const glm::mat4 &t)
    {
        for(Plane &p: planes) { p.transform(t); }
        return *this;
    }

    [[nodiscard]] inline Frustum transform(const glm::mat4 &t) const
    {
        Frustum ret = *this;
        return ret.transform(t);
    };
};

struct Capsule
{
    glm::vec3 center = {};
    float radius = 0.f;
    float height = 0.f;
};

}// namespace vierkant
