#include <crocore/crocore.hpp>
#include "vierkant/intersection.hpp"
#include "triangle_intersection.h"

namespace vierkant
{

///////////////////////////////////////////////////////////////////////////////////////////////////

ray_intersection intersect(const Plane &plane, const Ray &ray)
{
    // assuming normalized vectors
    float denom = glm::dot(-plane.normal(), ray.direction);

    if(denom > 1e-6)
    {
        float d = (plane.coefficients.z - glm::dot(ray.origin, -plane.normal())) / denom;
        if(d >= 0) return {INTERSECT, d};
    }
    return REJECT;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

vierkant::AABB compute_aabb(const std::vector<glm::vec3> &vertices)
{
    AABB ret;

    for(const glm::vec3 &vertex : vertices)
    {
        ret.min = glm::min(ret.min, vertex);
        ret.max = glm::max(ret.max, vertex);
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

glm::vec3 compute_centroid(const std::vector<glm::vec3> &vertices)
{
    return crocore::mean<glm::vec3>(vertices);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

ray_triangle_intersection intersect(const Triangle &theTri, const Ray &theRay)
{
    glm::vec3 e1 = theTri.v1 - theTri.v0, e2 = theTri.v2 - theTri.v0;
    glm::vec3 pvec = glm::cross(theRay.direction, e2);
    float det = glm::dot(e1, pvec);
    constexpr float epsilon = 10.0e-10;
    if(det > -epsilon && det < epsilon) return REJECT;
    float inv_det = 1.0f / det;
    glm::vec3 tvec = theRay.origin - theTri.v0;
    float u = inv_det * glm::dot(tvec, pvec);
    if(u < 0.0f || u > 1.0f) return REJECT;
    glm::vec3 qvec = glm::cross(tvec, e1);
    float v = glm::dot(theRay.direction, qvec) * inv_det;
    if(v < 0.0f || (u + v) > 1.0f) return REJECT;
    return {INTERSECT, glm::dot(e2, qvec) * inv_det, u, v};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

ray_intersection intersect(const Sphere &theSphere, const Ray &theRay)
{
    glm::vec3 l = theSphere.center - theRay.origin;
    float s = glm::dot(l, theRay.direction);
    float l2 = glm::dot(l, l);
    float r2 = theSphere.radius * theSphere.radius;
    if(s < 0 && l2 > r2) return REJECT;
    float m2 = l2 - s * s;
    if(m2 > r2) return REJECT;
    float q = sqrtf(r2 - m2);
    float t;
    if(l2 > r2) t = s - q;
    else t = s + q;
    return {INTERSECT, t};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

ray_intersection intersect(const OBB &theOBB, const Ray &theRay)
{
    float t_min = std::numeric_limits<float>::lowest();
    float t_max = std::numeric_limits<float>::max();
    glm::vec3 p = theOBB.center - theRay.origin;

    for(int i = 0; i < 3; i++)
    {
        float e = glm::dot(theOBB.axis[i], p);
        float f = glm::dot(theOBB.axis[i], theRay.direction);

        // this test avoids overflow from division
        if(std::abs(f) > std::numeric_limits<float>::epsilon())
        {
            float t1 = (e + theOBB.half_lengths[i]) / f;
            float t2 = (e - theOBB.half_lengths[i]) / f;

            if(t1 > t2) std::swap(t1, t2);
            if(t1 > t_min) t_min = t1;
            if(t2 < t_max) t_max = t2;
            if(t_min > t_max) return REJECT;
            if(t_max < 0) return REJECT;
        }
        else if((-e - theOBB.half_lengths[i]) > 0 || (-e + theOBB.half_lengths[i]) < 0){ return REJECT; }
    }
    if(t_min > 0){ return {INTERSECT, t_min}; }
    else{ return {INTERSECT, t_max}; }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Plane::Plane(const glm::vec4 &theCoefficients)
{
    float len = glm::length(theCoefficients.xyz());
    coefficients = theCoefficients / len;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////

OBB::OBB(const AABB &aabb, const glm::mat4 &t)
{
    center = (t * glm::vec4(aabb.center(), 1.0f)).xyz();
    glm::vec3 scale(glm::length(t[0]),
                    glm::length(t[1]),
                    glm::length(t[2]));
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

///////////////////////////////////////////////////////////////////////////////

AABB &AABB::transform(const glm::mat4 &t)
{
    glm::vec3 aMin, aMax;
    float a, b;
    int i, j;

    // Copy box A into min and max array.
    aMin = min;
    aMax = max;

    // Begin at T.
    min = max = t[3].xyz();

    // Find extreme points by considering product of
    // min and max with each component of t.

    for(j = 0; j < 3; j++)
    {
        for(i = 0; i < 3; i++)
        {
            a = t[i][j] * aMin[i];
            b = t[i][j] * aMax[i];

            if(a < b)
            {
                min[j] += a;
                max[j] += b;
            }
            else
            {
                min[j] += b;
                max[j] += a;
            }
        }
    }
    return *this;
}

ray_intersection AABB::intersect(const Ray &ray) const
{
    OBB obb(*this, glm::mat4(1));
    return obb.intersect(ray);
}

///////////////////////////////////////////////////////////////////////////////

Frustum::Frustum(const glm::mat4 &the_VP_martix)
{
    planes[NEAR] = Plane(the_VP_martix[2] + the_VP_martix[3]); // near plane
    planes[FAR] = Plane(the_VP_martix[3] - the_VP_martix[2]); // far plane
    planes[LEFT] = Plane(the_VP_martix[0] + the_VP_martix[3]); // left plane
    planes[RIGHT] = Plane(the_VP_martix[3] - the_VP_martix[0]); // right plane
    planes[TOP] = Plane(the_VP_martix[3] - the_VP_martix[1]); // top plane
    planes[BOTTOM] = Plane(the_VP_martix[1] + the_VP_martix[3]); // bottom plane
}

Frustum::Frustum(float aspect, float fov, float near, float far)
{
    glm::mat4 t;
    const glm::vec3 look_at = glm::vec3(0, 0, -1), eye = glm::vec3(0),
            side = glm::vec3(1, 0, 0), up = glm::vec3(0, 1, 0);
    float angle_y = glm::radians(90.0f - aspect * fov / 2.0f);
    float angle_x = glm::radians(90.0f - (fov / 2.0f));

    planes[NEAR] = Plane(eye + (near * look_at), look_at); // near plane
    planes[FAR] = Plane(eye + (far * look_at), -look_at); // far plane

    t = glm::rotate(glm::mat4(1), angle_y, up);
    planes[LEFT] = Plane(eye, look_at).transform(t); // left plane

    t = glm::rotate(glm::mat4(1), -angle_y, up);
    planes[RIGHT] = Plane(eye, look_at).transform(t); // right plane

    t = glm::rotate(glm::mat4(1), -angle_x, side);
    planes[TOP] = Plane(eye, look_at).transform(t); // top plane

    t = glm::rotate(glm::mat4(1), angle_x, side);
    planes[BOTTOM] = Plane(eye, look_at).transform(t); // bottom plane
}

Frustum::Frustum(float left, float right, float bottom, float top,
                 float near, float far)
{
    static glm::vec3 lookAt = glm::vec3(0, 0, -1), eyePos = glm::vec3(0),
            side = glm::vec3(1, 0, 0), up = glm::vec3(0, 1, 0);
    planes[NEAR] = Plane(eyePos + (near * lookAt), lookAt); // near plane
    planes[FAR] = Plane(eyePos + (far * lookAt), -lookAt); // far plane
    planes[LEFT] = Plane(eyePos + (left * side), side); // left plane
    planes[RIGHT] = Plane(eyePos + (right * side), -side); // right plane
    planes[TOP] = Plane(eyePos + (top * up), -up); // top plane
    planes[BOTTOM] = Plane(eyePos + (bottom * up), up); // bottom plane

}

////////////////////////////////////////////////////////////////////////////////////

// Adapted from code found here: http://forum.openframeworks.cc/t/quad-warping-homography-without-opencv/3121/19
void gaussian_elimination(float *a, int n)
{
    int i = 0;
    int j = 0;
    int m = n - 1;

    while(i < m && j < n)
    {
        int maxi = i;
        for(int k = i + 1; k < m; ++k)
        {
            if(fabsf(a[k * n + j]) > fabsf(a[maxi * n + j])){ maxi = k; }
        }

        if(a[maxi * n + j] != 0)
        {
            if(i != maxi)
                for(int k = 0; k < n; k++)
                {
                    float aux = a[i * n + k];
                    a[i * n + k] = a[maxi * n + k];
                    a[maxi * n + k] = aux;
                }

            float a_ij = a[i * n + j];
            for(int k = 0; k < n; k++)
            {
                a[i * n + k] /= a_ij;
            }

            for(int u = i + 1; u < m; u++)
            {
                float a_uj = a[u * n + j];
                for(int k = 0; k < n; k++)
                {
                    a[u * n + k] -= a_uj * a[i * n + k];
                }
            }
            ++i;
        }
        ++j;
    }

    for(int i = m - 2; i >= 0; --i)
    {
        for(int j = i + 1; j < n - 1; j++)
        {
            a[i * n + m] -= a[i * n + j] * a[j * n + m];
        }
    }
}

// Adapted from code found here: http://forum.openframeworks.cc/t/quad-warping-homography-without-opencv/3121/19
glm::mat4 compute_homography(const glm::vec2 *src, const glm::vec2 *dst)
{
    float p[8][9] = {
            {-src[0][0], -src[0][1], -1, 0, 0, 0, src[0][0] * dst[0][0], src[0][1] * dst[0][0], -dst[0][0]}, // h11
            {0, 0, 0, -src[0][0], -src[0][1], -1, src[0][0] * dst[0][1], src[0][1] * dst[0][1], -dst[0][1]}, // h12
            {-src[1][0], -src[1][1], -1, 0, 0, 0, src[1][0] * dst[1][0], src[1][1] * dst[1][0], -dst[1][0]}, // h13
            {0, 0, 0, -src[1][0], -src[1][1], -1, src[1][0] * dst[1][1], src[1][1] * dst[1][1], -dst[1][1]}, // h21
            {-src[2][0], -src[2][1], -1, 0, 0, 0, src[2][0] * dst[2][0], src[2][1] * dst[2][0], -dst[2][0]}, // h22
            {0, 0, 0, -src[2][0], -src[2][1], -1, src[2][0] * dst[2][1], src[2][1] * dst[2][1], -dst[2][1]}, // h23
            {-src[3][0], -src[3][1], -1, 0, 0, 0, src[3][0] * dst[3][0], src[3][1] * dst[3][0], -dst[3][0]}, // h31
            {0, 0, 0, -src[3][0], -src[3][1], -1, src[3][0] * dst[3][1], src[3][1] * dst[3][1], -dst[3][1]}, // h32
    };
    gaussian_elimination(&p[0][0], 9);
    return glm::mat4(p[0][8], p[3][8], 0, p[6][8], p[1][8], p[4][8], 0, p[7][8], 0, 0, 1, 0, p[2][8], p[5][8], 0, 1);
}

bool intersect(const Triangle &t, const AABB &b)
{
    auto box_center = b.center();
    auto box_half_extents = b.half_extents();
    return tri_box_overlap(glm::value_ptr(box_center), glm::value_ptr(box_half_extents),
                           reinterpret_cast<const float (*)[3]>(&t));
}

bool intersect(const Triangle &t1, const Triangle &t2)
{
    return tri_tri_overlap_test_3d(glm::value_ptr(t1.v0), glm::value_ptr(t1.v1), glm::value_ptr(t1.v2),
                                   glm::value_ptr(t2.v0), glm::value_ptr(t2.v1), glm::value_ptr(t2.v2));;
}

}//namespace
