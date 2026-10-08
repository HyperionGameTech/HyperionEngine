#ifndef HYP_LIGHTMAP
#define HYP_LIGHTMAP

float2 GetLightmapAtlasUV(uint rectOffset, uint rectSize, float2 uv1)
{
    const float2 offsetTexels = float2(rectOffset & 0xFFFu, (rectOffset >> 12u) & 0xFFFu);
    const float2 scaleTexels = float2((rectSize & 0xFFFu) + 1u, ((rectSize >> 12u) & 0xFFFu) + 1u);
    const float2 atlasDimensions = float2(1u << ((rectSize >> 24u) & 0xFu), 1u << ((rectSize >> 28u) & 0xFu));

    return (offsetTexels + uv1 * scaleTexels) / atlasDimensions;
}

uint GetLightmapStencilValue(uint rectOffset)
{
    return rectOffset >> 24u;
}

#endif
