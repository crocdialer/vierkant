module procedural_environment;

namespace utils
{

// low-life + neutral environment-light
public float3 environment_white(float3 direction)
{
    float3 col_sky = float3(1.3), col_horizon = float3(0.6), col_ground = float3(0.1);
    return direction.y > 0 ? lerp(col_horizon, col_sky, direction.y) :
                             lerp(col_horizon, col_ground, -direction.y);
}

public struct procedural_sky_params_t
{
    public float3 direction_to_light;
    public float angular_size_of_light;

    public float3 light_color;
    public float glow_size;

    public float3 sky_color;
    public float glow_intensity;

    public float3 horizon_color;
    public float horizon_size;

    public float3 ground_color;
    public float glow_sharpness;

    public float3 direction_up;
    public int pad;
};

public float3 procedural_sky(procedural_sky_params_t params, float3 direction, float angular_px_size)
{
    float elevation = asin(clamp(dot(direction, params.direction_up), -1.0, 1.0));
    float top    = smoothstep(0.0, params.horizon_size,  elevation);
    float bottom = smoothstep(0.0, params.horizon_size, -elevation);
    float3 environment = lerp(lerp(params.horizon_color, params.ground_color, bottom), params.sky_color, top);

    float angle_to_light = acos(clamp(dot(direction, params.direction_to_light), 0.0, 1.0));
    float half_angular_size = params.angular_size_of_light * 0.5;
    float light_intensity = clamp(1.0 - smoothstep(half_angular_size - angular_px_size * 2,
                                                   half_angular_size + angular_px_size * 2, angle_to_light), 0.0, 1.0);
    light_intensity = pow(light_intensity, 4.0);
    float glow_input = clamp(2.0 * (1.0 - smoothstep(half_angular_size - params.glow_size,
                                                     half_angular_size + params.glow_size, angle_to_light)), 0.0, 1.0);
    float glow_intensity = params.glow_intensity * pow(glow_input, params.glow_sharpness);
    float3 light = max(light_intensity, glow_intensity) * params.light_color;
    return environment + light;
}

} // namespace utils
