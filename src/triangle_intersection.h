#pragma once
/*
 *  Triangle-Triangle Overlap Test Routines
 *  July, 2002
 *  Updated December 2003
 *  Modified by Andreas Schuh in September 2016
 *
 *  This file contains C implementation of algorithms for
 *  performing two and three-dimensional triangle-triangle intersection test
 *  The algorithms and underlying theory are described in
 *
 * "Fast and Robust Triangle-Triangle Overlap Test
 *  Using Orientation Predicates"  P. Guigue - O. Devillers
 *
 *  Journal of Graphics Tools, 8(1), 2003
 *
 *  Several geometric predicates are defined.  Their parameters are all
 *  points.  Each point is an array of two or three real precision
 *  floating point numbers. The geometric predicates implemented in
 *  this file are:
 *
 *    int tri_tri_overlap_test_3d(p1,q1,r1,p2,q2,r2)
 *    int tri_tri_overlap_test_2d(p1,q1,r1,p2,q2,r2)
 *
 *    int tri_tri_intersection_test_3d(p1,q1,r1,p2,q2,r2,
 *                                     coplanar,source,target)
 *
 *       is a version that computes the segment of intersection when
 *       the triangles overlap (and are not coplanar)
 *
 *    each function returns 1 if the triangles (including their
 *    boundary) intersect, otherwise 0
 *
 *
 *  Other information are available from the Web page
 *  http:<i>//www.acm.org/jgt/papers/GuigueDevillers03/
 *
 */

// Andreas: Copied this source file on 6/6/2016 from
// https://raw.githubusercontent.com/benardp/contours/755cf3c5086d58d07928934384dd185604ea2c2c/freestyle/view_map/triangle_triangle_intersection.c
// and modified to use eps and zero constants for chosen real data type.
// Clamp dot products / signed distance to triangle plane to zero when
// absolute value is less than abs to better detect coplanarity.
typedef float real;

static const real zero = real(0);
static const real eps = static_cast<real>(1e-12);


#define ZERO_TEST(x)  (abs(x) <= eps)


/* function prototype */
inline int tri_tri_overlap_test_3d(const real p1[3], const real q1[3], const real r1[3],
                                   const real p2[3], const real q2[3], const real r2[3]);

inline int coplanar_tri_tri3d(const real p1[3], const real q1[3], const real r1[3],
                              const real p2[3], const real q2[3], const real r2[3],
                              const real N1[3], const real N2[3]);

inline int tri_tri_overlap_test_2d(const real p1[2], const real q1[2], const real r1[2],
                                   const real p2[2], const real q2[2], const real r2[2]);

/* coplanar returns whether the triangles are coplanar
*  source and target are the endpoints of the segment of intersection if it exists
*/
inline int tri_tri_intersection_test_3d(const real p1[3], const real q1[3], const real r1[3],
                                        const real p2[3], const real q2[3], const real r2[3],
                                        int *coplanar,
                                        real source[3], real target[3]);

/* fast AABB <-> Triangle test from Tomas Akenine-Möller */
int tri_box_overlap(const float boxcenter[3], const float boxhalfsize[3], const float triverts[3][3]);

#define CROSS(dest, v1, v2)   dest[0] = v1[1]*v2[2] - v1[2]*v2[1]; \
 dest[1] = v1[2]*v2[0] - v1[0]*v2[2]; \
 dest[2] = v1[0]*v2[1] - v1[1]*v2[0];

#define DOT(v1, v2) (v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2])


#define SUB(dest, v1, v2)   dest[0] = v1[0] - v2[0]; \
 dest[1] = v1[1] - v2[1]; \
 dest[2] = v1[2] - v2[2];


#define SCALAR(dest, alpha, v)   dest[0] = alpha * v[0]; \
 dest[1] = alpha * v[1]; \
 dest[2] = alpha * v[2];



