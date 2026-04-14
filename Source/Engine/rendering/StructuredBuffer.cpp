/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/StructuredBuffer.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/CommandRecorder.hpp>

namespace Hyperion {

#pragma region StructuredBuffer

void StructuredBuffer::Initialize()
{
    Assert(gpuBuffer);

    if (gpuBuffer->IsCreated())
        return;

    CheckResult(gpuBuffer->Create());
}

void StructuredBuffer::Shutdown()
{
    if (!gpuBuffer)
        return;

    delete gpuBuffer;
    gpuBuffer = nullptr;
}

void StructuredBuffer::Write(size_t offset, size_t count, const void* data)
{
    AssertDebug(offset + count <= cpuBuffer.Size());

    cpuBuffer.Write(count, offset, data);

    dirtyRangeStart = MathUtil::Min(dirtyRangeStart, offset);
    dirtyRangeEnd = MathUtil::Max(dirtyRangeEnd, offset + count);
}

void StructuredBuffer::Flush()
{
    if (!IsDirty())
    {
        return;
    }
    
    Assert(gpuBuffer && gpuBuffer->IsCreated());

    RenderInterface& ri = *g_renderInterface;

    CommandBuffer& cmdBuffer = ri.GetTransientCommandBuffer();

    GpuBuffer* stagingBuffer = ri.stagingBufferPool->AcquireStagingBuffer(dirtyRangeEnd - dirtyRangeStart);
    Assert(stagingBuffer != nullptr);

    Memory::Copy(stagingBuffer->Map(), cpuBuffer.Data() + dirtyRangeStart, dirtyRangeEnd - dirtyRangeStart);

    stagingBuffer->InsertBarrier(&cmdBuffer, RS_COPY_SRC);
    gpuBuffer->InsertBarrier(&cmdBuffer, RS_COPY_DST);
    gpuBuffer->CopyFrom(&cmdBuffer, stagingBuffer, 0, dirtyRangeStart, dirtyRangeEnd - dirtyRangeStart);
    gpuBuffer->InsertBarrier(&cmdBuffer, RS_SHADER_RESOURCE);

    ri.SubmitTransientCommandBuffer(cmdBuffer);

    dirtyRangeStart = SIZE_MAX;
    dirtyRangeEnd = 0;
}

#pragma endregion StructuredBuffer

} // namespace Hyperion
