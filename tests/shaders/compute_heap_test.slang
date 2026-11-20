// test-shader for tests/TestComputeHeap.cpp, writes a push-constant to a storage-buffer.

struct params_t
{
    uint value;
};

[[vk::push_constant]] params_t params;

[[vk::binding(0, 0)]] RWStructuredBuffer<uint> out_value;

[shader("compute")]
[numthreads(1, 1, 1)]
void main() { out_value[0] = params.value; }
