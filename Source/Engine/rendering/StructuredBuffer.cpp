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

template <>
void StructuredBuffer::Update<RenderAllocator>(TCommandRecorder<RenderAllocator>& cr)
{
    Assert(gpuBuffer && gpuBuffer->IsCreated());

    if (IsDirty())
    {
        GpuBuffer* stagingBuffer = g_renderInterface->stagingBufferPool->AcquireStagingBuffer(dirtyRangeEnd - dirtyRangeStart);
        Assert(stagingBuffer != nullptr);

        Memory::Copy(stagingBuffer->Map(), cpuBuffer.Data() + dirtyRangeStart, dirtyRangeEnd - dirtyRangeStart);

        cr << InsertBarrier(stagingBuffer, RS_COPY_SRC);
        cr << InsertBarrier(gpuBuffer, RS_COPY_DST);

        cr << CopyBuffer(stagingBuffer, gpuBuffer, 0, dirtyRangeStart, dirtyRangeEnd - dirtyRangeStart);
        
        cr << InsertBarrier(gpuBuffer, RS_SHADER_RESOURCE);

        dirtyRangeStart = SIZE_MAX;
        dirtyRangeEnd = 0;
    }
}

#pragma endregion StructuredBuffer

} // namespace Hyperion
