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

#define GROUND_COVER_FAR_RANK_CUTOFF 0.5
#define GROUND_COVER_RANK_SOFTNESS 0.3
#define GROUND_COVER_WIDEN_EXPONENT 0.35
#define GROUND_COVER_THIN_START 0.25
#define GROUND_COVER_THIN_END 0.9
#define GROUND_COVER_FADE_START 0.65
#define GROUND_COVER_GROW_SECONDS 0.6
// fractions of the near range across which clumps come to shade as one carpet
#define GROUND_COVER_FLATTEN_START 0.2
#define GROUND_COVER_FLATTEN_END 1.0
#define GROUND_COVER_FAR_NORMAL_BLEND 0.85
#define GROUND_COVER_FAR_OCCLUSION_FADE 0.7

float GroundCoverRank(float3 instancePosition)
{
    uint state = (asuint(instancePosition.x) * 0x8DA6B343u) ^ (asuint(instancePosition.z) * 0xD8163841u);
    state = state * 747796405u + 2891336453u;
    state = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    state = (state >> 22u) ^ state;

    return float(state >> 8u) * (1.0 / 16777216.0);
}

float GroundCoverBand(float start, float end, float x)
{
    return smoothstep(0.0, 1.0, saturate((x - start) / max(end - start, 1e-3)));
}

float GroundCoverViewpointDistance(
    WorldShaderData world,
    float3 position)
{
    float nearest = 0.0;
    bool found = false;

    for (uint viewpointIndex = 0; viewpointIndex < 4; viewpointIndex++)
    {
        const float4 viewpoint = world.terrain_lod_viewpoints[viewpointIndex];

        if (viewpoint.w > 0.0)
        {
            const float viewpointDistance = distance(viewpoint.xyz, position);

            nearest = found ? min(nearest, viewpointDistance) : viewpointDistance;
            found = true;
        }
    }

    return nearest;
}

/// \return x = height scale, y = width scale of a terrain grass instance
float2 GroundCoverInstanceScale(
    WorldShaderData world,
    float3 origin,
    float rank,
    float spawnTime,
    bool isShadow)
{
    const float range = world.terrain_lod_params.z;
    const float nearRange = world.terrain_lod_params.w;

    const float viewpointDistance = GroundCoverViewpointDistance(world, origin);

    const float thinning = GroundCoverBand(nearRange * GROUND_COVER_THIN_START, nearRange * GROUND_COVER_THIN_END, viewpointDistance);
    const float thinnedKeep = lerp(1.0, GROUND_COVER_FAR_RANK_CUTOFF, thinning);

    // the band narrows as thinning ends, so the instances far tiles keep are all full size
    const float thinningSoftness = GROUND_COVER_RANK_SOFTNESS * (1.0 - thinning);
    float size = saturate((thinnedKeep + thinningSoftness - rank) / (thinningSoftness + 1e-3));

    const float fadeThreshold = (GROUND_COVER_FAR_RANK_CUTOFF + GROUND_COVER_RANK_SOFTNESS) * (1.0 - GroundCoverBand(range * GROUND_COVER_FADE_START, range, viewpointDistance));
    size *= saturate((fadeThreshold - rank) / GROUND_COVER_RANK_SOFTNESS);

    if (isShadow)
    {
        const float shadowThreshold = (1.0 + GROUND_COVER_RANK_SOFTNESS) * (1.0 - GroundCoverBand(nearRange * 0.5, nearRange, viewpointDistance));
        size *= saturate((shadowThreshold - rank) / GROUND_COVER_RANK_SOFTNESS);
    }

    size *= smoothstep(0.0, 1.0, saturate((world.time_params.y - spawnTime) / GROUND_COVER_GROW_SECONDS));

    return float2(size, pow(thinnedKeep, -GROUND_COVER_WIDEN_EXPONENT) * lerp(0.75, 1.0, size));
}

float GroundCoverFlattening(
    WorldShaderData world,
    float3 origin)
{
    const float nearRange = world.terrain_lod_params.w;

    return GroundCoverBand(nearRange * GROUND_COVER_FLATTEN_START, nearRange * GROUND_COVER_FLATTEN_END, GroundCoverViewpointDistance(world, origin));
}

// in the mesh's own space, so the base stays on the ground under the instance's sheared basis
float3 ApplyGroundCoverScale(
    float3 localPosition,
    float2 scale)
{
    return localPosition * float3(scale.y, scale.x, scale.y);
}

#endif // HYP_GROUND_COVER_HLSLI
