#extension GL_EXT_shader_explicit_arithmetic_types_int8: require

//! nv-specific warp-value (use 64 for AMD)
#define LOCAL_SIZE 32

struct mesh_task_payload_t
{
    uint meshlet_base_index;
    uint8_t meshlet_delta_indices[LOCAL_SIZE];
    uint object_index;
};