import "utils/integer_traits.slang";
import "utils/hashmap.slang";

struct object_overlay_params_t
{
    utils::hashmap_t<uint, uint> object_id_map;
    bool silhouette;
};

[[vk::binding(0, 0)]]
RWTexture2D<uint> in_object_id_img;

[[vk::binding(1, 0)]]
RWTexture2D<float4> out_mask_img;

[[vk::binding(2, 0)]]
ConstantBuffer<object_overlay_params_t> params;

float sobel_edge(utils::hashmap_t<uint, uint> object_id_map, int2 coord)
{
    // sobel
    float nw = float(object_id_map.contains(in_object_id_img[coord + int2(-1, -1)]));
    float n  = float(object_id_map.contains(in_object_id_img[coord + int2(0, -1)]));
    float ne = float(object_id_map.contains(in_object_id_img[coord + int2(1, -1)]));
    float w  = float(object_id_map.contains(in_object_id_img[coord + int2(-1, 0)]));
    float c  = float(object_id_map.contains(in_object_id_img[coord + int2(0, 0)]));
    float e  = float(object_id_map.contains(in_object_id_img[coord + int2(1, 0)]));
    float sw = float(object_id_map.contains(in_object_id_img[coord + int2(-1, 1)]));
    float s  = float(object_id_map.contains(in_object_id_img[coord + int2(0, 1)]));
    float se = float(object_id_map.contains(in_object_id_img[coord + int2(1, 1)]));
    float sobel_edge_h = ne + (2.0 * e) + se - (nw + (2.0 * w) + sw);
    float sobel_edge_v = nw + (2.0 * n) + ne - (sw + (2.0 * s) + se);
    return sqrt((sobel_edge_h * sobel_edge_h) + (sobel_edge_v * sobel_edge_v));
}

[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    int2 out_coords = int2(dispatchThreadID.xy);

    uint in_width, in_height, out_width, out_height;
    in_object_id_img.GetDimensions(in_width, in_height);
    out_mask_img.GetDimensions(out_width, out_height);

    int2 in_coords = int2((in_width * out_coords.x) / out_width, (in_height * out_coords.y) / out_height);

    float result = 0.0;

    if(params.silhouette) { result = sobel_edge(params.object_id_map, in_coords); }
    else
    {
        // generate binary mask
        result = float(params.object_id_map.contains(in_object_id_img[in_coords]));
    }

    // store mask/silhouette value for selected objects
    out_mask_img[out_coords] = float4(result, 0.0, 0.0, 0.0);
}
