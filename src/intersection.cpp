#include "vierkant/intersection.hpp"
#include "triangle_intersection.h"
#include <crocore/crocore.hpp>

namespace vierkant
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ray_intersection intersect(const Plane &plane, const Ray &ray)
{
    // assuming normalized vectors
    float denom = glm::dot(-plane.normal(), ray.direction);

    if(std::abs(denom) > 1e-6)
    {
        float d = (plane.coefficients.w - glm::dot(ray.origin, -plane.normal())) / denom;
        if(d >= 0) return {INTERSECT, d};
    }
    return REJECT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::Sphere compute_bounding_sphere(const std::vector<glm::vec3> &vertices)
{
    Sphere ret;

#if 1
    // find extremum points along all 3 axes; for each axis we get a pair of points with min/max coordinates
    size_t pmin[3] = {0, 0, 0};
    size_t pmax[3] = {0, 0, 0};

    for(size_t i = 0; i < vertices.size(); ++i)
    {
        const auto &p = vertices[i];

        for(int axis = 0; axis < 3; ++axis)
        {
            pmin[axis] = (p[axis] < vertices[pmin[axis]][axis]) ? i : pmin[axis];
            pmax[axis] = (p[axis] > vertices[pmax[axis]][axis]) ? i : pmax[axis];
        }
    }

    // find the pair of points with largest distance
    float paxisd2 = 0;
    int paxis = 0;

    for(int axis = 0; axis < 3; ++axis)
    {
        const auto &p1 = vertices[pmin[axis]];
        const auto &p2 = vertices[pmax[axis]];
        float d2 = glm::length2(p2 - p1);

        if(d2 > paxisd2)
        {
            paxisd2 = d2;
            paxis = axis;
        }
    }

    // use the longest segment as the initial sphere diameter
    const auto &p1 = vertices[pmin[paxis]];
    const auto &p2 = vertices[pmax[paxis]];

    ret.center = (p1 + p2) / 2.f;
    ret.radius = sqrtf(paxisd2) / 2.f;

    // iteratively adjust the sphere up until all points fit
    for(const auto &p: vertices)
    {
        float d2 = glm::length2(p - ret.center);

        if(d2 > ret.radius * ret.radius)
        {
            float d = sqrtf(d2);
            assert(d > 0);

            float k = 0.5f + (ret.radius / d) / 2;

            ret.center = glm::mix(p, ret.center, k);
            ret.radius = (ret.radius + d) / 2;
        }
    }
#else
    ret.center = compute_centroid(vertices);

    for(const glm::vec3 &vertex: vertices) { ret.radius = std::max(ret.radius, glm::length2(ret.center - vertex)); }
    if(ret.radius > 0.f) { ret.radius = std::sqrt(ret.radius); }
#endif
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::AABB compute_aabb(const std::vector<glm::vec3> &vertices)
{
    AABB ret;

    for(const glm::vec3 &vertex: vertices)
    {
        ret.min = glm::min(ret.min, vertex);
        ret.max = glm::max(ret.max, vertex);
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

glm::vec3 compute_centroid(const std::vector<glm::vec3> &vertices) { return crocore::mean(vertices); }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ray_triangle_intersection intersect(const Triangle &triangle, const Ray &ray)
{
    glm::vec3 e1 = triangle.v1 - triangle.v0, e2 = triangle.v2 - triangle.v0;
    glm::vec3 pvec = glm::cross(ray.direction, e2);
    float det = glm::dot(e1, pvec);
    constexpr float epsilon = 10.0e-10f;
    if(det > -epsilon && det < epsilon) return REJECT;
    float inv_det = 1.0f / det;
    glm::vec3 tvec = ray.origin - triangle.v0;
    float u = inv_det * glm::dot(tvec, pvec);
    if(u < 0.0f || u > 1.0f) return REJECT;
    glm::vec3 qvec = glm::cross(tvec, e1);
    float v = glm::dot(ray.direction, qvec) * inv_det;
    if(v < 0.0f || (u + v) > 1.0f) return REJECT;
    return {INTERSECT, glm::dot(e2, qvec) * inv_det, u, v};
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ray_intersection intersect(const Sphere &sphere, const Ray &ray)
{
    glm::vec3 l = sphere.center - ray.origin;
    float s = glm::dot(l, ray.direction);
    float l2 = glm::dot(l, l);
    float r2 = sphere.radius * sphere.radius;
    if(s < 0 && l2 > r2) return REJECT;
    float m2 = l2 - s * s;
    if(m2 > r2) return REJECT;
    float q = sqrtf(r2 - m2);
    float t;
    if(l2 > r2) t = s - q;
    else
        t = s + q;
    return {INTERSECT, t};
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ray_intersection intersect(const OBB &obb, const Ray &ray)
{
    constexpr float epsilon = 1e-10f;

    float t_min = std::numeric_limits<float>::lowest();
    float t_max = std::numeric_limits<float>::max();
    glm::vec3 p = obb.center - ray.origin;

    for(int i = 0; i < 3; i++)
    {
        float e = glm::dot(obb.axis[i], p);
        float f = glm::dot(obb.axis[i], ray.direction);

        // this test avoids overflow from division
        if(std::abs(f) > epsilon)
        {
            float t1 = (e + obb.half_lengths[i]) / f;
            float t2 = (e - obb.half_lengths[i]) / f;

            if(t1 > t2) std::swap(t1, t2);
            if(t1 > t_min) t_min = t1;
            if(t2 < t_max) t_max = t2;
            if(t_min > t_max) return REJECT;
            if(t_max < 0) return REJECT;
        }
        else if((-e - obb.half_lengths[i]) > 0 || (-e + obb.half_lengths[i]) < 0) { return REJECT; }
    }
    if(t_min > 0) { return {INTERSECT, t_min}; }
    else { return {INTERSECT, t_max}; }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Plane::Plane(const glm::vec4 &theCoefficients)
{
    float len = glm::length(theCoefficients.xyz());
    coefficients = theCoefficients / len;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Plane::Plane(float a, float b, float c, float d)
{
    const glm::vec4 theCoefficients(a, b, c, d);
    float len = glm::length(theCoefficients.xyz());
    coefficients = theCoefficients / len;
}

Plane::Plane(const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2)
{
    glm::vec3 normal = glm::normalize(glm::cross(v2 - v0, v1 - v0));
    float distance = -glm::dot(v0, normal);
    coefficients = glm::vec4(normal, distance);
}

Plane::Plane(const glm::vec3 &theFoot, const glm::vec3 &theNormal)
{
    glm::vec3 normal = glm::normalize(theNormal);
    float distance = -glm::dot(theFoot, normal);
    coefficients = glm::vec4(normal, distance);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OBB::OBB(const AABB &aabb, const glm::mat4 &t)
{
    center = (t * glm::vec4(aabb.center(), 1.0f)).xyz();
    glm::vec3 scale(glm::length(t[0]), glm::length(t[1]), glm::length(t[2]));
    axis[0] = normalize(t[0].xyz());
    axis[1] = normalize(t[1].xyz());
    axis[2] = normalize(t[2].xyz());
    half_lengths = aabb.half_extents() * scale;
}

OBB &OBB::transform(const glm::mat4 &t)
{
    glm::vec3 scale(glm::length(t[0]), glm::length(t[1]), glm::length(t[2]));
    half_lengths *= scale;
    glm::mat3 normal_mat = glm::inverseTranspose(glm::mat3(t));
    axis[0] = normalize(normal_mat * axis[0]);
    axis[1] = normalize(normal_mat * axis[1]);
    axis[2] = normalize(normal_mat * axis[2]);
    center = t * glm::vec4(center, 1.f);
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline static void transform_aabb_helper(const auto &m, const glm::vec3 &t, vierkant::AABB &aabb)
{
    // invalid aabb, transform makes no sense
    if(!aabb.valid()) { return; }

    float a, b;

    // copy aabb into min and max array.
    glm::vec3 aMin = aabb.min;
    glm::vec3 aMax = aabb.max;

    // begin at t
    aabb.min = aabb.max = t;

    // Find extreme points by considering product of
    // min and max with each component of t.
    for(int j = 0; j < 3; j++)
    {
        for(int i = 0; i < 3; i++)
        {
            a = m[i][j] * aMin[i];
            b = m[i][j] * aMax[i];

            if(a < b)
            {
                aabb.min[j] += a;
                aabb.max[j] += b;
            }
            else
            {
                aabb.min[j] += b;
                aabb.max[j] += a;
            }
        }
    }
}

AABB AABB::transform(const vierkant::transform_t &t) const
{
    AABB ret = *this;
    auto m = glm::mat3(t.rotation);
    m[0] *= t.scale.x;
    m[1] *= t.scale.y;
    m[2] *= t.scale.z;
    transform_aabb_helper(m, t.translation, ret);
    return ret;
}

AABB AABB::transform(const glm::mat4 &t) const
{
    AABB ret = *this;
    transform_aabb_helper(t, t[3].xyz(), ret);
    return ret;
}

ray_intersection AABB::intersect(const Ray &ray) const
{
    OBB obb(*this, glm::mat4(1));
    return vierkant::intersect(obb, ray);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Frustum::Frustum(const glm::mat4 &view_projection)
{
    planes[0] = Plane(view_projection[2] + view_projection[3]);// near plane
    planes[1] = Plane(view_projection[3] - view_projection[2]);// far plane
    planes[2] = Plane(view_projection[0] + view_projection[3]);// left plane
    planes[3] = Plane(view_projection[3] - view_projection[0]);// right plane
    planes[4] = Plane(view_projection[3] - view_projection[1]);// top plane
    planes[5] = Plane(view_projection[1] + view_projection[3]);// bottom plane
}

Frustum::Frustum(float aspect, float fov, float near, float far)
{
    glm::mat4 t;
    constexpr glm::vec3 look_at = glm::vec3(0, 0, -1), eye = glm::vec3(0), side = glm::vec3(1, 0, 0),
                        up = glm::vec3(0, 1, 0);
    float angle_y = glm::half_pi<float>() - (fov / aspect) / 2.0f;
    float angle_x = glm::half_pi<float>() - fov / 2.0f;

    planes[0] = Plane(eye + (near * look_at), look_at);// near plane
    planes[1] = Plane(eye + (far * look_at), -look_at);// far plane

    t = glm::rotate(glm::mat4(1), angle_y, up);
    planes[2] = Plane(eye, look_at).transform(t);// left plane

    t = glm::rotate(glm::mat4(1), -angle_y, up);
    planes[3] = Plane(eye, look_at).transform(t);// right plane

    t = glm::rotate(glm::mat4(1), -angle_x, side);
    planes[4] = Plane(eye, look_at).transform(t);// top plane

    t = glm::rotate(glm::mat4(1), angle_x, side);
    planes[5] = Plane(eye, look_at).transform(t);// bottom plane
}

Frustum::Frustum(float left, float right, float bottom, float top, float near, float far)
{
    constexpr glm::vec3 lookAt = glm::vec3(0, 0, -1), eyePos = glm::vec3(0), side = glm::vec3(1, 0, 0),
                        up = glm::vec3(0, 1, 0);
    planes[0] = Plane(eyePos + (near * lookAt), lookAt);// near plane
    planes[1] = Plane(eyePos + (far * lookAt), -lookAt);// far plane
    planes[2] = Plane(eyePos + (left * side), side);    // left plane
    planes[3] = Plane(eyePos + (right * side), -side);  // right plane
    planes[4] = Plane(eyePos + (top * up), -up);        // top plane
    planes[5] = Plane(eyePos + (bottom * up), up);      // bottom plane
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t intersect(const Triangle &t, const AABB &b)
{
    auto box_center = b.center();
    auto box_half_extents = b.half_extents();
    return tri_box_overlap(glm::value_ptr(box_center), glm::value_ptr(box_half_extents),
                           reinterpret_cast<const float(*)[3]>(&t));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t intersect(const Triangle &t1, const Triangle &t2)
{
    return tri_tri_overlap_test_3d(glm::value_ptr(t1.v0), glm::value_ptr(t1.v1), glm::value_ptr(t1.v2),
                                   glm::value_ptr(t2.v0), glm::value_ptr(t2.v1), glm::value_ptr(t2.v2));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* used for fast AABB <-> Plane intersection test */
inline glm::vec3 pos_vertex(const AABB &aabb, const glm::vec3 &dir)
{
    glm::vec3 ret = aabb.min;
    if(dir.x >= 0) { ret.x = aabb.max.x; }
    if(dir.y >= 0) { ret.y = aabb.max.y; }
    if(dir.z >= 0) { ret.z = aabb.max.z; }
    return ret;
}

/* used for fast AABB <-> Plane intersection test */
inline glm::vec3 neg_vertex(const AABB &aabb, const glm::vec3 &dir)
{
    glm::vec3 ret = aabb.max;
    if(dir.x >= 0) { ret.x = aabb.min.x; }
    if(dir.y >= 0) { ret.y = aabb.min.y; }
    if(dir.z >= 0) { ret.z = aabb.min.z; }
    return ret;
}

uint32_t intersect(const Plane &plane, const AABB &aabb)
{
    // positive vertex outside ?
    if(plane.distance(pos_vertex(aabb, plane.normal())) < 0) { return REJECT; }

    // negative vertex outside ?
    if(plane.distance(neg_vertex(aabb, plane.normal())) < 0) { return INTERSECT; }

    return REJECT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t intersect(const Frustum &frustum, const glm::vec3 &p)
{
    for(const Plane &plane: frustum.planes)
    {
        if(plane.distance(p) < 0) { return REJECT; }
    }
    return INSIDE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t intersect(const Frustum &frustum, const Sphere &s)
{
    for(const Plane &plane: frustum.planes)
    {
        if(-plane.distance(s.center) > s.radius) { return REJECT; }
    }
    return INSIDE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t intersect(const Frustum &frustum, const AABB &aabb)
{
    uint32_t ret = INSIDE;

    for(const Plane &plane: frustum.planes)
    {
        //positive vertex outside ?
        if(plane.distance(pos_vertex(aabb, plane.normal())) < 0) { return REJECT; }

        //negative vertex outside ?
        else if(plane.distance(neg_vertex(aabb, plane.normal())) < 0) { ret = INTERSECT; }
    }
    return ret;
}

}// namespace vierkant
