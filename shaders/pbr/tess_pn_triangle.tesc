#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"
#include "../utils/camera.glsl"

layout (vertices = 3) out;

// http://onrendering.blogspot.com/2011/12/tessellation-on-gpu-curved-pn-triangles.html
// PN patch data
struct PnPatch
{
    float b210;
    float b120;
    float b021;
    float b012;
    float b102;
    float b201;
    float b111;
    float n110;
    float n011;
    float n101;
};

struct VertexData
{
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
    vec4 current_position;
    vec4 last_position;
};
//layout(location = 0) flat in uint in_object_index[];
//layout(location = 1) in VertexData vertex_in[];
layout(location = LOCATION_INDEX_BUNDLE) flat in index_bundle_t indices_in[];
layout(location = LOCATION_VERTEX_BUNDLE) in VertexData vertex_in[];

//layout(location = 0) flat out uint out_object_index[3];
//layout(location = 1) out VertexData vertex_out[3];
layout(location = LOCATION_INDEX_BUNDLE) flat out index_bundle_t indices_out[3];
layout(location = LOCATION_VERTEX_BUNDLE) out VertexData vertex_out[3];
layout(location = 10) out PnPatch out_patch[3];

float wij(int i, int j)
{
    return dot(gl_in[j].gl_Position.xyz - gl_in[i].gl_Position.xyz, vertex_in[i].normal);
}

float vij(int i, int j)
{
    vec3 Pj_minus_Pi = gl_in[j].gl_Position.xyz - gl_in[i].gl_Position.xyz;
    vec3 Ni_plus_Nj  = vertex_in[i].normal + vertex_in[j].normal;
    return 2.0 * dot(Pj_minus_Pi, Ni_plus_Nj) / dot(Pj_minus_Pi, Pj_minus_Pi);
}

void main(void)
{
    float tess_lvl = 4;

    // set tess levels
    gl_TessLevelOuter[gl_InvocationID] = tess_lvl;
    gl_TessLevelInner[0] = tess_lvl;

    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    indices_out[gl_InvocationID] = indices_in[gl_InvocationID];
    vertex_out[gl_InvocationID] = vertex_in[gl_InvocationID];

    // set base
    float P0 = gl_in[0].gl_Position[gl_InvocationID];
    float P1 = gl_in[1].gl_Position[gl_InvocationID];
    float P2 = gl_in[2].gl_Position[gl_InvocationID];
    float N0 = vertex_in[0].normal[gl_InvocationID];
    float N1 = vertex_in[1].normal[gl_InvocationID];
    float N2 = vertex_in[2].normal[gl_InvocationID];

    // compute control points
    out_patch[gl_InvocationID].b210 = (2.0 * P0 + P1 - wij(0, 1) * N0) / 3.0;
    out_patch[gl_InvocationID].b120 = (2.0 * P1 + P0 - wij(1, 0) * N1) / 3.0;
    out_patch[gl_InvocationID].b021 = (2.0 * P1 + P2 - wij(1, 2) * N1) / 3.0;
    out_patch[gl_InvocationID].b012 = (2.0 * P2 + P1 - wij(2, 1) * N2) / 3.0;
    out_patch[gl_InvocationID].b102 = (2.0 * P2 + P0 - wij(2, 0) * N2) / 3.0;
    out_patch[gl_InvocationID].b201 = (2.0 * P0 + P2 - wij(0, 2) * N0) / 3.0;

    float E = ( out_patch[gl_InvocationID].b210
                + out_patch[gl_InvocationID].b120
                + out_patch[gl_InvocationID].b021
                + out_patch[gl_InvocationID].b012
                + out_patch[gl_InvocationID].b102
                + out_patch[gl_InvocationID].b201 ) / 6.0;

    float V = (P0 + P1 + P2) / 3.0;
    out_patch[gl_InvocationID].b111 = E + (E - V) * 0.5;
    out_patch[gl_InvocationID].n110 = N0 + N1 - vij(0, 1) * (P1 - P0);
    out_patch[gl_InvocationID].n011 = N1 + N2 - vij(1, 2) * (P2 - P1);
    out_patch[gl_InvocationID].n101 = N2 + N0 - vij(2, 0) * (P0 - P2);
}