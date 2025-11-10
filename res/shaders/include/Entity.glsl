#ifndef HYP_OBJECT_GLSL
#define HYP_OBJECT_GLSL

#include "defines.inc"

struct Entity
{
    mat4 model_matrix;
    mat4 previous_model_matrix;

    vec4 _pad0;
    vec4 _pad1;
    vec4 world_aabb_max;
    vec4 world_aabb_min;

    uint entity_index;
    uint lightmap_volume_index;
    uint material_index;
    uint skeleton_index;

    uint bucket;
    uint _pad2;
    uint _pad3;
    uint _pad4;

    uvec4 user_data0;
    uvec4 user_data1;
};

#define MAX_ENTITIES_PER_INSTANCE_BATCH 60

struct EntityInstanceBatch
{
    uint batch_index;
    uint num_entities;
    uint _pad0;
    uint _pad1;

    uvec4 indices[MAX_ENTITIES_PER_INSTANCE_BATCH / 4];
    mat4 transforms[MAX_ENTITIES_PER_INSTANCE_BATCH];
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
#define OBJECT_INDEX (entity_instance_batch.indices[gl_InstanceIndex >> 2][gl_InstanceIndex & 3])
#define entity (entities[OBJECT_INDEX])
#elif defined(FRAGMENT_SHADER)
#define entity (entities[v_object_index])
#endif
#endif

#endif