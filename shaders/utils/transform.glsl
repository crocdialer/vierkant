#ifndef UTILS_TRANSFORM_GLSL
#define UTILS_TRANSFORM_GLSL

struct transform_t
{
    float translation_x, translation_y, translation_z;
    float rotation_w, rotation_x, rotation_y, rotation_z;
    float scale_x, scale_y, scale_z;
};

vec3 rotate_quat(const vec4 q, const vec3 v)
{
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

vec3 apply_transform(const transform_t t, const vec3 v)
{
    vec4 rot = vec4(t.rotation_x, t.rotation_y, t.rotation_z, t.rotation_w);
    vec3 scale = vec3(t.scale_x, t.scale_y, t.scale_z);
    return rotate_quat(rot, v * scale) + vec3(t.translation_x, t.translation_y, t.translation_z);
}

vec3 apply_rotation(const transform_t t, const vec3 v)
{
    return rotate_quat(vec4(t.rotation_x, t.rotation_y, t.rotation_z, t.rotation_w), v);
}

//! mutliply (chain) two quaternions
vec4 quat_mult(vec4 lhs, vec4 rhs)
{
    vec4 ret;
    ret.w = lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z;
    ret.x = lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y;
    ret.y = lhs.w * rhs.y + lhs.y * rhs.w + lhs.z * rhs.x - lhs.x * rhs.z;
    ret.z = lhs.w * rhs.z + lhs.z * rhs.w + lhs.x * rhs.y - lhs.y * rhs.x;
    return ret;
}

//! mutliply (chain) two transforms
transform_t transform_mult(const transform_t lhs, const transform_t rhs)
{
    vec4 rot = vec4(lhs.rotation_x, lhs.rotation_y, lhs.rotation_z, lhs.rotation_w);
    vec3 scale = vec3(lhs.scale_x, lhs.scale_y, lhs.scale_z);
    vec3 trans = vec3(lhs.translation_x, lhs.translation_y, lhs.translation_z);

    trans += rotate_quat(rot, vec3(rhs.translation_x, rhs.translation_y, rhs.translation_z) * scale);
    rot = quat_mult(rot, vec4(rhs.rotation_x, rhs.rotation_y, rhs.rotation_z, rhs.rotation_w));
    scale *= vec3(rhs.scale_x, rhs.scale_y, rhs.scale_z);

    transform_t ret;
    ret.translation_x = trans.x; ret.translation_y = trans.y; ret.translation_z = trans.z;
    ret.rotation_w = rot.w; ret.rotation_x = rot.x; ret.rotation_y = rot.y; ret.rotation_z = rot.z;
    ret.scale_x = scale.x; ret.scale_y = scale.y; ret.scale_z = scale.z;
    return ret;
}

//! cast a quaternion to a mat3
mat3 mat3_cast(vec4 q)
{
    mat3 ret;
    float qxx = q.x * q.x;
    float qyy = q.y * q.y;
    float qzz = q.z * q.z;
    float qxz = q.x * q.z;
    float qxy = q.x * q.y;
    float qyz = q.y * q.z;
    float qwx = q.w * q.x;
    float qwy = q.w * q.y;
    float qwz = q.w * q.z;

    ret[0][0] = 1.0 - 2.0 * (qyy + qzz);
    ret[0][1] = 2.0 * (qxy + qwz);
    ret[0][2] = 2.0 * (qxz - qwy);

    ret[1][0] = 2.0 * (qxy - qwz);
    ret[1][1] = 1.0 - 2.0 * (qxx + qzz);
    ret[1][2] = 2.0 * (qyz + qwx);

    ret[2][0] = 2.0 * (qxz + qwy);
    ret[2][1] = 2.0 * (qyz - qwx);
    ret[2][2] = 1.0 - 2.0 * (qxx + qyy);
    return ret;
}

//! cast a transform_t to a mat4
mat4 mat4_cast(transform_t t)
{
    mat4 ret = mat4(mat3_cast(vec4(t.rotation_x, t.rotation_y, t.rotation_z, t.rotation_w)));
    ret[0] *= t.scale_x;
    ret[1] *= t.scale_y;
    ret[2] *= t.scale_z;
    ret[3] = vec4(t.translation_x, t.translation_y, t.translation_z, 1.0);
    return ret;
}

#endif // UTILS_TRANSFORM_GLSL