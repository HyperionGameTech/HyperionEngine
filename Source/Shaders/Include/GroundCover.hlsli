#ifndef HYP_GROUND_COVER_HLSLI
#define HYP_GROUND_COVER_HLSLI

float GroundCoverHash(int2 cell)
{
    uint state = (uint(cell.x) * 0x8DA6B343u) ^ (uint(cell.y) * 0xD8163841u);
    state = state * 747796405u + 2891336453u;
    state = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    state = (state >> 22u) ^ state;

    return float(state) * (1.0 / 4294967296.0);
}

float GroundCoverValueNoise(float2 p)
{
    const float2 cell = floor(p);
    const float2 f = p - cell;
    const float2 s = f * f * (3.0 - 2.0 * f);

    const int2 c = int2(cell);

    return lerp(
        lerp(GroundCoverHash(c), GroundCoverHash(c + int2(1, 0)), s.x),
        lerp(GroundCoverHash(c + int2(0, 1)), GroundCoverHash(c + int2(1, 1)), s.x),
        s.y);
}

// albedo multiplier for an instance standing at origin: drifts of lush and dry growth, each clump a little brighter or darker than the next
float3 GroundCoverTint(float3 origin, float variation)
{
    const float drift = lerp(GroundCoverValueNoise(origin.xz * (1.0 / 9.0)), GroundCoverValueNoise(origin.xz * (1.0 / 2.5) + 31.7), 0.3);
    const float dryness = smoothstep(0.4, 0.85, drift);
    const float brightness = lerp(0.8, 1.12, GroundCoverHash(int2(floor(origin.xz * 16.0))));

    const float3 tint = lerp(float3(0.9, 1.0, 0.92), float3(1.3, 1.1, 0.55), dryness) * brightness;

    return lerp((float3)1.0, tint, variation);
}

float GroundCoverBaseOcclusion(float heightAboveOrigin, float occlusion, float occlusionHeight)
{
    if (occlusion <= 0.0 || occlusionHeight <= 0.0)
    {
        return 1.0;
    }

    return lerp(1.0 - occlusion, 1.0, smoothstep(0.0, 1.0, saturate(heightAboveOrigin / occlusionHeight)));
}

#endif // HYP_GROUND_COVER_HLSLI
