import "../renderer/types.slang";
import "../utils/dof.slang";

enum TextureIndex { COLOR = 0, DEPTH = 1 };

[[vk::binding(0, 0)]] Sampler2D u_textures[2];
[[vk::binding(1, 0)]] ConstantBuffer<utils::dof_params_t> u_dof_params;

[[vk::push_constant]]
renderer::context_t context;

struct FragOutput
{
    float4 color : SV_Target;
    float depth : SV_Depth;
};

[[shader("pixel")]]
FragOutput fragment_main(float2 tex_coord : TexCoord0, float4 sv_position : SV_Position)
{
    FragOutput o;
    o.depth = u_textures[TextureIndex::DEPTH].Sample(tex_coord).x;
    o.color = utils::depth_of_field(u_textures[TextureIndex::COLOR], u_textures[TextureIndex::DEPTH],
                                    tex_coord, sv_position.xy, u_dof_params);
    return o;
}
