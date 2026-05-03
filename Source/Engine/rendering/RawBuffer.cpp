/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/RawBuffer.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/CommandRecorder.hpp>

namespace Hyperion {

#pragma region StructuredBuffer

void RawBuffer::Initialize()
{
    Assert(gpuBuffer);

    if (gpuBuffer->IsCreated())
        return;

    CheckResult(gpuBuffer->Create());
}

void RawBuffer::Shutdown()
{
    if (!gpuBuffer)
        return;

    delete gpuBuffer;
    gpuBuffer = nullptr;
}

void RawBuffer::Write(size_t offset, size_t count, const void* data)
{
    AssertDebug(offset + count <= cpuBuffer.Size());

    cpuBuffer.Write(count, offset, data);

    dirtyRangeStart = MathUtil::Min(dirtyRangeStart, offset);
    dirtyRangeEnd = MathUtil::Max(dirtyRangeEnd, offset + count);
}

void RawBuffer::FlushInto(CommandBuffer& cmdBuffer)
{
    if (!IsDirty())
    {
        return;
    }

    Assert(gpuBuffer && gpuBuffer->IsCreated());

    RenderInterface& ri = *g_renderInterface;

    GpuBuffer* stagingBuffer = ri.stagingBufferPool->AcquireStagingBuffer(dirtyRangeEnd - dirtyRangeStart);
    Assert(stagingBuffer != nullptr);

    Memory::Copy(stagingBuffer->Map(), cpuBuffer.Data() + dirtyRangeStart, dirtyRangeEnd - dirtyRangeStart);

    stagingBuffer->InsertBarrier(&cmdBuffer, RS_COPY_SRC);
    gpuBuffer->InsertBarrier(&cmdBuffer, RS_COPY_DST);
    gpuBuffer->CopyFrom(&cmdBuffer, stagingBuffer, 0, dirtyRangeStart, dirtyRangeEnd - dirtyRangeStart);
    gpuBuffer->InsertBarrier(&cmdBuffer, RS_SHADER_RESOURCE);

    dirtyRangeStart = SIZE_MAX;
    dirtyRangeEnd = 0;
}

void RawBuffer::Flush()
{
    if (!IsDirty())
    {
        return;
    }

    RenderInterface& ri = *g_renderInterface;

    CommandBuffer& cmdBuffer = ri.GetTransientCommandBuffer();
    FlushInto(cmdBuffer);
    ri.SubmitTransientCommandBuffer(cmdBuffer);
}

#pragma endregion RawBuffer

} // namespace Hyperion
