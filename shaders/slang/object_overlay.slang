import "utils/hash.slang";

static const float PI = 3.1415926535897932384626433832795;
static const float ONE_OVER_PI = 0.31830988618379067153776752674503;
static const float ONE_OVER_FOUR_PI = 0.07957747154594767;

static const uint MAX_UINT16 = (1 << 16) - 1;
static const uint MAX_UINT32 = (1 << 32) - 1;

struct hashmap_item_t
{
    uint key;
    uint value;
};

struct hashmap_t
{
    hashmap_item_t* storage;
    uint size;
    uint capacity;
};

uint hash(uint v)
{
    return utils::xxhash32(v, 0);
}

uint get(hashmap_t hashmap, uint key)
{
    if (key == 0 || hashmap.capacity == 0) { return 0; }

    for (uint idx = hash(key); ; idx++)
    {
        idx &= hashmap.capacity - 1;
        hashmap_item_t item = hashmap.storage[idx];
        if (item.key == 0) { return 0; }
        else if (key == item.key && item.value != 0) { return item.value; }
    }
}

struct object_overlay_params_t
{
    hashmap_t object_id_map;
    bool silhouette;
};


[[vk::binding(0, 0)]] RWTexture2D<uint> in_object_id_img;
[[vk::binding(1, 0)]] RWTexture2D<float4> out_mask_img;
[[vk::binding(2, 0)]] ConstantBuffer<object_overlay_params_t> params;

bool contains(hashmap_t object_id_map, uint object_id)
{
    return get(object_id_map, object_id) != 0;
}

float sobel_edge(hashmap_t object_id_map, int2 coord)
{
    // sobel
    float n[9];
    n[0] = float(contains(object_id_map, in_object_id_img[coord + int2(-1, -1)]));
    n[1] = float(contains(object_id_map, in_object_id_img[coord + int2(0, -1)]));
    n[2] = float(contains(object_id_map, in_object_id_img[coord + int2(1, -1)]));
    n[3] = float(contains(object_id_map, in_object_id_img[coord + int2(-1, 0)]));
    n[4] = float(contains(object_id_map, in_object_id_img[coord + int2(0, 0)]));
    n[5] = float(contains(object_id_map, in_object_id_img[coord + int2(1, 0)]));
    n[6] = float(contains(object_id_map, in_object_id_img[coord + int2(-1, 1)]));
    n[7] = float(contains(object_id_map, in_object_id_img[coord + int2(0, 1)]));
    n[8] = float(contains(object_id_map, in_object_id_img[coord + int2(1, 1)]));
    float sobel_edge_h = n[2] + (2.0 * n[5]) + n[8] - (n[0] + (2.0 * n[3]) + n[6]);
    float sobel_edge_v = n[0] + (2.0 * n[1]) + n[2] - (n[6] + (2.0 * n[7]) + n[8]);
    return sqrt((sobel_edge_h * sobel_edge_h) + (sobel_edge_v * sobel_edge_v));
}

[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 out_coords = int2(dispatchThreadID.xy);
    
    uint in_width, in_height, out_width, out_height;
    in_object_id_img.GetDimensions(in_width, in_height);
    out_mask_img.GetDimensions(out_width, out_height);
    
    int2 in_coords = int2((in_width * out_coords.x) / out_width, (in_height * out_coords.y) / out_height);

    float result = 0.0;

    if (params.silhouette)
    {
        result = sobel_edge(params.object_id_map, in_coords);
    }
    else
    {
        // generate binary mask
        result = float(contains(params.object_id_map, in_object_id_img[in_coords]));
    }

    // store mask/silhouette value for selected objects
    out_mask_img[out_coords] = float4(result, 0.0, 0.0, 0.0);
}