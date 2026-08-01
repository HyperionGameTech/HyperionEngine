#include "../include/Defines.hlsli"
#include "../include/Shared.hlsli"
#include "../include/Aabb.hlsli"
#include "./Shared.hlsli"

DECLARE_SAMPLER(ComputeVisibility, SamplerNearest) SamplerState depth_pyramid_sampler;
DECLARE_SRV(ComputeVisibility, DepthPyramidResult) Texture2D depth_pyramid;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/Entity.hlsli"
#include "../include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV_DYNAMIC(ComputeVisibility, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

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
    uint2 depth_pyramid_dimensions;
    uint totalMips;
    uint batch_offset;
    uint num_instances;
    uint batch_stride;
};

float GetDepthAtTexel(float2 texcoord, int mip)
{
    const int2 mip_dimensions = max(int2(depth_pyramid_dimensions.x >> mip, depth_pyramid_dimensions.y >> mip), (int2)1);
    const float4 value = depth_pyramid.Load(int3(clamp(texcoord * mip_dimensions, (int2)0, (int2)mip_dimensions - 1), mip));

    return value.r;
}

[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint id = dispatchThreadID.x;
    const uint index = id;

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
    
    // skip sky
    const bool skip_check = bool(currEntity.bucket == HYP_OBJECT_BUCKET_SKY);

    is_visible = skip_check;

    if (!skip_check)
    {
        float4 clip_pos = float4(0.0, 0.0, 0.0, 0.0);
        float3 clip_min = float3(1.0, 1.0, 1.0);
        float3 clip_max = float3(-1.0, -1.0, 0.0);

        bool intersects_near_plane = false;

        for (int i = 0; i < 8; i++)
        {
            float4 projected_corner = mul(camera.viewProjMat, float4(AABBGetCorner(aabb, i), 1.0));
            cull_bits &= GetCullBits(projected_corner);

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
            is_visible = true;
        }
        else if (cull_bits == 0u)
        {
            // clip space +y is up but texel (0, 0) is top-left, so the y flip swaps which
            // corner is the uv minimum.
            const float2 uv_a = saturate(float2(clip_min.x * 0.5 + 0.5, 0.5 - clip_min.y * 0.5));
            const float2 uv_b = saturate(float2(clip_max.x * 0.5 + 0.5, 0.5 - clip_max.y * 0.5));

            const float2 uv_min = min(uv_a, uv_b);
            const float2 uv_max = max(uv_a, uv_b);

            const float2 dimensions = float2(depth_pyramid_dimensions);

            const float2 size = (uv_max - uv_min) * dimensions;
            const float max_size = max(max(size.x, size.y), 1.0);

            const int mip = clamp((int)ceil(log2(max_size)), 0, (int)totalMips - 1);

            const float4 depths = float4(
                GetDepthAtTexel(uv_min, mip),
                GetDepthAtTexel(float2(uv_max.x, uv_min.y), mip),
                GetDepthAtTexel(float2(uv_min.x, uv_max.y), mip),
                GetDepthAtTexel(uv_max, mip)
            );

            // find the max depth
            const float max_depth = max(max(max(depths.x, depths.y), depths.z), depths.w);

            // If the max depth is 1.0, we hit the sky/far plane
            is_visible = (clip_min.z <= max_depth);
        }
    }
    

    if (is_visible)
    {
        uint instance_index;
        InterlockedAdd(drawCommands[draw_command_index].instance_count, 1u, instance_index);

        if (object_instance.batch_index != ~0u && instance_index < MAX_ENTITIES_PER_INSTANCE_BATCH)
        {
            // uint bufferOffsetWords = object_instance.batch_index * batch_stride / 4;
            // // `indices` member of EntityInstanceBatch has offset of 64 bytes.
            // uint indicesOffsetWords = bufferOffsetWords + 16 + instance_index;

            // uint oldValue;
            // InterlockedExchange(entityInstanceBatchData[indicesOffsetWords >> 2][indicesOffsetWords & 3], entity_binding_index, oldValue);
            //

            // 64 is the byte offset to the 'indices' array
            static const uint sizeofUint = 4;
            uint byteOffset = (object_instance.batch_index * batch_stride) + 64 + (instance_index * sizeofUint);

            uint oldValue;
            entityInstanceBatchData.InterlockedExchange(byteOffset, entity_binding_index, oldValue);
        }
    }
}
