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

void StructuredBuffer::Update()
{
    Assert(gpuBuffer && gpuBuffer->IsCreated());

    if (IsDirty())
    {
        // @TODO We'll need to use a separate command buffer than this,
        // as this command buffer may be used for rendering commands and we don't want to insert barriers in the middle of rendering.
        // We could potentially use a transient command buffer for this, but for now we'll just use the main command buffer and insert barriers around the copy.
        CommandBuffer* cmdBuffer = g_renderInterface->GetCurrentCommandBuffer();

        GpuBuffer* stagingBuffer = g_renderInterface->stagingBufferPool->AcquireStagingBuffer(dirtyRangeEnd - dirtyRangeStart);
        Assert(stagingBuffer != nullptr);

        Memory::Copy(stagingBuffer->Map(), cpuBuffer.Data() + dirtyRangeStart, dirtyRangeEnd - dirtyRangeStart);

        stagingBuffer->InsertBarrier(cmdBuffer, RS_COPY_SRC);
        gpuBuffer->InsertBarrier(cmdBuffer, RS_COPY_DST);
        gpuBuffer->CopyFrom(cmdBuffer, stagingBuffer, 0, dirtyRangeStart, dirtyRangeEnd - dirtyRangeStart);
        gpuBuffer->InsertBarrier(cmdBuffer, RS_SHADER_RESOURCE);

        dirtyRangeStart = SIZE_MAX;
        dirtyRangeEnd = 0;
    }
}

#pragma endregion StructuredBuffer

} // namespace Hyperion
