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

    uint bucket;
    uint lightmap_rect_size;

    float lod_morph_start;
    float lod_morph_end;

    float4 lod_morph_origin_multiplier;
};

// UV1 is a per-mesh lightmap unwrap in [0, 1]; each entity maps it into its own rect of its volume's atlas.
// Packing matches LightmapVolume::GetEntityLightmapRect()
float2 GetLightmapAtlasUV(Entity entity, float2 uv1)
{
    const uint rectOffset = entity.lightmap_rect_offset;
    const uint rectSize = entity.lightmap_rect_size;

    const float2 offsetTexels = float2(rectOffset & 0xFFFu, (rectOffset >> 12u) & 0xFFFu);
    const float2 scaleTexels = float2((rectSize & 0xFFFu) + 1u, ((rectSize >> 12u) & 0xFFFu) + 1u);
    const float2 atlasDimensions = float2(1u << ((rectSize >> 24u) & 0xFu), 1u << ((rectSize >> 28u) & 0xFu));

    return (offsetTexels + uv1 * scaleTexels) / atlasDimensions;
}

// 0 if the entity has no lightmap it can be routed to
uint GetLightmapStencilValue(Entity entity)
{
    return entity.lightmap_rect_offset >> 24u;
}

#define MAX_ENTITIES_PER_INSTANCE_BATCH 16

struct EntityInstanceBatch
{
    uint batchIndex;
    uint numEntities;
    uint _pad0;
    uint _pad1;

    uint4 _pad[3]; // pad 48 bytes so struct size % 64 == 0

    uint4 indices[MAX_ENTITIES_PER_INSTANCE_BATCH / 4];
    float4x4 transforms[MAX_ENTITIES_PER_INSTANCE_BATCH];
};

struct MeshEntityInstanceBatch
{
    uint batchIndex;
    uint numEntities;
    uint _pad0;
    uint _pad1;

    uint4 _pad[3]; // pad 48 bytes so struct size % 64 == 0

    uint4 indices[MAX_ENTITIES_PER_INSTANCE_BATCH / 4];

    float4x4 transforms[MAX_ENTITIES_PER_INSTANCE_BATCH];
    float4x4 previousTransforms[MAX_ENTITIES_PER_INSTANCE_BATCH];
};

#ifdef INSTANCING
#endif // INSTANCING

#endif // HYP_ENTITY
