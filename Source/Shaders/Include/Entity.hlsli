#ifndef HYP_ENTITY
#define HYP_ENTITY

#include "Defines.hlsli"

struct Entity
{
    float4x4 model_matrix;
    float4x4 previous_model_matrix;

    float3x4 normal_matrix;

    float4 world_aabb_max;
    float4 world_aabb_min;

    uint entity_index;
    uint lightmap_rect_offset;
    uint material_index;
    uint skeleton_index;

    uint bucket_and_object_mask;
    uint lightmap_rect_size;

    float lod_morph_start;
    float lod_morph_end;

    float4 lod_morph_origin_multiplier;
};

// Map entity's lightmap rect onto uv1 [0,1]
float2 GetLightmapAtlasUV(Entity entity, float2 uv1)
{
    const uint rectOffset = entity.lightmap_rect_offset;
    const uint rectSize = entity.lightmap_rect_size;

    const float2 offsetTexels = float2(rectOffset & 0xFFFu, (rectOffset >> 12u) & 0xFFFu);
    const float2 scaleTexels = float2((rectSize & 0xFFFu) + 1u, ((rectSize >> 12u) & 0xFFFu) + 1u);
    const float2 atlasDimensions = float2(1u << ((rectSize >> 24u) & 0xFu), 1u << ((rectSize >> 28u) & 0xFu));

    return (offsetTexels + uv1 * scaleTexels) / atlasDimensions;
}

uint GetEntityBucket(Entity entity)
{
    return entity.bucket_and_object_mask & 0xFFFFu;
}

uint GetEntityObjectMask(Entity entity)
{
    return entity.bucket_and_object_mask >> 16u;
}

// 0 if the entity has no lightmap it can be routed to
uint GetLightmapStencilValue(Entity entity)
{
    return entity.lightmap_rect_offset >> 24u;
}

#define MAX_INSTANCES_PER_BATCH 256

// Batches only say which slot of the instance data buffer each entry draws
struct EntityInstanceBatch
{
    uint batchIndex;
    uint numEntities;
    uint _pad0;
    uint _pad1;

    uint4 _pad[3]; // pad 48 bytes so struct size % 64 == 0

    uint4 indices[MAX_INSTANCES_PER_BATCH / 4];
    uint4 instanceSlots[MAX_INSTANCES_PER_BATCH / 4];
};

struct InstanceTransform
{
    float4x4 transform;
    float4x4 previousTransform;
};

#ifdef INSTANCING
#endif // INSTANCING

#endif // HYP_ENTITY
