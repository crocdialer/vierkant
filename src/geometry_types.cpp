// __ ___ ____ _____ ______ _______ ________ _______ ______ _____ ____ ___ __
//
// Copyright (C) 2012-2016, Fabian Schmidt <crocdialer@googlemail.com>
//
// It is distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
// __ ___ ____ _____ ______ _______ ________ _______ ______ _____ ____ ___ __

#include "../include/vierkant/geometry_types.hpp"

namespace vierkant
{
namespace geom
{

/* fast AABB <-> Triangle test from Tomas Akenine-Möller */
int triBoxOverlap(float boxcenter[3], float boxhalfsize[3], float triverts[3][3]);

ray_intersection intersect(const Plane &plane, const Ray &ray)
{
    // assuming std::vectors are all normalized
    float denom = glm::dot(-plane.normal(), ray.direction);
    if(denom > 1e-6)
    {
        float d = (plane.coefficients.z - glm::dot(ray.origin, -plane.normal())) / denom;
        if(d >= 0) return ray_intersection(INTERSECT, d);
    }
    return REJECT;
}

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
    return ray_triangle_intersection(INTERSECT, glm::dot(e2, qvec) * inv_det, u, v);
}

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
    return ray_intersection(INTERSECT, t);
}

ray_intersection intersect(const OBB &theOBB, const Ray &theRay)
{
    float t_min = std::numeric_limits<float>::min();
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
        }else if((-e - theOBB.half_lengths[i]) > 0 || (-e + theOBB.half_lengths[i]) < 0)
        {
            return REJECT;
        }
    }

    if(t_min > 0)
        return ray_intersection(INTERSECT, t_min);
    else
        return ray_intersection(INTERSECT, t_max);
}

///////////////////////////////////////////////////////////////////////////////

glm::vec3 calculate_centroid(const std::vector<glm::vec3> &theVertices)
{
//    if(theVertices.empty())
//    {
//        LOG_TRACE << "Called calculateCentroid() on zero vertices, returned glm::vec3(0, 0, 0)";
//        return glm::vec3(0);
//    }
//    return kinski::mean<glm::vec3>(theVertices);
    return glm::vec3(0);
}

///////////////////////////////////////////////////////////////////////////////

Plane::Plane()
{
    coefficients = glm::vec4(0, 1, 0, 0);
}

Plane::Plane(const glm::vec4 &theCoefficients)
{
    float len = glm::length(theCoefficients.xyz());
    coefficients = theCoefficients / len;
}

Plane::Plane(float theA, float theB, float theC, float theD)
{
    const glm::vec4 theCoefficients(theA, theB, theC, theD);
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

OBB::OBB(const AABB &theAABB, const glm::mat4 &t)
{
    center = (t * glm::vec4(theAABB.center(), 1.0f)).xyz();
    glm::vec3 scale(glm::length(t[0]),
                    glm::length(t[1]),
                    glm::length(t[2]));
    axis[0] = normalize(t[0].xyz());
    axis[1] = normalize(t[1].xyz());
    axis[2] = normalize(t[2].xyz());
    half_lengths = theAABB.halfExtents() * scale;
}

OBB &OBB::transform(const glm::mat4 &t)
{
    glm::vec3 scale(glm::length(t[0]),
                    glm::length(t[1]),
                    glm::length(t[2]));
    half_lengths *= scale;

    glm::mat3 normal_mat = glm::inverseTranspose(glm::mat3(t));
    axis[0] = normalize(normal_mat * axis[0]);
    axis[1] = normalize(normal_mat * axis[1]);
    axis[2] = normalize(normal_mat * axis[2]);
    center += t[3].xyz();

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
            }else
            {
                min[j] += b;
                max[j] += a;
            }
        }
    }
    return *this;
}

ray_intersection AABB::intersect(const Ray &theRay) const
{
    OBB obb(*this, glm::mat4());
    return obb.intersect(theRay);
}

uint32_t AABB::intersect(const Triangle &t) const
{
    float triVerts[3][3] = {{t.v0[0], t.v0[1], t.v0[2]},
                            {t.v1[0], t.v1[1], t.v1[2]},
                            {t.v2[0], t.v2[1], t.v2[2]}
    };
    return triBoxOverlap(&center()[0], &halfExtents()[0], triVerts);
}

///////////////////////////////////////////////////////////////////////////////