// Andreas: Changed conditions from DOT(v1,N1) > zero to DOT(v1,N1) >= -eps
#define CHECK_MIN_MAX(p1, q1, r1, p2, q2, r2) \
 { \
 SUB(v1,p2,q1) \
 SUB(v2,p1,q1) \
 CROSS(N1,v1,v2) \
 SUB(v1,q2,q1) \
 if (DOT(v1,N1) >= -eps) return 0; \
 SUB(v1,p2,p1) \
 SUB(v2,r1,p1) \
 CROSS(N1,v1,v2) \
 SUB(v1,r2,p1) \
 if (DOT(v1,N1) >= -eps) return 0; \
 return 1; \
 }


/* Permutation in a canonical form of T2's vertices */
#define TRI_TRI_3D(p1, q1, r1, p2, q2, r2, dp2, dq2, dr2) \
 { \
 if (dp2 > zero) { \
 if      (dq2 > zero) CHECK_MIN_MAX(p1,r1,q1,r2,p2,q2) \
 else if (dr2 > zero) CHECK_MIN_MAX(p1,r1,q1,q2,r2,p2) \
 else                 CHECK_MIN_MAX(p1,q1,r1,p2,q2,r2) \
 } else if (dp2 < zero) { \
 if      (dq2 < zero) CHECK_MIN_MAX(p1,q1,r1,r2,p2,q2) \
 else if (dr2 < zero) CHECK_MIN_MAX(p1,q1,r1,q2,r2,p2) \
 else                 CHECK_MIN_MAX(p1,r1,q1,p2,q2,r2) \
 } else { \
 if (dq2 < zero) { \
 if (dr2 >= zero) CHECK_MIN_MAX(p1,r1,q1,q2,r2,p2) \
 else             CHECK_MIN_MAX(p1,q1,r1,p2,q2,r2) \
 } else if (dq2 > zero) { \
 if (dr2 > zero) CHECK_MIN_MAX(p1,r1,q1,p2,q2,r2) \
 else            CHECK_MIN_MAX(p1,q1,r1,q2,r2,p2) \
 } else { \
 if      (dr2 > zero) CHECK_MIN_MAX(p1,q1,r1,r2,p2,q2) \
 else if (dr2 < zero) CHECK_MIN_MAX(p1,r1,q1,r2,p2,q2) \
 else return coplanar_tri_tri3d(p1,q1,r1,p2,q2,r2,N1,N2); \
 } \
 } \
 }


/*
*
*  Three-dimensional Triangle-Triangle Overlap Test
*
*/


