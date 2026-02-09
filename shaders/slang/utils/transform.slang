module transform;

namespace utils
{
    
public struct transform_t
{
    public float translation_x, translation_y, translation_z;
    public float rotation_x, rotation_y, rotation_z, rotation_w;
    public float scale_x, scale_y, scale_z;
};

public float3 rotate_quat(float4 q, float3 v)
{
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

public float3 apply_transform(transform_t t, float3 v)
{
    float4 rot = float4(t.rotation_x, t.rotation_y, t.rotation_z, t.rotation_w);
    float3 scale = float3(t.scale_x, t.scale_y, t.scale_z);
    return rotate_quat(rot, v * scale) + float3(t.translation_x, t.translation_y, t.translation_z);
}

public float3 apply_rotation(transform_t t, float3 v)
{
    float4 rot = float4(t.rotation_x, t.rotation_y, t.rotation_z, t.rotation_w);
    return rotate_quat(rot, v);
}

public float4 quat_mult(float4 lhs, float4 rhs)
{
    return float4(
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y + lhs.y * rhs.w + lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.w * rhs.z + lhs.z * rhs.w + lhs.x * rhs.y - lhs.y * rhs.x,
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z
    );
}

public transform_t transform_mult(transform_t lhs, transform_t rhs)
{
    float4 rot_l = float4(lhs.rotation_x, lhs.rotation_y, lhs.rotation_z, lhs.rotation_w);
    float3 scale_l = float3(lhs.scale_x, lhs.scale_y, lhs.scale_z);
    float3 trans_l = float3(lhs.translation_x, lhs.translation_y, lhs.translation_z);

    float3 trans = trans_l + rotate_quat(rot_l, float3(rhs.translation_x, rhs.translation_y, rhs.translation_z) * scale_l);
    float4 rot = quat_mult(rot_l, float4(rhs.rotation_x, rhs.rotation_y, rhs.rotation_z, rhs.rotation_w));
    float3 scale = scale_l * float3(rhs.scale_x, rhs.scale_y, rhs.scale_z);

    transform_t result;
    result.translation_x = trans.x;
    result.translation_y = trans.y;
    result.translation_z = trans.z;
    result.rotation_x = rot.x;
    result.rotation_y = rot.y;
    result.rotation_z = rot.z;
    result.rotation_w = rot.w;
    result.scale_x = scale.x;
    result.scale_y = scale.y;
    result.scale_z = scale.z;
    return result;
}

public float3x3 mat3_cast(float4 q)
{
    float qxx = q.x * q.x;
    float qyy = q.y * q.y;
    float qzz = q.z * q.z;
    float qxz = q.x * q.z;
    float qxy = q.x * q.y;
    float qyz = q.y * q.z;
    float qwx = q.w * q.x;
    float qwy = q.w * q.y;
    float qwz = q.w * q.z;

    return float3x3(
        float3(1.0 - 2.0 * (qyy + qzz), 2.0 * (qxy + qwz), 2.0 * (qxz - qwy)),
        float3(2.0 * (qxy - qwz), 1.0 - 2.0 * (qxx + qzz), 2.0 * (qyz + qwx)),
        float3(2.0 * (qxz + qwy), 2.0 * (qyz - qwx), 1.0 - 2.0 * (qxx + qyy))
    );
}

public float4x4 mat4_cast(transform_t t)
{
    float3x3 rot = mat3_cast(float4(t.rotation_x, t.rotation_y, t.rotation_z, t.rotation_w));
    return float4x4(
        float4(rot[0] * t.scale_x, 0.0),
        float4(rot[1] * t.scale_y, 0.0),
        float4(rot[2] * t.scale_z, 0.0),
        float4(t.translation_x, t.translation_y, t.translation_z, 1.0)
    );
}
}