AABB compute_aabb(const std::vector<glm::vec3> &theVertices)
{
    if(theVertices.empty()){ return AABB(); }

    AABB ret = AABB(glm::vec3(std::numeric_limits<float>::max()),
                    glm::vec3(std::numeric_limits<float>::min()));

    for(const glm::vec3 &vertex : theVertices)
    {
        // X
        if(vertex.x < ret.min.x){ ret.min.x = vertex.x; }
        else if(vertex.x > ret.max.x){ ret.max.x = vertex.x; }
        // Y
        if(vertex.y < ret.min.y){ ret.min.y = vertex.y; }
        else if(vertex.y > ret.max.y){ ret.max.y = vertex.y; }
        // Z
        if(vertex.z < ret.min.z){ ret.min.z = vertex.z; }
        else if(vertex.z > ret.max.z){ ret.max.z = vertex.z; }
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////

Frustum::Frustum(const glm::mat4 &the_VP_martix)
{
    planes[0] = Plane(the_VP_martix[2] + the_VP_martix[3]); // near plane
    planes[1] = Plane(the_VP_martix[3] - the_VP_martix[2]); // far plane
    planes[2] = Plane(the_VP_martix[0] + the_VP_martix[3]); // left plane
    planes[3] = Plane(the_VP_martix[3] - the_VP_martix[0]); // right plane
    planes[4] = Plane(the_VP_martix[3] - the_VP_martix[1]); // top plane
    planes[5] = Plane(the_VP_martix[1] + the_VP_martix[3]); // bottom plane
}

Frustum::Frustum(float aspect, float fov, float near, float far)
{
    glm::mat4 t;
    constexpr glm::vec3 look_at = glm::vec3(0, 0, -1), eye = glm::vec3(0),
            side = glm::vec3(1, 0, 0), up = glm::vec3(0, 1, 0);
    float angle_y = glm::radians(90.0f - aspect * fov / 2.0f);
    float angle_x = glm::radians(90.0f - (fov / 2.0f));

    planes[0] = Plane(eye + (near * look_at), look_at); // near plane
    planes[1] = Plane(eye + (far * look_at), -look_at); // far plane

    t = glm::rotate(glm::mat4(), angle_y, up);
    planes[2] = Plane(eye, look_at).transform(t); // left plane

    t = glm::rotate(glm::mat4(), -angle_y, up);
    planes[3] = Plane(eye, look_at).transform(t); // right plane

    t = glm::rotate(glm::mat4(), -angle_x, side);
    planes[4] = Plane(eye, look_at).transform(t); // top plane

    t = glm::rotate(glm::mat4(), angle_x, side);
    planes[5] = Plane(eye, look_at).transform(t); // bottom plane
}

Frustum::Frustum(float left, float right, float bottom, float top,
                 float near, float far)
{
    static glm::vec3 lookAt = glm::vec3(0, 0, -1), eyePos = glm::vec3(0),
            side = glm::vec3(1, 0, 0), up = glm::vec3(0, 1, 0);
    planes[0] = Plane(eyePos + (near * lookAt), lookAt); // near plane
    planes[1] = Plane(eyePos + (far * lookAt), -lookAt); // far plane
    planes[2] = Plane(eyePos + (left * side), side); // left plane
    planes[3] = Plane(eyePos + (right * side), -side); // right plane
    planes[4] = Plane(eyePos + (top * up), -up); // top plane
    planes[5] = Plane(eyePos + (bottom * up), up); // bottom plane

}

////////////////////////////////////////////////////////////////////////////////////

// Adapted from code found here: http://forum.openframeworks.cc/t/quad-warping-homography-without-opencv/3121/19
void gaussian_elimination(float* a, int n)
{
    int i = 0;
    int j = 0;
    int m = n - 1;

    while(i < m && j < n)
    {
        int maxi = i;
        for(int k = i + 1; k < m; ++k)
        {
            if(fabs(a[k * n + j]) > fabs(a[maxi * n + j])){ maxi = k; }
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
glm::mat4 calculate_homography(const glm::vec2 src[4], const glm::vec2 dst[4])
{
    float p[8][9] =
            {
                    {-src[0][0], -src[0][1], -1, 0,          0,          0,  src[0][0] * dst[0][0],
                                                                                                    src[0][1] *
                                                                                                    dst[0][0], -dst[0][0]}, // h11
                    {0,          0,          0,  -src[0][0], -src[0][1], -1, src[0][0] * dst[0][1], src[0][1] *
                                                                                                    dst[0][1], -dst[0][1]}, // h12
                    {-src[1][0], -src[1][1], -1, 0,          0,          0,  src[1][0] * dst[1][0], src[1][1] *
                                                                                                    dst[1][0], -dst[1][0]}, // h13
                    {0,          0,          0,  -src[1][0], -src[1][1], -1, src[1][0] * dst[1][1], src[1][1] *
                                                                                                    dst[1][1], -dst[1][1]}, // h21
                    {-src[2][0], -src[2][1], -1, 0,          0,          0,  src[2][0] * dst[2][0], src[2][1] *
                                                                                                    dst[2][0], -dst[2][0]}, // h22
                    {0,          0,          0,  -src[2][0], -src[2][1], -1, src[2][0] * dst[2][1], src[2][1] *
                                                                                                    dst[2][1], -dst[2][1]}, // h23
                    {-src[3][0], -src[3][1], -1, 0,          0,          0,  src[3][0] * dst[3][0], src[3][1] *
                                                                                                    dst[3][0], -dst[3][0]}, // h31
                    {0,          0,          0,  -src[3][0], -src[3][1], -1, src[3][0] * dst[3][1], src[3][1] *
                                                                                                    dst[3][1], -dst[3][1]}, // h32
            };
    gaussian_elimination(&p[0][0], 9);
    return glm::mat4(p[0][8], p[3][8], 0, p[6][8], p[1][8], p[4][8], 0, p[7][8], 0, 0, 1, 0, p[2][8], p[5][8], 0, 1);
}

///////////////////////////////////////////////////////////////////////////////

/********************************************************/

/* AABB-triangle overlap test code                      */

/* by Tomas Akenine-Möller                              */

/* Function: int triBoxOverlap(float boxcenter[3],      */

/*          float boxhalfsize[3],float triverts[3][3]); */

/* History:                                             */

/*   2001-03-05: released the code in its first version */

/*   2001-06-18: changed the order of the tests, faster */

/*                                                      */

/* Acknowledgement: Many thanks to Pierre Terdiman for  */

/* suggestions and discussions on how to optimize code. */

/* Thanks to David Hunt for finding a ">="-bug!         */

/********************************************************/

#define X 0
#define Y 1
#define Z 2

#define CROSS(dest, v1, v2) \
dest[0]=v1[1]*v2[2]-v1[2]*v2[1]; \
dest[1]=v1[2]*v2[0]-v1[0]*v2[2]; \
dest[2]=v1[0]*v2[1]-v1[1]*v2[0];

#define DOT(v1, v2) (v1[0]*v2[0]+v1[1]*v2[1]+v1[2]*v2[2])

#define SUB(dest, v1, v2) \
dest[0]=v1[0]-v2[0]; \
dest[1]=v1[1]-v2[1]; \
dest[2]=v1[2]-v2[2];

#define FINDMINMAX(x0, x1, x2, min, max) \
min = max = x0;   \
if(x1<min) min=x1;\
if(x1>max) max=x1;\
if(x2<min) min=x2;\
if(x2>max) max=x2;

int planeBoxOverlap(float normal[3], float vert[3], float maxbox[3])    // -NJMP-
{
    int q;
    float vmin[3], vmax[3], v;
    for(q = X; q <= Z; q++)
    {
        v = vert[q];                    // -NJMP-
        if(normal[q] > 0.0f)
        {
            vmin[q] = -maxbox[q] - v;    // -NJMP-
            vmax[q] = maxbox[q] - v;    // -NJMP-
        }else
        {
            vmin[q] = maxbox[q] - v;    // -NJMP-
            vmax[q] = -maxbox[q] - v;    // -NJMP-
        }
    }

    if(DOT(normal, vmin) > 0.0f) return 0;    // -NJMP-
    if(DOT(normal, vmax) >= 0.0f) return 1;    // -NJMP-

    return 0;
}

/*======================== X-tests ========================*/

#define AXISTEST_X01(a, b, fa, fb)               \
p0 = a*v0[Y] - b*v0[Z];                       \
p2 = a*v2[Y] - b*v2[Z];                       \
if(p0<p2) {min=p0; max=p2;} else {min=p2; max=p0;} \
rad = fa * boxhalfsize[Y] + fb * boxhalfsize[Z];   \
if(min>rad || max<-rad) return 0;


#define AXISTEST_X2(a, b, fa, fb)               \
p0 = a*v0[Y] - b*v0[Z];                       \
p1 = a*v1[Y] - b*v1[Z];                       \
if(p0<p1) {min=p0; max=p1;} else {min=p1; max=p0;} \
rad = fa * boxhalfsize[Y] + fb * boxhalfsize[Z];   \
if(min>rad || max<-rad) return 0;

/*======================== Y-tests ========================*/

#define AXISTEST_Y02(a, b, fa, fb)               \
p0 = -a*v0[X] + b*v0[Z];                   \
p2 = -a*v2[X] + b*v2[Z];                       \
if(p0<p2) {min=p0; max=p2;} else {min=p2; max=p0;} \
rad = fa * boxhalfsize[X] + fb * boxhalfsize[Z];   \
if(min>rad || max<-rad) return 0;


#define AXISTEST_Y1(a, b, fa, fb)               \
p0 = -a*v0[X] + b*v0[Z];                   \
p1 = -a*v1[X] + b*v1[Z];                       \
if(p0<p1) {min=p0; max=p1;} else {min=p1; max=p0;} \
rad = fa * boxhalfsize[X] + fb * boxhalfsize[Z];   \
if(min>rad || max<-rad) return 0;

/*======================== Z-tests ========================*/

#define AXISTEST_Z12(a, b, fa, fb)               \
p1 = a*v1[X] - b*v1[Y];                       \
p2 = a*v2[X] - b*v2[Y];                       \
if(p2<p1) {min=p2; max=p1;} else {min=p1; max=p2;} \
rad = fa * boxhalfsize[X] + fb * boxhalfsize[Y];   \
if(min>rad || max<-rad) return 0;


#define AXISTEST_Z0(a, b, fa, fb)               \
p0 = a*v0[X] - b*v0[Y];                   \
p1 = a*v1[X] - b*v1[Y];                       \
if(p0<p1) {min=p0; max=p1;} else {min=p1; max=p0;} \
rad = fa * boxhalfsize[X] + fb * boxhalfsize[Y];   \
if(min>rad || max<-rad) return 0;

int triBoxOverlap(float boxcenter[3], float boxhalfsize[3], float triverts[3][3])
{

    /*    use separating axis theorem to test overlap between triangle and box */
    /*    need to test for overlap in these directions: */
    /*    1) the {x,y,z}-directions (actually, since we use the AABB of the triangle */
    /*       we do not even need to test these) */
    /*    2) normal of the triangle */
    /*    3) crossproduct(edge from tri, {x,y,z}-directin) */
    /*       this gives 3x3=9 more tests */

    float v0[3], v1[3], v2[3];
    //   float axis[3];
    float min, max, p0, p1, p2, rad, fex, fey, fez;        // -NJMP- "d" local variable removed
    float normal[3], e0[3], e1[3], e2[3];

    /* This is the fastest branch on Sun */
    /* move everything so that the boxcenter is in (0,0,0) */

    SUB(v0, triverts[0], boxcenter);
    SUB(v1, triverts[1], boxcenter);
    SUB(v2, triverts[2], boxcenter);

    /* compute triangle edges */

    SUB(e0, v1, v0);      /* tri edge 0 */
    SUB(e1, v2, v1);      /* tri edge 1 */
    SUB(e2, v0, v2);      /* tri edge 2 */

    /* Bullet 3:  */

    /*  test the 9 tests first (this was faster) */
    fex = fabsf(e0[X]);
    fey = fabsf(e0[Y]);
    fez = fabsf(e0[Z]);

    AXISTEST_X01(e0[Z], e0[Y], fez, fey);
    AXISTEST_Y02(e0[Z], e0[X], fez, fex);
    AXISTEST_Z12(e0[Y], e0[X], fey, fex);

    fex = fabsf(e1[X]);
    fey = fabsf(e1[Y]);
    fez = fabsf(e1[Z]);

    AXISTEST_X01(e1[Z], e1[Y], fez, fey);
    AXISTEST_Y02(e1[Z], e1[X], fez, fex);
    AXISTEST_Z0(e1[Y], e1[X], fey, fex);

    fex = fabsf(e2[X]);
    fey = fabsf(e2[Y]);
    fez = fabsf(e2[Z]);

    AXISTEST_X2(e2[Z], e2[Y], fez, fey);
    AXISTEST_Y1(e2[Z], e2[X], fez, fex);
    AXISTEST_Z12(e2[Y], e2[X], fey, fex);



    /* Bullet 1: */

    /*  first test overlap in the {x,y,z}-directions */
    /*  find min, max of the triangle each direction, and test for overlap in */
    /*  that direction -- this is equivalent to testing a minimal AABB around */
    /*  the triangle against the AABB */

    /* test in X-direction */

    FINDMINMAX(v0[X], v1[X], v2[X], min, max);

    if(min > boxhalfsize[X] || max < -boxhalfsize[X]) return REJECT;

    /* test in Y-direction */
    FINDMINMAX(v0[Y], v1[Y], v2[Y], min, max);

    if(min > boxhalfsize[Y] || max < -boxhalfsize[Y]) return REJECT;

    /* test in Z-direction */

    FINDMINMAX(v0[Z], v1[Z], v2[Z], min, max);

    if(min > boxhalfsize[Z] || max < -boxhalfsize[Z]) return REJECT;

    /* Bullet 2: */
    /*  test if the box intersects the plane of the triangle */
    /*  compute plane equation of triangle: normal*x+d=0 */
    CROSS(normal, e0, e1);
    // -NJMP- (line removed here)
    if(!planeBoxOverlap(normal, v0, boxhalfsize)) return REJECT;    // -NJMP-

    return INTERSECT;   /* box and triangle overlaps */
}

}
}//namespace
