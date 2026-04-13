/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/CommandRecorder.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/Frame.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/utilities/ByteUtil.hpp>
#include <Core/reflection/TypeInfo.hpp>

#include <Core/profiling/ProfileScope.hpp>

#include <Core/threading/Threads.hpp>

#include <engine/EngineGlobals.hpp>

namespace Hyperion {

HYP_API extern const char* LookupTypeName(const TypeId& typeId);

#pragma region StagingBufferPool

static thread_local StagingBufferPool* s_stagingBufferPool = nullptr;

struct StagingBufferPoolImpl
{
    static constexpr uint32 MaxFramesBeforeDiscard = 300; // about 5 seconds at 60fps

    struct CachedStagingBuffer
    {
        uint32 size = 0;
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
    Array<CachedStagingBuffer, RenderAllocator> usedBuffers[NumFramesInFlight];
    SharedMutex mutex;

    ~StagingBufferPoolImpl() = default;

    void OnFrameStart()
    {
        auto& used = usedBuffers[GetFrameCounter() % NumFramesInFlight];

        // recycle it
        for (CachedStagingBuffer& usedBuffer : used)
        {
            auto lowerBoundIt = cachedBuffers.LowerBound(usedBuffer);
            cachedBuffers.Insert(lowerBoundIt, std::move(usedBuffer));
        }

        used.Clear();
    }

    void OnFrameEnd()
    {
    }

    void Cleanup()
    {
        const uint32 currFrame = GetFrameCounter();

        for (auto it = cachedBuffers.Begin(); it != cachedBuffers.End();)
        {
            const int64 frameDiff = int64(currFrame) - int64(it->lastUsedFrame);

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

    GpuBuffer* GetOrCreateBuffer(uint32 bufferSize)
    {
        TUniqueLock lock(mutex);

        const uint32 currFrame = GetFrameCounter();

        auto lowerBoundIt = cachedBuffers.LowerBound(CachedStagingBuffer { bufferSize });

        // unused one (different frame)
        for (auto it = lowerBoundIt != cachedBuffers.End() ? lowerBoundIt : cachedBuffers.Begin();
            it != cachedBuffers.End();)
        {
            auto& cachedBuffer = *it;

            // find first that fits to reuse and is not too big (don't want to waste space creating more larger buffers for those that need it after us)
            if (cachedBuffer.size >= bufferSize && cachedBuffer.size < bufferSize * 2)
            {
                cachedBuffer.size = bufferSize;
                cachedBuffer.lastUsedFrame = currFrame;

                GpuBuffer* buffer = cachedBuffer.buffer.Get();

                Assert(buffer != nullptr
                    && buffer->IsCreated()
                    && buffer->Size() >= bufferSize);
                
                auto& used = usedBuffers[currFrame % NumFramesInFlight];
                auto cached = std::move(cachedBuffer);

                cachedBuffers.Erase(it);

                return used.PushBack(std::move(cached)).buffer.Get();
            }

            ++it;
        }

        // bump it up a bit
        bufferSize = MathUtil::NextMultiple(bufferSize, 256);

        // create new one if none found
        CachedStagingBuffer newBuffer;
        newBuffer.size = bufferSize;
        newBuffer.lastUsedFrame = currFrame;
        newBuffer.buffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, bufferSize, 256);

#if HYP_DEBUG_MODE
        newBuffer.buffer->SetDebugName(NAME("StagingBufferPoolTempBuffer"));
#endif

        Assert(newBuffer.buffer->Create());

        void* dataPtr = newBuffer.buffer->Map();
        Assert(dataPtr != nullptr);

        Memory::Zero(dataPtr, bufferSize);
                
        auto& used = usedBuffers[currFrame % NumFramesInFlight];
        return used.PushBack(std::move(newBuffer)).buffer.Get();
    }
};

StagingBufferPool::StagingBufferPool()
    : m_impl(MakePimpl<StagingBufferPoolImpl>())
{
}

void StagingBufferPool::OnFrameStart()
{
    m_impl->OnFrameStart();
}

void StagingBufferPool::OnFrameEnd()
{
    m_impl->OnFrameEnd();
}

void StagingBufferPool::Cleanup()
{
    m_impl->Cleanup();
}

GpuBuffer* StagingBufferPool::AcquireStagingBuffer(uint32 bufferSize)
{
    return m_impl->GetOrCreateBuffer(bufferSize);
}

#pragma endregion StagingBufferPool

#pragma region GpuBufferHolderBase

GpuBufferHolderBase::~GpuBufferHolderBase()
{
    EnqueueDeletion(std::move(m_gpuBuffer));
}

void GpuBufferHolderBase::CreateBuffers(GpuBufferType bufferType, size_t initialCount, size_t size)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (initialCount == 0)
    {
        initialCount = 1;
    }