int tri_tri_overlap_test_3d(const real p1[3], const real q1[3], const real r1[3],
                            const real p2[3], const real q2[3], const real r2[3])
{
    real dp1, dq1, dr1, dp2, dq2, dr2;
    real v1[3], v2[3];
    real N1[3], N2[3];

    // Compute distance signs  of p1, q1 and r1 to the plane of triangle(p2,q2,r2)
    SUB(v1, p2, r2)
    SUB(v2, q2, r2)
    CROSS(N2, v1, v2)

    SUB(v1, p1, r2)
    dp1 = DOT(v1, N2);
    SUB(v1, q1, r2)
    dq1 = DOT(v1, N2);
    SUB(v1, r1, r2)
    dr1 = DOT(v1, N2);

    if(((dp1 * dq1) > zero) && ((dp1 * dr1) > zero)){ return 0; }

    // Compute distance signs  of p2, q2 and r2 to the plane of triangle(p1,q1,r1)
    SUB(v1, q1, p1)
    SUB(v2, r1, p1)
    CROSS(N1, v1, v2)

    SUB(v1, p2, r1)
    dp2 = DOT(v1, N1);
    SUB(v1, q2, r1)
    dq2 = DOT(v1, N1);
    SUB(v1, r2, r1)
    dr2 = DOT(v1, N1);

    if(((dp2 * dq2) > zero) && ((dp2 * dr2) > zero)){ return 0; }

    // Permutation in a canonical form of T1's vertices
    if(ZERO_TEST(dp1)){ dp1 = zero; }
    if(ZERO_TEST(dq1)){ dq1 = zero; }
    if(ZERO_TEST(dr1)){ dr1 = zero; }

    if(ZERO_TEST(dp2)){ dp2 = zero; }
    if(ZERO_TEST(dq2)){ dq2 = zero; }
    if(ZERO_TEST(dr2)){ dr2 = zero; }

    if(dp1 > zero)
    {
        if(dq1 > zero)TRI_TRI_3D(r1, p1, q1, p2, r2, q2, dp2, dr2, dq2)
        else if(dr1 > zero)TRI_TRI_3D(q1, r1, p1, p2, r2, q2, dp2, dr2, dq2)
        else TRI_TRI_3D(p1, q1, r1, p2, q2, r2, dp2, dq2, dr2)
    }
    else if(dp1 < zero)
    {
        if(dq1 < zero)TRI_TRI_3D(r1, p1, q1, p2, q2, r2, dp2, dq2, dr2)
        else if(dr1 < zero)TRI_TRI_3D(q1, r1, p1, p2, q2, r2, dp2, dq2, dr2)
        else TRI_TRI_3D(p1, q1, r1, p2, r2, q2, dp2, dr2, dq2)
    }
    else
    {
        if(dq1 < zero)
        {
            if(dr1 >= zero)TRI_TRI_3D(q1, r1, p1, p2, r2, q2, dp2, dr2, dq2)
            else TRI_TRI_3D(p1, q1, r1, p2, q2, r2, dp2, dq2, dr2)
        }
        else if(dq1 > zero)
        {
            if(dr1 > zero)TRI_TRI_3D(p1, q1, r1, p2, r2, q2, dp2, dr2, dq2)
            else TRI_TRI_3D(q1, r1, p1, p2, q2, r2, dp2, dq2, dr2)
        }
        else
        {
            if(dr1 > zero)TRI_TRI_3D(r1, p1, q1, p2, q2, r2, dp2, dq2, dr2)
            else if(dr1 < zero)TRI_TRI_3D(r1, p1, q1, p2, r2, q2, dp2, dr2, dq2)
            else{ return coplanar_tri_tri3d(p1, q1, r1, p2, q2, r2, N1, N2); }
        }
    }
}

int coplanar_tri_tri3d(const real p1[3], const real q1[3], const real r1[3],
                       const real p2[3], const real q2[3], const real r2[3],
                       const real N1[3], const real /*N2*/[3])
{
    real P1[2], Q1[2], R1[2];
    real P2[2], Q2[2], R2[2];

    real n_x, n_y, n_z;

    n_x = ((N1[0] < zero) ? -N1[0] : N1[0]);
    n_y = ((N1[1] < zero) ? -N1[1] : N1[1]);
    n_z = ((N1[2] < zero) ? -N1[2] : N1[2]);

    /* Projection of the triangles in 3D onto 2D such that the area of
       the projection is maximized. */

    // Project onto plane YZ
    if((n_x > n_z) && (n_x >= n_y))
    {

        P1[0] = q1[2];
        P1[1] = q1[1];
        Q1[0] = p1[2];
        Q1[1] = p1[1];
        R1[0] = r1[2];
        R1[1] = r1[1];

        P2[0] = q2[2];
        P2[1] = q2[1];
        Q2[0] = p2[2];
        Q2[1] = p2[1];
        R2[0] = r2[2];
        R2[1] = r2[1];

        // Project onto plane XZ
    }
    else if((n_y > n_z) && (n_y >= n_x))
    {

        P1[0] = q1[0];
        P1[1] = q1[2];
        Q1[0] = p1[0];
        Q1[1] = p1[2];
        R1[0] = r1[0];
        R1[1] = r1[2];

        P2[0] = q2[0];
        P2[1] = q2[2];
        Q2[0] = p2[0];
        Q2[1] = p2[2];
        R2[0] = r2[0];
        R2[1] = r2[2];

        // Project onto plane XY
    }
    else
    {

        P1[0] = p1[0];
        P1[1] = p1[1];
        Q1[0] = q1[0];
        Q1[1] = q1[1];
        R1[0] = r1[0];
        R1[1] = r1[1];

        P2[0] = p2[0];
        P2[1] = p2[1];
        Q2[0] = q2[0];
        Q2[1] = q2[1];
        R2[0] = r2[0];
        R2[1] = r2[1];

    }

    return tri_tri_overlap_test_2d(P1, Q1, R1, P2, Q2, R2);
};



