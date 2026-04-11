#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/aabb.inc"
#include "./Shared.inc"

DECLARE_SAMPLER(ComputeVisibility, SamplerNearest) SamplerState depth_pyramid_sampler;
DECLARE_SRV(ComputeVisibility, DepthPyramidResult) Texture2D depth_pyramid;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/Entity.inc"
#include "../include/scene.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_BUFFER_DYNAMIC(ComputeVisibility, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

DECLARE_SRV(ComputeVisibility, EntitiesBuffer) StructuredBuffer<Entity> entities;

DECLARE_BUFFER(ComputeVisibility, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

struct IndirectDrawCommand
{
    // VkDrawIndexedIndirectCommand
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
};

struct ObjectInstance
{
    uint entity_binding_index;
    uint draw_command_index;
    uint batch_index;
    uint _pad;
};

DECLARE_SRV(ComputeVisibility, ObjectInstancesBuffer) StructuredBuffer<ObjectInstance> instances;

DECLARE_UAV(ComputeVisibility, IndirectDrawCommandsBuffer) RWStructuredBuffer<IndirectDrawCommand> drawCommands;

DECLARE_UAV(ComputeVisibility, EntityInstanceBatchesBuffer) RWStructuredBuffer<uint4> entityInstanceBatchData;

DECLARE_BUFFER(ComputeVisibility, ComputeVisibilityConstants) cbuffer ComputeVisibilityConstants
{
    uint2 depth_pyramid_dimensions;
    uint batch_offset;
    uint num_instances;
    uint batch_stride;
};

bool IsPixelVisible(float3 clip_min, float3 clip_max)
{
    float2 dim = (clip_max.xy - clip_min.xy) * 0.5 * float2(depth_pyramid_dimensions);

    return max(dim.x, dim.y) < 16.0;
}

float GetDepthAtTexel(float2 texcoord, int mip)
{
    uint mip_width, mip_height, mip_levels;
    depth_pyramid.GetDimensions(mip, mip_width, mip_height, mip_levels);

    const int2 mip_dimensions = int2(mip_width, mip_height);

    float4 value = TEXEL_FETCH_2D_LOD(
        depth_pyramid_sampler,
        depth_pyramid,
        clamp(int2(texcoord * float2(mip_dimensions)), (int2)0, mip_dimensions - 1),
        mip);

    return value.r;
}

[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint id = dispatchThreadID.x;
    const uint index = id; // batch_offset + id;

    if (id >= num_instances)
    {
        return;
    }

    ObjectInstance object_instance = instances[index];
    const uint entity_binding_index = object_instance.entity_binding_index;
    const uint draw_command_index = object_instance.draw_command_index;

    // entity binding index should not ever be ~0u/unset,
    // but we should make sure we do not cause a gpu crash by indexing
    // an invalid slot.
    if (entity_binding_index == ~0u)
    {
        return;
    }

    Entity currEntity = entities[entity_binding_index];

    bool is_visible = false;

    AABB aabb;
    aabb.max = currEntity.world_aabb_max.xyz;
    aabb.min = currEntity.world_aabb_min.xyz;

    uint cull_bits = 0x7Fu;
    const bool skip_check = bool(GET_OBJECT_BUCKET_MASK(currEntity) & OBJECT_MASK_SKY);

    is_visible = skip_check;

    if (!skip_check)
    {
        float4x4 view = camera.view;
        float4x4 proj = camera.projection;

        float4 clip_pos = float4(0.0, 0.0, 0.0, 0.0);
        float3 clip_min = float3(1.0, 1.0, 1.0);
        float3 clip_max = float3(-1.0, -1.0, 0.0);

        // transform worldspace aabb to screenspace
        for (int i = 0; i < 8; i++)
        {
            float4 projected_corner = mul(proj, mul(view, float4(AABBGetCorner(aabb, i), 1.0)));
            cull_bits &= GetCullBits(projected_corner);

            clip_pos = projected_corner;
            clip_pos.z = max(clip_pos.z, 0.0);
            clip_pos.xyz /= clip_pos.w;
            clip_pos.xy = clamp(clip_pos.xy, -1.0, 1.0);

            clip_min = min(clip_pos.xyz, clip_min);
            clip_max = max(clip_pos.xyz, clip_max);
        }

        if (cull_bits == 0u)
        {
            clip_min.xy = clip_min.xy * float2(0.5, 0.5) + float2(0.5, 0.5);
            clip_max.xy = clip_max.xy * float2(0.5, 0.5) + float2(0.5, 0.5);

            float2 dimensions = float2(depth_pyramid_dimensions);

            // Calculate hi-Z buffer mip
            float2 size = (clip_max.xy - clip_min.xy) * max(dimensions.x, dimensions.y);
            int mip = (int)ceil(log2(max(size.x, size.y)));

            const float4 depths = float4(
                GetDepthAtTexel(clip_min.xy, mip),
                GetDepthAtTexel(float2(clip_max.x, clip_min.y), mip),
                GetDepthAtTexel(float2(clip_min.x, clip_max.y), mip),
                GetDepthAtTexel(clip_max.xy, mip)
            );

            // find the max depth
            float max_depth = max(max(max(depths.x, depths.y), depths.z), depths.w);
            is_visible = (clip_min.z <= max_depth + 0.001);
        }
    }

    if (is_visible)
    {
        uint instance_index;
        InterlockedAdd(drawCommands[draw_command_index].instance_count, 1u, instance_index);

        if (object_instance.batch_index != ~0u && instance_index < MAX_ENTITIES_PER_INSTANCE_BATCH)
        {
            uint bufferOffsetWords = object_instance.batch_index * batch_stride / 4;
            // `indices` member of EntityInstanceBatch has offset of 16 bytes (4 words)
            uint indicesOffsetWords = bufferOffsetWords + 4 + instance_index;

            uint oldValue;
            InterlockedExchange(entityInstanceBatchData[indicesOffsetWords >> 2][indicesOffsetWords & 3], entity_binding_index, oldValue);
        }
    }
}
