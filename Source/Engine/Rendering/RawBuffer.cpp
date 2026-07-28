/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/RawBuffer.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>

namespace Hyperion {

#pragma region StructuredBuffer

void RawBuffer::Initialize()
{
    Assert(gpuBuffer);

    if (gpuBuffer->IsCreated())
    {
        return;
    }

    Check(gpuBuffer->Create());
}

void RawBuffer::Shutdown()
{
    if (!gpuBuffer)
    {
        return;
    }

    delete gpuBuffer;
    gpuBuffer = nullptr;

    cpuBuffer.Clear();

    dirtyRangeStart = SIZE_MAX;
    dirtyRangeEnd = 0;
}

void RawBuffer::Write(size_t offset, size_t count, const void* data)
{
    Assert(offset + count <= cpuBuffer.Size());

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

    GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(dirtyRangeEnd - dirtyRangeStart);
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

    CommandBuffer& cmdBuffer = RI.GetTransientCommandBuffer();
    FlushInto(cmdBuffer);

    RI.SubmitTransientCommandBuffer(cmdBuffer);
}

void RawBuffer::FlushBatched()
{
    if (!IsDirty())
    {
        return;
    }

    RI.DeferFlushBuffer(this);
}

#pragma endregion RawBuffer

} // namespace Hyperion
