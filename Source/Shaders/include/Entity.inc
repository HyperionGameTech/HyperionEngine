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
};

#define MAX_ENTITIES_PER_INSTANCE_BATCH 16

struct EntityInstanceBatch
{
    uint batchIndex;
    uint numEntities;
    uint _pad0;
    uint _pad1;

    uint4 indices[MAX_ENTITIES_PER_INSTANCE_BATCH / 4];
    float4x4 transforms[MAX_ENTITIES_PER_INSTANCE_BATCH];
};

struct MeshEntityInstanceBatch
{
    uint batchIndex;
    uint numEntities;
    uint _pad0;
    uint _pad1;

    uint4 indices[MAX_ENTITIES_PER_INSTANCE_BATCH / 4];
    
    float4x4 transforms[MAX_ENTITIES_PER_INSTANCE_BATCH];
    float4x4 previousTransforms[MAX_ENTITIES_PER_INSTANCE_BATCH];
};

// can use up to 10 bits.
#define OBJECT_MASK_OPAQUE (0x01)
#define OBJECT_MASK_TRANSLUCENT (0x02)
#define OBJECT_MASK_PARTICLE (0x04)
#define OBJECT_MASK_VEGETATION (0x08)
#define OBJECT_MASK_SKY (0x10)
#define OBJECT_MASK_TERRAIN (0x20)
#define OBJECT_MASK_UI (0x40)
#define OBJECT_MASK_LIGHTMAP_RADIANCE (0x80)
#define OBJECT_MASK_LIGHTMAP_IRRADIANCE (0x100)
#define OBJECT_MASK_LIGHTMAP (OBJECT_MASK_LIGHTMAP_RADIANCE | OBJECT_MASK_LIGHTMAP_IRRADIANCE)
#define OBJECT_MASK_DEBUG (0x200)

#define GET_OBJECT_BUCKET(obj)                                                              \
    ((uint((obj).bucket == HYP_OBJECT_BUCKET_OPAQUE) * (OBJECT_MASK_OPAQUE))                \
        | (uint((obj).bucket == HYP_OBJECT_BUCKET_TRANSLUCENT) * (OBJECT_MASK_TRANSLUCENT)) \
        | (uint((obj).bucket == HYP_OBJECT_BUCKET_DEBUG) * (OBJECT_MASK_DEBUG))             \
        | (uint((obj).bucket == HYP_OBJECT_BUCKET_SKYBOX) * (OBJECT_MASK_SKY)))

#ifdef INSTANCING
    #if defined(VERTEX_SHADER)
        #ifdef LANG_HLSL
            #define OBJECT_INDEX (entity_instance_batch.indices[instanceId >> 2][instanceId & 3])
        #else
            #define OBJECT_INDEX (entity_instance_batch.indices[gl_InstanceIndex >> 2][gl_InstanceIndex & 3])
        #endif
        #define entity (entities[OBJECT_INDEX])
    #elif defined(PIXEL_SHADER)
        #define entity (entities[v_object_index])
    #endif
#else // !INSTANCING
    #ifdef LANG_HLSL // for HLSL (non-instancing) we use a StructuredBuffer<Entity> that is offset, so we need to access the entity by the first element
        #define entity (entities[0])
    #endif // LANG_HLSL
#endif // INSTANCING
#endif // HYP_OBJECT_GLSL