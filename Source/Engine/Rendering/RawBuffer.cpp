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

    ClearDirty();
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

    MarkDirty(offset, count);
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

    MarkDirty(offset, count);
}

uint32 RawBuffer::CollectDirtyRanges(RawBufferDirtyRange (&outRanges)[MaxDirtyRanges]) const
{
    const size_t bufferSize = gpuBuffer->Size();

    uint32 numRanges = 0;

    for (uint32 i = 0; i < numDirtyRanges; i++)
    {
        const RawBufferDirtyRange range { dirtyRanges[i].start, MathUtil::Min(dirtyRanges[i].end, bufferSize) };

        if (range.start >= range.end)
        {
            continue;
        }

        uint32 insertIndex = numRanges;

        while (insertIndex > 0 && outRanges[insertIndex - 1].start > range.start)
        {
            outRanges[insertIndex] = outRanges[insertIndex - 1];
            --insertIndex;
        }

        outRanges[insertIndex] = range;
        ++numRanges;
    }

    // ranges grown after they were added can overlap their neighbours
    uint32 numMerged = 0;

    for (uint32 i = 0; i < numRanges; i++)
    {
        if (numMerged != 0 && outRanges[i].start <= outRanges[numMerged - 1].end)
        {
            outRanges[numMerged - 1].end = MathUtil::Max(outRanges[numMerged - 1].end, outRanges[i].end);

            continue;
        }

        outRanges[numMerged++] = outRanges[i];
    }

    return numMerged;
}

void RawBuffer::FlushAllInto(CommandBuffer& cmdBuffer, Span<RawBuffer* const> buffers)
{
    struct PendingCopy
    {
        const RawBuffer* buffer;
        size_t stagingOffset;
        RawBufferDirtyRange range;
    };

    static constexpr size_t StagingCopyAlignment = 16;

    FatArray<PendingCopy, InlineAllocator<32, RenderAllocator>> copies;
    FatArray<const GpuBuffer*, InlineAllocator<16, RenderAllocator>> dstBuffers;

    size_t stagingSize = 0;

    for (RawBuffer* buffer : buffers)
    {
        if (!buffer->IsDirty())
        {
            continue;
        }

        Assert(buffer->gpuBuffer && buffer->gpuBuffer->IsCreated());
        Assert(!(buffer->flags & RawBufferFlags::GpuOnly));

        RawBufferDirtyRange ranges[MaxDirtyRanges];
        const uint32 numRanges = buffer->CollectDirtyRanges(ranges);

        if (buffer->flags & RawBufferFlags::CpuWriteCoherent)
        {
            for (uint32 i = 0; i < numRanges; i++)
            {
                buffer->gpuBuffer->Flush(ranges[i].start, ranges[i].end - ranges[i].start);
            }

            buffer->ClearDirty();

            continue;
        }

        for (uint32 i = 0; i < numRanges; i++)
        {
            copies.PushBack(PendingCopy { buffer, stagingSize, ranges[i] });

            stagingSize += ByteUtil::AlignAs(ranges[i].end - ranges[i].start, StagingCopyAlignment);
        }

        if (numRanges != 0)
        {
            dstBuffers.PushBack(buffer->gpuBuffer);
        }

        buffer->ClearDirty();
    }

    if (copies.Empty())
    {
        return;
    }

    GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(stagingSize);
    Assert(stagingBuffer != nullptr);

    ubyte* stagingData = static_cast<ubyte*>(stagingBuffer->Map());

    for (const PendingCopy& copy : copies)
    {
        Memory::Copy(stagingData + copy.stagingOffset, copy.buffer->cpuBuffer.Data() + copy.range.start, copy.range.end - copy.range.start);
    }

    const Span<const GpuBuffer* const> dstBufferSpan(dstBuffers.Data(), dstBuffers.Size());

    stagingBuffer->InsertBarrier(&cmdBuffer, ResourceState::CopySrc);
    GpuBuffer::InsertBarriers(&cmdBuffer, dstBufferSpan, ResourceState::CopyDst);

    for (const PendingCopy& copy : copies)
    {
        copy.buffer->gpuBuffer->CopyFrom(
            &cmdBuffer,
            stagingBuffer,
            uint32(copy.stagingOffset),
            uint32(copy.range.start),
            uint32(copy.range.end - copy.range.start));
    }

    GpuBuffer::InsertBarriers(&cmdBuffer, dstBufferSpan, ResourceState::ShaderResource);

    RI.stagingBufferPool->RetainForCommandBuffer(stagingBuffer, &cmdBuffer);
}

void RawBuffer::FlushInto(CommandBuffer& cmdBuffer)
{
    RawBuffer* self = this;

    FlushAllInto(cmdBuffer, Span<RawBuffer* const>(&self, 1));
}

void RawBuffer::Flush()
{
    if (!IsDirty())
    {
        return;
    }
    
    if (flags & RawBufferFlags::CpuWriteCoherent)
    {
        RawBufferDirtyRange ranges[MaxDirtyRanges];
        const uint32 numRanges = CollectDirtyRanges(ranges);

        for (uint32 i = 0; i < numRanges; i++)
        {
            gpuBuffer->Flush(ranges[i].start, ranges[i].end - ranges[i].start);
        }

        ClearDirty();

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
    
    ClearDirty();

    if (flags & RawBufferFlags::GpuOnly)
    {
        return true;
    }

    cpuBuffer.SetSize(newSize, /* zeroize */ true);

    // Need to copy everything over from CPU to GPU on next flush.
    MarkDirty(0, newSize);

    return true;
}

#pragma endregion RawBuffer

} // namespace Hyperion
