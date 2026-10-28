/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/RawBuffer.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/CrashHandler.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

namespace Hyperion {

#pragma region StructuredBuffer

void RawBuffer::Initialize()
{
    if (gpuBuffer->IsCreated())
    {
        return;
    }

    if (!Check(gpuBuffer->Create()))
    {
        CrashHandler::Dump();
    }
}

void RawBuffer::Shutdown()
{
    if (!gpuBuffer)
    {
        return;
    }

    if (IsOnThread(g_renderThread))
    {
        RI.deferredFlushBuffers.Erase(this);
    }

    EnqueueDeletion(gpuBuffer);
    gpuBuffer = nullptr;

    cpuBuffer.Clear();

    dirtyRangeStart = SIZE_MAX;
    dirtyRangeEnd = 0;
}

void RawBuffer::Write(size_t offset, size_t count, const void* data)
{
    Assert(offset + count <= TotalSizeBytes());
    Assert(!(flags & RawBufferFlags::GpuOnly));
    
    if (flags & RawBufferFlags::CpuWriteCoherent)
    {
        void* ptr = gpuBuffer->Map();
        Memory::Copy(((ubyte*)ptr) + offset, data, count);
    }
    else
    {
        cpuBuffer.Write(count, offset, data);
    }

    dirtyRangeStart = MathUtil::Min(dirtyRangeStart, offset);
    dirtyRangeEnd = MathUtil::Max(dirtyRangeEnd, offset + count);
}

void RawBuffer::Fill(size_t offset, size_t count, ubyte value)
{
    Assert(offset + count <= TotalSizeBytes());
    Assert(!(flags & RawBufferFlags::GpuOnly));
    
    if (flags & RawBufferFlags::CpuWriteCoherent)
    {
        void* ptr = gpuBuffer->Map();
        Memory::Fill(((ubyte*)ptr) + offset, value, count);
    }
    else
    {
        cpuBuffer.Fill(count, offset, value);
    }

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
    Assert(!(flags & RawBufferFlags::GpuOnly));

    // Capped to allow us to use SIZE_MAX and to prevent OOM.
    const size_t realDirtyRangeEnd = MathUtil::Min(dirtyRangeEnd, gpuBuffer->Size());

    if (flags & RawBufferFlags::CpuWriteCoherent)
    {
        // In the case of a coherent buffer, dirty range start/end are used as markers for where we wrote and need to flush.
        gpuBuffer->Flush(dirtyRangeStart, realDirtyRangeEnd - dirtyRangeStart);
    }
    else
    {
        GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(realDirtyRangeEnd - dirtyRangeStart);
        Assert(stagingBuffer != nullptr);

        Memory::Copy(stagingBuffer->Map(), cpuBuffer.Data() + dirtyRangeStart, realDirtyRangeEnd - dirtyRangeStart);

        stagingBuffer->InsertBarrier(&cmdBuffer, ResourceState::CopySrc);
        gpuBuffer->InsertBarrier(&cmdBuffer, ResourceState::CopyDst);
        gpuBuffer->CopyFrom(&cmdBuffer, stagingBuffer, 0, dirtyRangeStart, realDirtyRangeEnd - dirtyRangeStart);
        gpuBuffer->InsertBarrier(&cmdBuffer, ResourceState::ShaderResource);

        RI.stagingBufferPool->RetainForCommandBuffer(stagingBuffer, &cmdBuffer);
    }

    dirtyRangeStart = SIZE_MAX;
    dirtyRangeEnd = 0;
}

void RawBuffer::Flush()
{
    if (!IsDirty())
    {
        return;
    }
    
    if (flags & RawBufferFlags::CpuWriteCoherent)
    {
        // Capped to allow us to use SIZE_MAX and to prevent OOM.
        const size_t realDirtyRangeEnd = MathUtil::Min(dirtyRangeEnd, gpuBuffer->Size());

        // In the case of a coherent buffer, dirrty range start/end are used as markers for where we wrote and need to flush.
        gpuBuffer->Flush(dirtyRangeStart, realDirtyRangeEnd - dirtyRangeStart);

        dirtyRangeStart = SIZE_MAX;
        dirtyRangeEnd = 0;

        // Don't allocate a command buffer for coherent buffers
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

bool RawBuffer::Resize(size_t newSize)
{
    Assert(!(flags & RawBufferFlags::CpuWriteCoherent),
        "Cannot dynamically resize a coherent buffer because the original data would be lost");

    const size_t bufferAlignment = gpuBuffer->Alignment();

    if (bufferAlignment != 0)
    {
        newSize = ByteUtil::AlignAs(newSize, bufferAlignment);
    }

    if (newSize == gpuBuffer->Size())
    {
        return false;
    }

    const GpuBufferType bufferType = gpuBuffer->GetBufferType();

    // Create the new buffer, if the existing one has been created already.
    const bool needCreate = gpuBuffer->IsCreated();

    EnqueueDeletion(gpuBuffer);

    gpuBuffer = new GpuBuffer(bufferType, newSize, bufferAlignment);

    if (flags & RawBufferFlags::CpuReadCoherent)
    {
        gpuBuffer->SetIsCpuAccessible(true);
    }

    if (needCreate)
    {
        if (!Check(gpuBuffer->Create()))
        {
            CrashHandler::Dump();
        }
    }
    
    if (flags & RawBufferFlags::GpuOnly)
    {
        dirtyRangeStart = SIZE_MAX;
        dirtyRangeEnd = 0;

        return true;
    }

    cpuBuffer.SetSize(newSize, /* zeroize */ true);

    // Need to copy everything over from CPU to GPU on next flush.
    dirtyRangeStart = 0;
    dirtyRangeEnd = SIZE_MAX;

    return true;
}

#pragma endregion RawBuffer

} // namespace Hyperion