/*
*
*  Three-dimensional Triangle-Triangle Intersection
*
*/

/*
   This macro is called when the triangles surely intersect
   It constructs the segment of intersection of the two triangles
   if they are not coplanar.
*/

#define CONSTRUCT_INTERSECTION(p1, q1, r1, p2, q2, r2) \
 { \
 SUB(v1,q1,p1) \
 SUB(v2,r2,p1) \
 CROSS(N,v1,v2) \
 SUB(v,p2,p1) \
 if (DOT(v,N) > zero) {\
 SUB(v1,r1,p1) \
 CROSS(N,v1,v2) \
 if (DOT(v,N) <= zero) { \
 SUB(v2,q2,p1) \
 CROSS(N,v1,v2) \
 if (DOT(v,N) > zero) { \
 SUB(v1,p1,p2) \
 SUB(v2,p1,r1) \
 alpha = DOT(v1,N2) / DOT(v2,N2); \
 SCALAR(v1,alpha,v2) \
 SUB(source,p1,v1) \
 SUB(v1,p2,p1) \
 SUB(v2,p2,r2) \
 alpha = DOT(v1,N1) / DOT(v2,N1); \
 SCALAR(v1,alpha,v2) \
 SUB(target,p2,v1) \
 return 1; \
 } else { \
 SUB(v1,p2,p1) \
 SUB(v2,p2,q2) \
 alpha = DOT(v1,N1) / DOT(v2,N1); \
 SCALAR(v1,alpha,v2) \
 SUB(source,p2,v1) \
 SUB(v1,p2,p1) \
 SUB(v2,p2,r2) \
 alpha = DOT(v1,N1) / DOT(v2,N1); \
 SCALAR(v1,alpha,v2) \
 SUB(target,p2,v1) \
 return 1; \
 } \
 } else { \
 return 0; \
 } \
 } else { \
 SUB(v2,q2,p1) \
 CROSS(N,v1,v2) \
 if (DOT(v,N) < zero) { \
 return 0; \
 } else { \
 SUB(v1,r1,p1) \
 CROSS(N,v1,v2) \
 if (DOT(v,N) >= zero) { \
 SUB(v1,p1,p2) \
 SUB(v2,p1,r1) \
 alpha = DOT(v1,N2) / DOT(v2,N2); \
 SCALAR(v1,alpha,v2) \
 SUB(source,p1,v1) \
 SUB(v1,p1,p2) \
 SUB(v2,p1,q1) \
 alpha = DOT(v1,N2) / DOT(v2,N2); \
 SCALAR(v1,alpha,v2) \
 SUB(target,p1,v1) \
 return 1; \
 } else { \
 SUB(v1,p2,p1) \
 SUB(v2,p2,q2) \
 alpha = DOT(v1,N1) / DOT(v2,N1); \
 SCALAR(v1,alpha,v2) \
 SUB(source,p2,v1) \
 SUB(v1,p1,p2) \
 SUB(v2,p1,q1) \
 alpha = DOT(v1,N2) / DOT(v2,N2); \
 SCALAR(v1,alpha,v2) \
 SUB(target,p1,v1) \
 return 1; \
 } \
 } \
 } \
 }


