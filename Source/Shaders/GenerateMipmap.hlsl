#include "include/Defines.hlsli"
#include "include/Shared.hlsli"

DECLARE_SAMPLER(GenerateMipmaps, SamplerLinear) SamplerState SamplerLinear;

DECLARE_SRV(GenerateMipmaps, InputTexture) Texture2D<float4> mip_input;
DECLARE_UAV(GenerateMipmaps, OutputTexture) RWTexture2D<float4> mip_output;

DECLARE_BUFFER_DYNAMIC(GenerateMipmaps, Constants) cbuffer Constants
{
    uint2 src_dimensions;
    uint2 dst_dimensions;
    uint src_mip_level;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint2 dst_coord = dispatchThreadID.xy;

    if (any(dst_coord >= dst_dimensions))
        return;

    const float2 uv = (float2(dst_coord) + 0.5) / float2(dst_dimensions);

    const float4 result = mip_input.SampleLevel(SamplerLinear, uv, 0.0);

    mip_output[dst_coord] = result;
}
