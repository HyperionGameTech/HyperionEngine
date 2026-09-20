/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/BufferCache.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Utilities/ByteUtil.hpp>
#include <Core/Reflection/TypeInfo.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <Core/Threading/Threads.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {

CORE_API extern const char* LookupTypeName(const TypeId& typeId);

#pragma region StagingBufferPool

struct StagingBufferPoolImpl
{
    static constexpr uint32 MaxFramesBeforeDiscard = 300; // about 5 seconds at 60fps
    static constexpr size_t StagingBufferAlignment = 256;

    struct CachedStagingBuffer
    {
        size_t size = 0;
        uint32 lastUsedFrame = uint32(-1);
        GpuBufferRef buffer;

        // Command buffers that copy from this buffer and have not yet finished executing on the GPU
        Array<const CommandBufferBase*, RenderAllocator> retainers;
        bool wasRetained = false;

        HYP_FORCE_INLINE bool operator==(const CachedStagingBuffer& other) const
        {
            return buffer == other.buffer;
        }

        HYP_FORCE_INLINE bool operator<(const CachedStagingBuffer& other) const
        {
            return size < other.size;
        }
    };

    Array<CachedStagingBuffer, RenderAllocator> cachedBuffers;
    List<CachedStagingBuffer, RenderAllocator> usedBuffers;
    uint32 numRetainers = 0;
    SharedMutex mutex;

    ~StagingBufferPoolImpl() = default;

    void OnFrameStart(uint32 newFrameIndex)
    {
    }

    void OnFrameEnd(uint32 prevFrameIndex)
    {
        TUniqueLock lock(mutex);

        for (auto it = usedBuffers.Begin(); it != usedBuffers.End();)
        {
            CachedStagingBuffer& usedBuffer = *it;

            const bool isRecyclable = usedBuffer.retainers.Empty()
                && (usedBuffer.wasRetained || int64(prevFrameIndex) - int64(usedBuffer.lastUsedFrame) >= MaxFramesBeforeDiscard);

            if (!isRecyclable)
            {
                ++it;

                continue;
            }

            usedBuffer.lastUsedFrame = prevFrameIndex;
            usedBuffer.wasRetained = false;

            auto lowerBoundIt = cachedBuffers.LowerBound(usedBuffer);
            cachedBuffers.Insert(lowerBoundIt, std::move(usedBuffer));

            it = usedBuffers.Erase(it);
        }

        for (auto it = cachedBuffers.Begin(); it != cachedBuffers.End();)
        {
            const int64 frameDiff = int64(prevFrameIndex) - int64(it->lastUsedFrame);

            if (frameDiff >= MaxFramesBeforeDiscard)
            {
                GpuBufferRef& gpuBuffer = it->buffer;
                EnqueueDeletion(std::move(gpuBuffer));

                it = cachedBuffers.Erase(it);

                continue;
            }

            ++it;
        }
    }

    GpuBuffer* GetOrCreateBuffer(size_t bufferSize)
    {
        TUniqueLock lock(mutex);

        const uint32 currFrame = GetFrameCounter();

        CachedStagingBuffer bestMatchEntry;
        auto bestMatchIt = TBufferCache<CachedStagingBuffer, GpuBufferRef>::FindBestMatch(
            cachedBuffers, bufferSize, bestMatchEntry);

        // Use the best match if found
        if (bestMatchIt != cachedBuffers.End())
        {
            bestMatchEntry.lastUsedFrame = currFrame;

            Assert(bestMatchEntry.buffer != nullptr
                   && bestMatchEntry.buffer->IsCreated()
                   && bestMatchEntry.buffer->Size() >= bestMatchEntry.size);

            return TBufferCache<CachedStagingBuffer, GpuBufferRef>::MoveToUsed(
                cachedBuffers, usedBuffers, bestMatchIt, bestMatchEntry);
        }

        // Round up to minimum alignment
        bufferSize = MathUtil::NextMultiple(bufferSize, StagingBufferAlignment);

        // create new one if none found
        CachedStagingBuffer newBuffer;
        newBuffer.size = bufferSize;
        newBuffer.lastUsedFrame = currFrame;
        newBuffer.buffer = RI.MakeGpuBuffer(GpuBufferType::StagingBuffer, bufferSize, StagingBufferAlignment);

#ifdef HYP_RHI_DEBUG_NAMES
        newBuffer.buffer->SetDebugName(NAME("StagingBufferPoolTempBuffer"));
#endif

        if (Check(newBuffer.buffer->Create()))
        {
            void* dataPtr = newBuffer.buffer->Map();
            Assert(dataPtr != nullptr);

            Memory::Zero(dataPtr, bufferSize);
        }

        return usedBuffers.PushBack(std::move(newBuffer)).buffer.Get();
    }

    void RetainForCommandBuffer(const GpuBuffer* buffer, const CommandBufferBase* commandBuffer)
    {
        TUniqueLock lock(mutex);

        for (CachedStagingBuffer& usedBuffer : usedBuffers)
        {
            if (usedBuffer.buffer.Get() != buffer)
            {
                continue;
            }

            if (!usedBuffer.retainers.Contains(commandBuffer))
            {
                usedBuffer.retainers.PushBack(commandBuffer);
                ++numRetainers;
            }

            usedBuffer.wasRetained = true;

            return;
        }
    }

    void ReleaseForCommandBuffer(const CommandBufferBase* commandBuffer)
    {
        TUniqueLock lock(mutex);

        for (CachedStagingBuffer& usedBuffer : usedBuffers)
        {
            if (numRetainers == 0)
            {
                break;
            }

            auto retainerIt = usedBuffer.retainers.Find(commandBuffer);

            if (retainerIt == usedBuffer.retainers.End())
            {
                continue;
            }

            usedBuffer.retainers.Erase(retainerIt);
            --numRetainers;
        }
    }
};

StagingBufferPool::StagingBufferPool()
    : m_impl(MakePimplWithAllocator<StagingBufferPoolImpl, RenderAllocator>())
{
}

void StagingBufferPool::OnFrameStart(uint32 newFrameIndex)
{
    m_impl->OnFrameStart(newFrameIndex);
}

void StagingBufferPool::OnFrameEnd(uint32 prevFrameIndex)
{
    m_impl->OnFrameEnd(prevFrameIndex);
}

GpuBuffer* StagingBufferPool::AcquireStagingBuffer(size_t bufferSize)
{
    return m_impl->GetOrCreateBuffer(bufferSize);
}

void StagingBufferPool::RetainForCommandBuffer(const GpuBuffer* buffer, const CommandBufferBase* commandBuffer)
{
    if (buffer == nullptr || buffer->GetBufferType() != GpuBufferType::StagingBuffer)
    {
        return;
    }

    AssertDebug(commandBuffer != nullptr);

    m_impl->RetainForCommandBuffer(buffer, commandBuffer);
}

void StagingBufferPool::ReleaseForCommandBuffer(const CommandBufferBase* commandBuffer)
{
    m_impl->ReleaseForCommandBuffer(commandBuffer);
}

#pragma endregion StagingBufferPool

} // namespace Hyperion
