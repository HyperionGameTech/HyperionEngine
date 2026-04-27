#include "include/Defines.hlsli"
#include "include/Shared.hlsli"

DECLARE_SRV(BlitDescriptorSet, InputTexture) Texture2D<float4> src_image;
DECLARE_UAV(BlitDescriptorSet, OutputTexture) RWTexture2D<float4> dst_image;
DECLARE_SAMPLER(BlitDescriptorSet, SamplerLinear) SamplerState SamplerLinear;
DECLARE_BUFFER_DYNAMIC(BlitDescriptorSet, BlitConstants) cbuffer BlitConstants
{
    uint2 src_rect_min;
    uint2 src_rect_max;
    uint2 dst_rect_min;
    uint2 dst_rect_max;
    uint2 src_dimensions;
    uint  src_mip_level;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint2 dst_coord = dst_rect_min + dispatchThreadID.xy;

    if (any(dst_coord >= dst_rect_max))
        return;

    const uint2 src_rect_size = src_rect_max - src_rect_min;
    const uint2 dst_rect_size = dst_rect_max - dst_rect_min;

    /* Map the current destination pixel to its corresponding
       source position using proportional rect mapping. */
    const float2 uv = float2(
        src_rect_min.x + (float(dst_coord.x - dst_rect_min.x) + 0.5) * float(src_rect_size.x) / float(dst_rect_size.x),
        src_rect_min.y + (float(dst_coord.y - dst_rect_min.y) + 0.5) * float(src_rect_size.y) / float(dst_rect_size.y)
    ) / float2(src_dimensions);

    const float4 color = src_image.SampleLevel(SamplerLinear, uv, src_mip_level);

    dst_image[dst_coord] = color;
}
