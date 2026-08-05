#ifndef HYP_INSTANCING_HLSLI
#define HYP_INSTANCING_HLSLI

#ifdef INSTANCING
static const uint s_offsetOfIndices = 64; // 64 bytes after header start
static const uint s_offsetOfTransforms = s_offsetOfIndices + (sizeof(uint4) * (MAX_ENTITIES_PER_INSTANCE_BATCH >> 2));
static const uint s_offsetOfPrevTransforms = s_offsetOfTransforms + (sizeof(float4x4) * MAX_ENTITIES_PER_INSTANCE_BATCH);

float4x4 LoadInstanceTransform(uint offset)
{
    return float4x4(
        EntityInstanceBatchBuffer.Load<float4>(offset + 0),
        EntityInstanceBatchBuffer.Load<float4>(offset + 16),
        EntityInstanceBatchBuffer.Load<float4>(offset + 32),
        EntityInstanceBatchBuffer.Load<float4>(offset + 48));
}

#endif // INSTANCING

#endif // HYP_INSTANCING_HLSLI