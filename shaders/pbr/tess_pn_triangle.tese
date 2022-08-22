#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"
#include "../utils/camera.glsl"

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

layout (triangles, fractional_odd_spacing, cw) in;

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, set = 0, binding = BINDING_MESH_DRAWS) readonly buffer MeshDrawBuffer
{
    mesh_draw_t draws[];
};

layout(std140, binding = BINDING_JITTER_OFFSET) uniform UBOJitter
{
    camera_t camera;
    camera_t last_camera;
};

layout(location = LOCATION_INDEX_BUNDLE) flat in index_bundle_t indices_in[];
layout(location = LOCATION_VERTEX_BUNDLE) in VertexData vertex_in[];
layout(location = 10) in PnPatch in_patch[];

layout(location = LOCATION_INDEX_BUNDLE) flat out index_bundle_t indices_out;
layout(location = LOCATION_VERTEX_BUNDLE) out VertexData vertex_out;

VertexData interpolate_vertex(vec3 coord)
{
    VertexData ret;
    ret.tex_coord = coord.x * vertex_in[0].tex_coord + coord.y * vertex_in[1].tex_coord + coord.z * vertex_in[2].tex_coord;
    ret.normal = coord.x * vertex_in[0].normal + coord.y * vertex_in[1].normal + coord.z * vertex_in[2].normal;
    ret.tangent = coord.x * vertex_in[0].tangent + coord.y * vertex_in[1].tangent + coord.z * vertex_in[2].tangent;
    ret.current_position = coord.x * vertex_in[0].current_position + coord.y * vertex_in[1].current_position + coord.z * vertex_in[2].current_position;
    ret.last_position = coord.x * vertex_in[0].last_position + coord.y * vertex_in[1].last_position + coord.z * vertex_in[2].last_position;
    return ret;
}

#define uvw gl_TessCoord

VertexData interpolate_vertex_pn()
{
    VertexData ret = interpolate_vertex(gl_TessCoord.zxy);
    vec3 uvwSquared = uvw * uvw;
    vec3 uvwCubed   = uvwSquared * uvw;

    // extract control points
    vec3 b210 = vec3(in_patch[0].b210, in_patch[1].b210, in_patch[2].b210);
    vec3 b120 = vec3(in_patch[0].b120, in_patch[1].b120, in_patch[2].b120);
    vec3 b021 = vec3(in_patch[0].b021, in_patch[1].b021, in_patch[2].b021);
    vec3 b012 = vec3(in_patch[0].b012, in_patch[1].b012, in_patch[2].b012);
    vec3 b102 = vec3(in_patch[0].b102, in_patch[1].b102, in_patch[2].b102);
    vec3 b201 = vec3(in_patch[0].b201, in_patch[1].b201, in_patch[2].b201);
    vec3 b111 = vec3(in_patch[0].b111, in_patch[1].b111, in_patch[2].b111);

    // extract control normals
    vec3 n110 = normalize(vec3(in_patch[0].n110, in_patch[1].n110, in_patch[2].n110));
    vec3 n011 = normalize(vec3(in_patch[0].n011, in_patch[1].n011, in_patch[2].n011));
    vec3 n101 = normalize(vec3(in_patch[0].n101, in_patch[1].n101, in_patch[2].n101));

    // pn-normal
    ret.normal = vertex_in[0].normal * uvwSquared[2] + vertex_in[1].normal * uvwSquared[0] + vertex_in[2].normal * uvwSquared[1]
                 + n110 * uvw[2] * uvw[0] + n011 * uvw[0] * uvw[1] + n101 * uvw[2] * uvw[1];

    // save some computations
    uvwSquared *= 3.0;

    vec3 offset =  b210 * uvwSquared[2] * uvw[0] +
                   b120 * uvwSquared[0] * uvw[2] +
                   b201 * uvwSquared[2] * uvw[1] +
                   b021 * uvwSquared[0] * uvw[1] +
                   b102 * uvwSquared[1] * uvw[2] +
                   b012 * uvwSquared[1] * uvw[0] +
                   b111 * 6.0 * uvw[0] * uvw[1] * uvw[2];

    // compute PN position
    ret.current_position.xyz = vertex_in[0].current_position.xyz * uvwCubed[2] +
                               vertex_in[1].current_position.xyz * uvwCubed[0] +
                               vertex_in[2].current_position.xyz * uvwCubed[1] +
                               offset;

    // compute PN position
    ret.last_position.xyz = vertex_in[0].last_position.xyz * uvwCubed[2] +
                            vertex_in[1].last_position.xyz * uvwCubed[0] +
                            vertex_in[2].last_position.xyz * uvwCubed[1] +
                            offset;
    return ret;
}

void main(void)
{
//    vertex_out = interpolate_vertex(gl_TessCoord);
    vertex_out = interpolate_vertex_pn();

    // mvp transform and jittering
    uint object_index = indices_in[0].mesh_draw_index;
    matrix_struct_t m = draws[object_index].current_matrices;
    matrix_struct_t m_last = draws[object_index].last_matrices;

    vertex_out.current_position = camera.projection * camera.view * m.modelview * vertex_out.current_position;
    vertex_out.last_position = last_camera.projection * last_camera.view * m_last.modelview * vertex_out.last_position;

    vec4 jittered_position = vertex_out.current_position;
    jittered_position.xy += 2.0 * camera.sample_offset * jittered_position.w;
    gl_Position = jittered_position;

    indices_out = indices_in[0];
    vertex_out.tex_coord = (m.texture * vec4(vertex_out.tex_coord, 0, 1)).xy;
    vertex_out.normal = normalize(mat3(m.normal) * vertex_out.normal);
    vertex_out.tangent = normalize(mat3(m.normal) * vertex_out.tangent);
}