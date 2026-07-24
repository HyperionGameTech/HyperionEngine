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
    SharedMutex mutex;

    ~StagingBufferPoolImpl() = default;

    void OnFrameStart(uint32 newFrameIndex)
    {
    }

    void OnFrameEnd(uint32 prevFrameIndex)
    {
        TUniqueLock lock(mutex);

        if (HYP_UNLIKELY(prevFrameIndex < NumFramesInFlight))
        {
            return;
        }

        TBufferCache<CachedStagingBuffer, GpuBufferRef>::RecycleUsedBuffers(
            usedBuffers,
            cachedBuffers,
            prevFrameIndex - NumFramesInFlight);

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

        Check(newBuffer.buffer->Create());

        void* dataPtr = newBuffer.buffer->Map();
        Assert(dataPtr != nullptr);

        Memory::Zero(dataPtr, bufferSize);

        return usedBuffers.PushBack(std::move(newBuffer)).buffer.Get();
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

#pragma endregion StagingBufferPool

} // namespace Hyperion
