#ifndef HYP_TERRAIN_MORPH
#define HYP_TERRAIN_MORPH

#include "Entity.hlsli"

// morph_target_heights.x = height on the next coarser LOD's surface, .y = height two LODs coarser.
// morphing is a function of distance only, so a cell and its coarser replacement agree wherever both can represent the surface
float3 ApplyTerrainMorph(Entity terrain_entity, float4x4 model_matrix, float3 local_position, float2 morph_target_heights)
{
    const float morph_start = terrain_entity.lod_morph_start;
    const float morph_end = terrain_entity.lod_morph_end;

    if (morph_end <= morph_start)
    {
        return local_position;
    }

    const float3 world_position = mul(model_matrix, float4(local_position, 1.0)).xyz;
    const float morph_distance = distance(terrain_entity.lod_morph_origin_multiplier.xyz, world_position);

    const float range_multiplier = terrain_entity.lod_morph_origin_multiplier.w;

    const float next_lod_morph = saturate((morph_distance - morph_start) / (morph_end - morph_start));
    const float second_lod_morph = saturate((morph_distance - morph_start * range_multiplier) / ((morph_end - morph_start) * range_multiplier));

    local_position.y = lerp(lerp(local_position.y, morph_target_heights.x, next_lod_morph), morph_target_heights.y, second_lod_morph);

    return local_position;
}

#endif // HYP_TERRAIN_MORPH
