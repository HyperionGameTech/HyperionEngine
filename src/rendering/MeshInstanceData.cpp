#include <rendering/MeshInstanceData.hpp>

#include <core/math/MathUtil.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <MeshInstanceData.generated.inl>

namespace hyperion {

void MeshInstanceData_OnPostLoad(MeshInstanceData& meshInstanceData)
{
    // Ensure at least one instance
    meshInstanceData.numInstances = MathUtil::Max(meshInstanceData.numInstances, 1u);

    if (meshInstanceData.buffers.Any())
    {
        for (uint32 bufferIndex = 0; bufferIndex < MathUtil::Min(MeshInstanceData::maxBuffers, uint32(meshInstanceData.buffers.Size())); bufferIndex++)
        {
            if (meshInstanceData.buffers[bufferIndex].Size() / meshInstanceData.bufferStructSizes[bufferIndex] != meshInstanceData.numInstances)
            {
                HYP_LOG(Rendering, Warning, "Expected mesh instance data to have a buffer size that is equal to (buffer struct size / number of instances). Buffer size: {}, Buffer struct size: {}, Num instances: {}",
                    meshInstanceData.buffers[bufferIndex].Size(), meshInstanceData.bufferStructSizes[bufferIndex],
                    meshInstanceData.numInstances);
            }
        }

        if (meshInstanceData.buffers.Size() > MeshInstanceData::maxBuffers)
        {
            HYP_LOG(Rendering, Warning, "MeshInstanceData has more buffers than the maximum allowed: {} > {}", meshInstanceData.buffers.Size(), MeshInstanceData::maxBuffers);
        }
    }
}

} // namespace hyperion
