#include "../include/Defines.hlsli"
#include "../include/Shared.hlsli"
#include "../include/Aabb.hlsli"
#include "./Shared.hlsli"

DECLARE_SAMPLER(ComputeVisibility, SamplerNearest) SamplerState depth_pyramid_sampler;
DECLARE_SRV(ComputeVisibility, DepthPyramidResult) Texture2D<float2> depth_pyramid;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/Entity.hlsli"
#include "../include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(ComputeVisibility, EntitiesBuffer) StructuredBuffer<Entity> entities;

DECLARE_SRV(ComputeVisibility, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

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

DECLARE_UAV(ComputeVisibility, EntityInstanceBatchesBuffer) RWByteAddressBuffer entityInstanceBatchData;

DECLARE_BUFFER_DYNAMIC(ComputeVisibility, ComputeVisibilityConstants) cbuffer ComputeVisibilityConstants
{
    float4x4 viewProj;
    uint2 hzbDimensions;
    uint totalMips;
    uint batchOffset;
    uint numInstances;
    uint batchStride;
};

float GetMaxDepthAtTexel(float2 texcoord, int mip)
{
    const int2 mipDimensions = max(hzbDimensions >> mip, (int2) 1);
    
    const int2 mip0Coord = clamp(int2(float2(texcoord.x, 1.0 - texcoord.y) * float2(hzbDimensions)), (int2) 0, hzbDimensions - (int2) 1);
    const int2 coord = clamp(mip0Coord >> mip, (int2) 0, mipDimensions - (int2) 1);

    const float2 depths = depth_pyramid.Load(int3(coord, mip));

    return depths.r;
}

[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint id = dispatchThreadID.x;
    const uint index = id;

    if (id >= numInstances)
    {
        return;
    }

    ObjectInstance instance = instances[index];
    const uint entity_binding_index = instance.entity_binding_index;
    const uint draw_command_index = instance.draw_command_index;

    // entity binding index should not ever be ~0u/unset,
    // but we should make sure we do not cause a gpu crash by indexing
    // an invalid slot.
    if (entity_binding_index == ~0u)
    {
        return;
    }

    Entity currEntity = entities[entity_binding_index];

    bool visibility = false;

    AABB aabb;
    aabb.max = currEntity.world_aabb_max.xyz;
    aabb.min = currEntity.world_aabb_min.xyz;

    uint cullBits = 0x7Fu;
    
    // skip sky
    const bool skipVisibilityCheck = bool(currEntity.bucket == HYP_OBJECT_BUCKET_SKY);

    visibility = skipVisibilityCheck;

    if (!skipVisibilityCheck)
    {
        float4 clip_pos = float4(0.0, 0.0, 0.0, 0.0);
        float3 clip_min = float3(1.0, 1.0, 1.0);
        float3 clip_max = float3(-1.0, -1.0, 0.0);

        bool intersects_near_plane = false;

        for (int i = 0; i < 8; i++)
        {
            float4 projected_corner = mul(viewProj, float4(AABBGetCorner(aabb, i), 1.0));
            cullBits &= GetCullBits(projected_corner);

            if (projected_corner.w <= 0.0)
            {
                intersects_near_plane = true;
            }
            else
            {
                clip_pos = projected_corner;
                clip_pos.z = max(clip_pos.z, 0.0);
                clip_pos.xyz /= clip_pos.w;

                clip_min = min(clip_pos.xyz, clip_min);
                clip_max = max(clip_pos.xyz, clip_max);
            }
        }

        if (intersects_near_plane)
        {
            visibility = true;
        }
        else if (cullBits == 0u)
        {
            const float2 uv0 = saturate(float2(clip_min.xy * 0.5 + 0.5));
            const float2 uv1 = saturate(float2(clip_max.xy * 0.5 + 0.5));

            const float2 uvMin = min(uv0, uv1);
            const float2 uvMax = max(uv0, uv1);

            const float2 dimensions = float2(hzbDimensions);

            const float2 size = (uvMax - uvMin) * dimensions;
            const float max_size = max(max(size.x, size.y), 1.0);

            const int mip = clamp((int) ceil(log2(max_size)), 0, (int) totalMips - 1);

            const float4 depths = float4(
                GetMaxDepthAtTexel(uvMin, mip),
                GetMaxDepthAtTexel(float2(uvMax.x, uvMin.y), mip),
                GetMaxDepthAtTexel(float2(uvMin.x, uvMax.y), mip),
                GetMaxDepthAtTexel(uvMax, mip)
            );

            // find the max depth
            const float max_depth = max(max(max(depths.x, depths.y), depths.z), depths.w);

            // If the max depth is 1.0, we hit the sky/far plane
            visibility = (clip_min.z <= max_depth);
        }
    }
    

    if (visibility)
    {
        uint instance_index;
        InterlockedAdd(drawCommands[draw_command_index].instance_count, 1u, instance_index);

        if (instance.batch_index != ~0u && instance_index < MAX_ENTITIES_PER_INSTANCE_BATCH)
        {
            // uint bufferOffsetWords = instance.batch_index * batchStride / 4;
            // // `indices` member of EntityInstanceBatch has offset of 64 bytes.
            // uint indicesOffsetWords = bufferOffsetWords + 16 + instance_index;

            // uint oldValue;
            // InterlockedExchange(entityInstanceBatchData[indicesOffsetWords >> 2][indicesOffsetWords & 3], entity_binding_index, oldValue);
            //

            // 64 is the byte offset to the 'indices' array
            static const uint sizeofUint = 4;
            uint byteOffset = (instance.batch_index * batchStride) + 64 + (instance_index * sizeofUint);

            uint oldValue;
            entityInstanceBatchData.InterlockedExchange(byteOffset, entity_binding_index, oldValue);
        }
    }
}
