#ifndef RENDERER_MESH_TASK_PAYLOAD_GLSL
#define RENDERER_MESH_TASK_PAYLOAD_GLSL

#extension GL_EXT_shader_explicit_arithmetic_types_int8: require

// TODO: reasonable default setting for nvidia only!?
#define MESHLET_MAX_VERTICES 64
#define MESHLET_MAX_TRIANGLES 84

// NOTE: mesh/task workgroup sizes are vendor-specific (NV -> 32/32, AMD -> 64/128)
// we reserve a maximum value for task-payloads to avoid shader-permutations per vendor
#define MAX_MESH_WORKGROUP_SIZE 128

struct mesh_task_payload_t
{
    uint meshlet_base_index;
    uint8_t meshlet_delta_indices[MAX_MESH_WORKGROUP_SIZE];
    uint object_index;
};

#endif // RENDERER_MESH_TASK_PAYLOAD_GLSL