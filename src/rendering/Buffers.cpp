/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Buffers.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderFrame.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/utilities/ByteUtil.hpp>
#include <core/reflection/TypeInfo.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/threading/Threads.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

extern HYP_API const char* LookupTypeName(TypeId typeId);

#pragma region StagingBufferPool

static thread_local StagingBufferPool* s_stagingBufferPool = nullptr;

struct StagingBufferPoolImpl
{
    static constexpr uint32 MaxFramesBeforeDiscard = 300; // about 5 seconds at 60fps

    struct CachedStagingBuffer
    {
        uint32 offset = 0;
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

    FlatSet<CachedStagingBuffer> cachedBuffers[NumFramesInFlight];

    ~StagingBufferPoolImpl()
    {
        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            for (CachedStagingBuffer& cachedBuffer : cachedBuffers[frameIndex])
            {
                SafeDelete(std::move(cachedBuffer.buffer));
            }
        }
    }

    void Cleanup(uint32 frameIndex)
    {
        const uint32 currFrame = RenderApi::GetFrameCounter();

        for (auto it = cachedBuffers[frameIndex].Begin(); it != cachedBuffers[frameIndex].End();)
        {
            const int64 frameDiff = int64(currFrame) - int64(it->lastUsedFrame);

            if (frameDiff >= MaxFramesBeforeDiscard)
            {
                GpuBufferRef& gpuBuffer = it->buffer;
                SafeDelete(std::move(gpuBuffer));

                it = cachedBuffers[frameIndex].Erase(it);

                continue;
            }

            ++it;
        }
    }

    GpuBufferBase* GetOrCreateBuffer(uint32 frameIndex, uint32 offset, uint32 bufferSize)
    {
        const uint32 currFrame = RenderApi::GetFrameCounter();

        // unused one (different frame)
        for (CachedStagingBuffer& cachedBuffer : cachedBuffers[frameIndex])
        {
            // find first that fits to reuse
            if (cachedBuffer.size >= bufferSize && cachedBuffer.lastUsedFrame != currFrame)
            {
                cachedBuffer.offset = offset;
                cachedBuffer.size = bufferSize;
                cachedBuffer.lastUsedFrame = currFrame;

                HYP_GFX_ASSERT(cachedBuffer.buffer != nullptr
                    && cachedBuffer.buffer->IsCreated()
                    && cachedBuffer.buffer->Size() >= bufferSize);

                return cachedBuffer.buffer;
            }
        }

        // create new one if none found
        CachedStagingBuffer newBuffer;
        newBuffer.offset = offset;
        newBuffer.size = bufferSize;
        newBuffer.lastUsedFrame = currFrame;
        newBuffer.buffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, bufferSize);

#ifdef HYP_DEBUG_MODE
        newBuffer.buffer->SetDebugName(HYP_NAME("StagingBufferPoolTempBuffer"));
#endif

        HYP_GFX_ASSERT(newBuffer.buffer->Create());

        auto insertResult = cachedBuffers[frameIndex].Insert(std::move(newBuffer));
        HYP_GFX_ASSERT(insertResult.second); // must be inserted

        return insertResult.first->buffer;
    }
};

StagingBufferPool::StagingBufferPool()
    : m_impl(MakePimpl<StagingBufferPoolImpl>())
{
}

StagingBufferPool& StagingBufferPool::GetInstance()
{
    if (!s_stagingBufferPool)
    {
        s_stagingBufferPool = new StagingBufferPool();

        Threads::CurrentThreadObject()->AtExit([]()
            {
                delete s_stagingBufferPool;
            });
    }

    return *s_stagingBufferPool;
}

void StagingBufferPool::Cleanup(uint32 frameIndex)
{
    m_impl->Cleanup(frameIndex);
}

GpuBufferBase* StagingBufferPool::AcquireStagingBuffer(uint32 frameIndex, uint32 offset, uint32 bufferSize)
{
    return m_impl->GetOrCreateBuffer(frameIndex, offset, bufferSize);
}

#pragma endregion StagingBufferPool

#pragma region GpuBufferHolderBase

GpuBufferHolderBase::~GpuBufferHolderBase()
{
    SafeDelete(std::move(m_gpuBuffer));
}

void GpuBufferHolderBase::CreateBuffers(GpuBufferType bufferType, SizeType initialCount, SizeType size)
{
    HYP_SCOPE;
    // Threads::AssertOnThread(g_renderThread);

    if (initialCount == 0)
    {
        initialCount = 1;
    }

    const SizeType structSize = TypeInfo_GetSize(*m_structTypeInfo);
    AssertDebug(size == structSize, "Size does not match the expected size! Size = {}, Expected = {}", size, structSize);

    const SizeType gpuBufferSize = MathUtil::NextMultiple(size * initialCount, structSize);

    m_gpuBuffer = g_renderBackend->MakeGpuBuffer(bufferType, gpuBufferSize);
    m_gpuBuffer->SetDebugName(NAME_FMT("GpuBufferHolder_{}", *m_structTypeInfo->name));
    DeferCreate(m_gpuBuffer);
}

void GpuBufferHolderBase::CopyToGpuBuffer(
    FrameBase* frame,
    const Array<GpuBufferBase*>& stagingBuffers,
    const Array<uint32>& chunkStarts,
    const Array<uint32>& chunkEnds)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    if (stagingBuffers.Empty())
    {
        return;
    }

    Assert(stagingBuffers.Size() == chunkStarts.Size());
    Assert(stagingBuffers.Size() == chunkEnds.Size());

    // gauranteed to be ordered ascending due to the way we build the staging buffers
    const uint32 rangeStart = chunkStarts.Front();
    const uint32 rangeEnd = chunkEnds.Back();

    const uint32 frameIndex = frame->GetFrameIndex();
    RenderQueue& rq = frame->preRenderQueue;

    Assert(m_gpuBuffer != nullptr);

    const SizeType requiredBufferSize = rangeEnd;

    Assert(m_gpuBuffer->Size() >= requiredBufferSize);

    rq << InsertBarrier(m_gpuBuffer, RS_COPY_DST);

    for (SizeType i = 0; i < stagingBuffers.Size(); i++)
    {
        GpuBufferBase* stagingBuffer = stagingBuffers[i];
        const uint32 chunkStart = chunkStarts[i];
        const uint32 chunkEnd = chunkEnds[i];

        Assert(stagingBuffer != nullptr);
        Assert(stagingBuffer->IsCreated());
        Assert(chunkEnd >= chunkStart);
        Assert(chunkEnd - chunkStart <= stagingBuffer->Size(),
            "Staging buffer size is too small! Staging buffer size = {}, required size = {}",
            stagingBuffer->Size(), chunkEnd - chunkStart);

        rq << InsertBarrier(stagingBuffer, RS_COPY_SRC);

        rq << CopyBuffer(stagingBuffer, m_gpuBuffer, 0, chunkStart, (chunkEnd - chunkStart));
    }

    rq << InsertBarrier(m_gpuBuffer, m_gpuBuffer->GetBufferType() == GpuBufferType::SSBO ? RS_UNORDERED_ACCESS : RS_SHADER_RESOURCE);
}

#pragma endregion GpuBufferHolderBase

} // namespace hyperion