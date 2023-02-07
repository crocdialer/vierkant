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

vec3 apply_transform(const transform_t t, const vec3 p)
{
    vec4 rot = vec4(t.rotation_x, t.rotation_y, t.rotation_z, t.rotation_w);
    vec3 scale = vec3(t.scale_x, t.scale_y, t.scale_z);
    return rotate_quat(rot, p * scale) + vec3(t.translation_x, t.translation_y, t.translation_z);
}