    const size_t structSize = TypeInfo_GetSize(*m_structTypeInfo);
    AssertDebug(size == structSize, "Size does not match the expected size! Size = {}, Expected = {}", size, structSize);

    const size_t gpuBufferSize = MathUtil::NextMultiple(size * initialCount, structSize);

    if (m_gpuBuffer.IsValid())
    {
        EnqueueDeletion(std::move(m_gpuBuffer));
    }

    m_gpuBuffer = g_renderInterface->MakeGpuBuffer(bufferType, gpuBufferSize);

#if HYP_DEBUG_MODE
    m_gpuBuffer->SetDebugName(NAME_FMT("GpuBufferHolder_{}", *m_structTypeInfo->name));
#endif
    
    m_gpuBuffer->SetIsCpuAccessible(m_cpuAccessible);
    CheckResult(m_gpuBuffer->Create());
}

void GpuBufferHolderBase::CopyStagingToGpu(
    uint32 frameIndex, CommandRecorder& cr,
    Span<GpuBuffer* const> stagingBuffers,
    Span<const uint32> chunkStarts,
    Span<const uint32> chunkEnds)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (chunkEnds.Size() == 0)
    {
        return;
    }

    AssertDebug(stagingBuffers.Size() == chunkStarts.Size());
    AssertDebug(stagingBuffers.Size() == chunkEnds.Size());

    // gauranteed to be ordered ascending due to the way we build the staging buffers
    const uint32 rangeStart = chunkStarts[0];
    const uint32 rangeEnd = chunkEnds[chunkEnds.Size() - 1];

    AssertDebug(m_gpuBuffer != nullptr);

    const size_t requiredBufferSize = rangeEnd;

    AssertDebug(m_gpuBuffer->Size() >= requiredBufferSize);

    cr << InsertBarrier(m_gpuBuffer, RS_COPY_DST);

    for (size_t i = 0; i < stagingBuffers.Size(); i++)
    {
        GpuBuffer* stagingBuffer = stagingBuffers[i];
        const uint32 chunkStart = chunkStarts[i];
        const uint32 chunkEnd = chunkEnds[i];

        AssertDebug(stagingBuffer != nullptr);
        AssertDebug(stagingBuffer->IsCreated());
        AssertDebug(chunkEnd >= chunkStart);
        AssertDebug(chunkEnd - chunkStart <= stagingBuffer->Size(),
            "Staging buffer size is too small! Staging buffer size = {}, required size = {}",
            stagingBuffer->Size(), chunkEnd - chunkStart);

        cr << InsertBarrier(stagingBuffer, RS_COPY_SRC);

        cr << CopyBuffer(stagingBuffer, m_gpuBuffer, 0, chunkStart, (chunkEnd - chunkStart));
    }

    cr << InsertBarrier(m_gpuBuffer, m_gpuBuffer->GetBufferType() == GpuBufferType::STORAGE_BUFFER ? RS_UNORDERED_ACCESS : RS_SHADER_RESOURCE);
}

#pragma endregion GpuBufferHolderBase

} // namespace Hyperion
