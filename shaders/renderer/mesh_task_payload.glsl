#ifndef RENDERER_MESH_TASK_PAYLOAD_GLSL
#define RENDERER_MESH_TASK_PAYLOAD_GLSL

#extension GL_EXT_shader_explicit_arithmetic_types_int8: require

//! TODO: these mesh/task workgroup sizes are still vendor-specific (NV -> 32, AMD -> 64)
#define MESH_WORKGROUP_SIZE 32
#define TASK_WORKGROUP_SIZE 32

struct mesh_task_payload_t
{
    uint meshlet_base_index;
    uint8_t meshlet_delta_indices[MESH_WORKGROUP_SIZE];
    uint object_index;
};

#endif // RENDERER_MESH_TASK_PAYLOAD_GLSL