#ifndef HYP_ENTITY
#define HYP_ENTITY

#include "defines.inc"

struct Entity
{
    float4x4 model_matrix;
    float4x4 previous_model_matrix;
#ifdef LANG_HLSL
    float4x3 normal_matrix;
#else
    mat3 normal_matrix;
#endif

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

    uvec4 _pad[3]; // pad 48 bytes so struct size % 64 == 0

    uint4 indices[MAX_ENTITIES_PER_INSTANCE_BATCH / 4];
    float4x4 transforms[MAX_ENTITIES_PER_INSTANCE_BATCH];
};

struct MeshEntityInstanceBatch
{
    uint batchIndex;
    uint numEntities;
    uint _pad0;
    uint _pad1;

    uvec4 _pad[3]; // pad 48 bytes so struct size % 64 == 0

    uint4 indices[MAX_ENTITIES_PER_INSTANCE_BATCH / 4];
    
    float4x4 transforms[MAX_ENTITIES_PER_INSTANCE_BATCH];
    float4x4 previousTransforms[MAX_ENTITIES_PER_INSTANCE_BATCH];
};

#ifdef INSTANCING
    #if defined(VERTEX_SHADER)
        #ifdef LANG_HLSL
            #define OBJECT_INDEX (entity_instance_batch.indices[instanceId >> 2][instanceId & 3])
        #else
            #define OBJECT_INDEX (entity_instance_batch.indices[gl_InstanceIndex >> 2][gl_InstanceIndex & 3])
        #endif
    #endif
    #define entity (entities[OBJECT_INDEX])
#else // !INSTANCING
    #ifdef LANG_HLSL // for HLSL (non-instancing) we use a StructuredBuffer<Entity> that is offset, so we need to access the entity by the first element
        #define entity (entities[0])
    #endif // LANG_HLSL
#endif // INSTANCING
#endif // HYP_OBJECT_GLSL