#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2: require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types: require

#include "../renderer/types.glsl"
#include "../utils/packed_vertex.glsl"
#include "../utils/camera.glsl"

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(buffer_reference, scalar) readonly buffer VertexBufferPtr { packed_vertex_t v[]; };
layout(binding = BINDING_VERTICES, set = 0) readonly buffer Vertices { VertexBufferPtr vertex_buffers[]; };

layout(set = 0, binding = BINDING_DRAW_COMMANDS) readonly buffer DrawBuffer
{
    indexed_indirect_command_t draw_commands[];
};

layout(set = 0, binding = BINDING_BONE_VERTEX_DATA) readonly buffer BoneVertexBuffer
{
    bone_vertex_data_t bone_vertex_data[];
};

layout(std140, set = 0, binding = BINDING_MESH_DRAWS) readonly buffer MeshDrawBuffer
{
    mesh_draw_t draws[];
};

layout(set = 0, binding = BINDING_BONES, scalar) readonly buffer UBOBones
{
    transform_t u_bones[];
};

layout(set = 0, binding = BINDING_PREVIOUS_BONES, scalar) readonly buffer UBOPreviousBones
{
    transform_t u_previous_bones[];
};

layout(std140, binding = BINDING_JITTER_OFFSET) uniform UBOJitter
{
    camera_t camera;
    camera_t last_camera;
};

layout(location = LOCATION_INDEX_BUNDLE) flat out index_bundle_t indices;
layout(location = LOCATION_VERTEX_BUNDLE) out VertexData
{
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
    vec4 current_position;
    vec4 last_position;
} vertex_out;

void main()
{
    indices.mesh_draw_index = gl_BaseInstance;
    indices.material_index = draws[gl_BaseInstance].material_index;

    // retrieve vertex-buffer, unpack vertex
    Vertex v = unpack(vertex_buffers[draws[indices.mesh_draw_index].mesh_index].v[gl_VertexIndex]);

    vec4 bone_weights = vec4(float(bone_vertex_data[gl_VertexIndex].weight_x),
                             float(bone_vertex_data[gl_VertexIndex].weight_y),
                             float(bone_vertex_data[gl_VertexIndex].weight_z),
                             float(bone_vertex_data[gl_VertexIndex].weight_w));
    uvec4 bone_ids = uvec4(uint(bone_vertex_data[gl_VertexIndex].index_x),
                           uint(bone_vertex_data[gl_VertexIndex].index_y),
                           uint(bone_vertex_data[gl_VertexIndex].index_z),
                           uint(bone_vertex_data[gl_VertexIndex].index_w));

    matrix_struct_t m = draws[indices.mesh_draw_index].current_matrices;
    matrix_struct_t m_last = draws[indices.mesh_draw_index].last_matrices;

    vec3 current_vertex = vec3(0);
    vec3 current_normal = vec3(0);
    vec3 current_tangent = vec3(0);
    vec3 last_vertex = vec3(0);

    for (int i = 0; i < 4; i++)
    {
        current_vertex += apply_transform(u_bones[bone_ids[i]], v.position) * bone_weights[i];
        current_normal += apply_rotation(u_bones[bone_ids[i]], v.normal) * bone_weights[i];
        current_tangent += apply_rotation(u_bones[bone_ids[i]], v.tangent) * bone_weights[i];
        last_vertex += apply_transform(u_previous_bones[bone_ids[i]], v.position) * bone_weights[i];
    }

    vertex_out.tex_coord = (m.texture * vec4(v.tex_coord, 0, 1)).xy;
    vertex_out.normal = normalize(apply_rotation(m.transform, current_normal));
    vertex_out.tangent = normalize(apply_rotation(m.transform, current_tangent));

    vertex_out.current_position = camera.projection * camera.view * vec4(apply_transform(m.transform, current_vertex), 1.0);
    vertex_out.last_position = last_camera.projection * last_camera.view * vec4(apply_transform(m_last.transform, last_vertex), 1.0);

    vec4 jittered_position = vertex_out.current_position;
    jittered_position.xy += 2.0 * camera.sample_offset * jittered_position.w;
    gl_Position = jittered_position;
}