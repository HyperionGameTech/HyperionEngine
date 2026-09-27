#ifndef HYP_INSTANCING_HLSLI
#define HYP_INSTANCING_HLSLI

static const uint s_offsetOfIndices = 64; // 64 bytes after header start
static const uint s_offsetOfInstanceSlots = s_offsetOfIndices + (sizeof(uint) * MAX_INSTANCES_PER_BATCH);

#ifdef INSTANCING

void LoadEntityIndexAndDataOffset(uint instanceId, out uint entityIndex, out uint dataOffset)
{
    const uint data = EntityInstanceBatchBuffer.Load<uint>(s_offsetOfIndices + (instanceId * sizeof(uint)));
    entityIndex = data & 0xFFFFFFu;
    dataOffset = data >> 24;
}

uint LoadInstanceSlot(uint dataOffset)
{
    return EntityInstanceBatchBuffer.Load<uint>(s_offsetOfInstanceSlots + (dataOffset * sizeof(uint)));
}

// shaders that read the instance data buffer with their own layout (e.g UI) define HYP_CUSTOM_INSTANCE_DATA
#ifndef HYP_CUSTOM_INSTANCE_DATA

float4x4 LoadInstanceTransform(uint dataOffset)
{
    return InstanceTransforms[LoadInstanceSlot(dataOffset)].transform;
}

float4x4 LoadPreviousInstanceTransform(uint dataOffset)
{
    return InstanceTransforms[LoadInstanceSlot(dataOffset)].previousTransform;
}

float3x3 GetInstanceNormalMatrix(float4x4 transform)
{
    const float3x3 basis = (float3x3)transform;

    const float3 row0 = cross(basis[1], basis[2]);
    const float3 row1 = cross(basis[2], basis[0]);
    const float3 row2 = cross(basis[0], basis[1]);

    const float determinant = dot(basis[0], row0);

    if (abs(determinant) < 1e-12)
    {
        return float3x3(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0);
    }

    return float3x3(row0, row1, row2) / determinant;
}

#endif // !HYP_CUSTOM_INSTANCE_DATA

#endif // INSTANCING

#endif // HYP_INSTANCING_HLSLI