#define TRI_TRI_INTER_3D(p1, q1, r1, p2, q2, r2, dp2, dq2, dr2) \
 { \
 if (dp2 > zero) { \
 if      (dq2 > zero) CONSTRUCT_INTERSECTION(p1,r1,q1,r2,p2,q2) \
 else if (dr2 > zero) CONSTRUCT_INTERSECTION(p1,r1,q1,q2,r2,p2) \
 else                 CONSTRUCT_INTERSECTION(p1,q1,r1,p2,q2,r2) \
 } else if (dp2 < zero) { \
 if      (dq2 < zero) CONSTRUCT_INTERSECTION(p1,q1,r1,r2,p2,q2) \
 else if (dr2 < zero) CONSTRUCT_INTERSECTION(p1,q1,r1,q2,r2,p2) \
 else                 CONSTRUCT_INTERSECTION(p1,r1,q1,p2,q2,r2) \
 } else { \
 if (dq2 < zero) { \
 if (dr2 >= zero) CONSTRUCT_INTERSECTION(p1,r1,q1,q2,r2,p2) \
 else             CONSTRUCT_INTERSECTION(p1,q1,r1,p2,q2,r2) \
 } else if (dq2 > zero) { \
 if (dr2 > zero) CONSTRUCT_INTERSECTION(p1,r1,q1,p2,q2,r2) \
 else            CONSTRUCT_INTERSECTION(p1,q1,r1,q2,r2,p2) \
 } else  { \
 if      (dr2 > zero) CONSTRUCT_INTERSECTION(p1,q1,r1,r2,p2,q2) \
 else if (dr2 < zero) CONSTRUCT_INTERSECTION(p1,r1,q1,r2,p2,q2) \
 else { \
 *coplanar = 1; \
 return coplanar_tri_tri3d(p1,q1,r1,p2,q2,r2,N1,N2);\
 } \
 } \
 } \
 }


/*
   The following version computes the segment of intersection of the
   two triangles if it exists.
   coplanar returns whether the triangles are coplanar
   source and target are the endpoints of the line segment of intersection
*/

int tri_tri_intersection_test_3d(const real p1[3], const real q1[3], const real r1[3],
                                 const real p2[3], const real q2[3], const real r2[3],
                                 int *coplanar,
                                 real source[3], real target[3])
{
    real dp1, dq1, dr1, dp2, dq2, dr2;
    real v1[3], v2[3], v[3];
    real N1[3], N2[3], N[3];
    real alpha;

    // Compute distance signs  of p1, q1 and r1
    // to the plane of triangle(p2,q2,r2)
    SUB(v1, p2, r2)
    SUB(v2, q2, r2)
    CROSS(N2, v1, v2)

    SUB(v1, p1, r2)
    dp1 = DOT(v1, N2);
    SUB(v1, q1, r2)
    dq1 = DOT(v1, N2);
    SUB(v1, r1, r2)
    dr1 = DOT(v1, N2);

    if(((dp1 * dq1) > zero) && ((dp1 * dr1) > zero)){ return 0; }

    // Compute distance signs  of p2, q2 and r2
    // to the plane of triangle(p1,q1,r1)
    SUB(v1, q1, p1)
    SUB(v2, r1, p1)
    CROSS(N1, v1, v2)

    SUB(v1, p2, r1)
    dp2 = DOT(v1, N1);
    SUB(v1, q2, r1)
    dq2 = DOT(v1, N1);
    SUB(v1, r2, r1)
    dr2 = DOT(v1, N1);

    if(((dp2 * dq2) > zero) && ((dp2 * dr2) > zero)){ return 0; }

    // Permutation in a canonical form of T1's vertices
    if(ZERO_TEST(dp1)){ dp1 = zero; }
    if(ZERO_TEST(dq1)){ dq1 = zero; }
    if(ZERO_TEST(dr1)){ dr1 = zero; }

    if(ZERO_TEST(dp2)){ dp2 = zero; }
    if(ZERO_TEST(dq2)){ dq2 = zero; }
    if(ZERO_TEST(dr2)){ dr2 = zero; }

    if(dp1 > zero)
    {
        if(dq1 > zero)TRI_TRI_INTER_3D(r1, p1, q1, p2, r2, q2, dp2, dr2, dq2)
        else if(dr1 > zero)TRI_TRI_INTER_3D(q1, r1, p1, p2, r2, q2, dp2, dr2, dq2)
        else TRI_TRI_INTER_3D(p1, q1, r1, p2, q2, r2, dp2, dq2, dr2)
    }
    else if(dp1 < zero)
    {
        if(dq1 < zero)TRI_TRI_INTER_3D(r1, p1, q1, p2, q2, r2, dp2, dq2, dr2)
        else if(dr1 < zero)TRI_TRI_INTER_3D(q1, r1, p1, p2, q2, r2, dp2, dq2, dr2)
        else TRI_TRI_INTER_3D(p1, q1, r1, p2, r2, q2, dp2, dr2, dq2)
    }
    else
    {
        if(dq1 < zero)
        {
            if(dr1 >= zero)TRI_TRI_INTER_3D(q1, r1, p1, p2, r2, q2, dp2, dr2, dq2)
            else TRI_TRI_INTER_3D(p1, q1, r1, p2, q2, r2, dp2, dq2, dr2)
        }
        else if(dq1 > zero)
        {
            if(dr1 > zero)TRI_TRI_INTER_3D(p1, q1, r1, p2, r2, q2, dp2, dr2, dq2)
            else TRI_TRI_INTER_3D(q1, r1, p1, p2, q2, r2, dp2, dq2, dr2)
        }
        else
        {
            if(dr1 > zero)TRI_TRI_INTER_3D(r1, p1, q1, p2, q2, r2, dp2, dq2, dr2)
            else if(dr1 < zero)TRI_TRI_INTER_3D(r1, p1, q1, p2, r2, q2, dp2, dr2, dq2)
            else
            {
                *coplanar = 1;
                return coplanar_tri_tri3d(p1, q1, r1, p2, q2, r2, N1, N2);
            }
        }
    }
};



