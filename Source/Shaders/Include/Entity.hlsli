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
    uint lightmap_volume_index;
    uint material_index;
    uint skeleton_index;

    uint bucket;
    uint flags;
    uint _pad0;
    uint _pad1;

    float4 _pad2;
};

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
#ifdef VERTEX_SHADER
#define OBJECT_INSTANCE_DATA (batch.indices[instanceId >> 2][instanceId & 3])
#define OBJECT_INDEX (OBJECT_INSTANCE_DATA & 0xFFFFFFu)
#define OBJECT_DATA_OFFSET (OBJECT_INSTANCE_DATA >> 24)
#endif // VERTEX_SHADER
#define entity (entities[OBJECT_INDEX])
#endif // INSTANCING

#endif // HYP_ENTITY
