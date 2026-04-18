module colormap;

namespace utils
{

public float3 jet(float val)
{
    return clamp(float3(min(4.0 * val - 1.5, -4.0 * val + 4.5),
                        min(4.0 * val - 0.5, -4.0 * val + 3.5),
                        min(4.0 * val + 0.5, -4.0 * val + 2.5)), 0.0, 1.0);
}

}// namespace utils