/*
*
*  Two dimensional Triangle-Triangle Overlap Test
*
*/

// -----------------------------------------------------------------------------
#define ORIENT_2D(a, b, c)  ((a[0]-c[0])*(b[1]-c[1])-(a[1]-c[1])*(b[0]-c[0]))

// -----------------------------------------------------------------------------
#define INTERSECTION_TEST_VERTEX(P1, Q1, R1, P2, Q2, R2) \
 { \
 if (ORIENT_2D(R2,P2,Q1) >= zero) { \
 if (ORIENT_2D(R2,Q2,Q1) <= zero) { \
 if (ORIENT_2D(P1,P2,Q1) > zero) { \
 if (ORIENT_2D(P1,Q2,Q1) <= zero) return 1; \
 } else { \
 if (ORIENT_2D(P1,P2,R1) >= zero) { \
 if (ORIENT_2D(Q1,R1,P2) >= zero) return 1; \
 } \
 } \
 } else { \
 if (ORIENT_2D(P1,Q2,Q1) <= zero) { \
 if (ORIENT_2D(R2,Q2,R1) <= zero) { \
 if (ORIENT_2D(Q1,R1,Q2) >= zero) return 1; \
 } \
 } \
 } \
 } else { \
 if (ORIENT_2D(R2,P2,R1) >= zero) { \
 if (ORIENT_2D(Q1,R1,R2) >= zero) { \
 if (ORIENT_2D(P1,P2,R1) >= zero) return 1;\
 } else { \
 if (ORIENT_2D(Q1,R1,Q2) >= zero) { \
 if (ORIENT_2D(R2,R1,Q2) >= zero) return 1; \
 } \
 } \
 } \
 } \
 return 0; \
 }

// -----------------------------------------------------------------------------
#define INTERSECTION_TEST_EDGE(P1, Q1, R1, P2, Q2, R2) \
 { \
 if (ORIENT_2D(R2,P2,Q1) >= zero) { \
 if (ORIENT_2D(P1,P2,Q1) >= zero) { \
 if (ORIENT_2D(P1,Q1,R2) >= zero) return 1; \
 } else { \
 if (ORIENT_2D(Q1,R1,P2) >= zero) { \
 if (ORIENT_2D(R1,P1,P2) >= zero) return 1; \
 } \
 } \
 } else { \
 if (ORIENT_2D(R2,P2,R1) >= zero) { \
 if (ORIENT_2D(P1,P2,R1) >= zero) { \
 if (ORIENT_2D(P1,R1,R2) >= zero) return 1; \
 if (ORIENT_2D(Q1,R1,R2) >= zero) return 1; \
 } \
 } \
 } \
 return 0; \
 }

// -----------------------------------------------------------------------------
int ccw_tri_tri_intersection_2d(const real p1[2], const real q1[2], const real r1[2],
                                const real p2[2], const real q2[2], const real r2[2])
{
    if(ORIENT_2D(p2, q2, p1) >= zero)
    {
        if(ORIENT_2D(q2, r2, p1) >= zero)
        {
            if(ORIENT_2D(r2, p2, p1) >= zero){ return 1; }
            INTERSECTION_TEST_EDGE(p1, q1, r1, p2, q2, r2)
        }
        else
        {
            if(ORIENT_2D(r2, p2, p1) >= zero)INTERSECTION_TEST_EDGE(p1, q1, r1, r2, p2, q2)
            else INTERSECTION_TEST_VERTEX(p1, q1, r1, p2, q2, r2)
        }
    }
    else
    {
        if(ORIENT_2D(q2, r2, p1) >= zero)
        {
            if(ORIENT_2D(r2, p2, p1) >= zero)INTERSECTION_TEST_EDGE(p1, q1, r1, q2, r2, p2)
            else INTERSECTION_TEST_VERTEX(p1, q1, r1, q2, r2, p2)
        }
        else
        {
            INTERSECTION_TEST_VERTEX(p1, q1, r1, r2, p2, q2)
        }
    }
};

// -----------------------------------------------------------------------------
int tri_tri_overlap_test_2d(const real p1[2], const real q1[2], const real r1[2],
                            const real p2[2], const real q2[2], const real r2[2])
{
    if(ORIENT_2D(p1, q1, r1) < zero)
    {
        if(ORIENT_2D(p2, q2, r2) < zero)
        {
            return ccw_tri_tri_intersection_2d(p1, r1, q1, p2, r2, q2);
        }
        else
        {
            return ccw_tri_tri_intersection_2d(p1, r1, q1, p2, q2, r2);
        }
    }
    else
    {
        if(ORIENT_2D(p2, q2, r2) < zero)
        {
            return ccw_tri_tri_intersection_2d(p1, q1, r1, p2, r2, q2);
        }
        else
        {
            return ccw_tri_tri_intersection_2d(p1, q1, r1, p2, q2, r2);
        }
    }
};

///////////////////////////////////////////////////////////////////////////////

/********************************************************/

/* AABB-triangle overlap test code                      */

/* by Tomas Akenine-Möller                              */

/* Function: int tri_box_overlap(float boxcenter[3],      */

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

#define FINDMINMAX(x0, x1, x2, min, max) \
min = max = x0;   \
if(x1<min) min=x1;\
if(x1>max) max=x1;\
if(x2<min) min=x2;\
if(x2>max) max=x2;

int planeBoxOverlap(const float normal[3], const float vert[3], const float maxbox[3])    // -NJMP-
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
        }
        else
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

int tri_box_overlap(const float boxcenter[3], const float boxhalfsize[3], const float triverts[3][3])
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

    if(min > boxhalfsize[X] || max < -boxhalfsize[X]) return 0;

    /* test in Y-direction */
    FINDMINMAX(v0[Y], v1[Y], v2[Y], min, max);

    if(min > boxhalfsize[Y] || max < -boxhalfsize[Y]) return 0;

    /* test in Z-direction */

    FINDMINMAX(v0[Z], v1[Z], v2[Z], min, max);

    if(min > boxhalfsize[Z] || max < -boxhalfsize[Z]) return 0;

    /* Bullet 2: */
    /*  test if the box intersects the plane of the triangle */
    /*  compute plane equation of triangle: normal*x+d=0 */
    CROSS(normal, e0, e1);
    // -NJMP- (line removed here)
    if(!planeBoxOverlap(normal, v0, boxhalfsize)) return 0;    // -NJMP-

    return 1;   /* box and triangle overlaps */
}