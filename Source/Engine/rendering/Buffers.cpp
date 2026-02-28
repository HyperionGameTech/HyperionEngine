/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderQueue.hpp>
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
                cachedBuffer.buffer.Reset();
            }
        }
    }

    void Cleanup(uint32 frameIndex)
    {
        const uint32 currFrame = GetFrameCounter();

        for (auto it = cachedBuffers[frameIndex].Begin(); it != cachedBuffers[frameIndex].End();)
        {
            const int64 frameDiff = int64(currFrame) - int64(it->lastUsedFrame);

            if (frameDiff >= MaxFramesBeforeDiscard)
            {
                GpuBufferRef& gpuBuffer = it->buffer;
                EnqueueDeletion(std::move(gpuBuffer));

                it = cachedBuffers[frameIndex].Erase(it);

                continue;
            }

            ++it;
        }
    }

    GpuBuffer* GetOrCreateBuffer(uint32 frameIndex, uint32 offset, uint32 bufferSize)
    {
        const uint32 currFrame = GetFrameCounter();

        // unused one (different frame)
        for (CachedStagingBuffer& cachedBuffer : cachedBuffers[frameIndex])
        {
            // find first that fits to reuse
            if (cachedBuffer.size >= bufferSize && (int64(currFrame) - int64(cachedBuffer.lastUsedFrame)) >= NumFramesInFlight)
            {
                cachedBuffer.offset = offset;
                cachedBuffer.size = bufferSize;
                cachedBuffer.lastUsedFrame = currFrame;

                Assert(cachedBuffer.buffer != nullptr
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
        newBuffer.buffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, bufferSize);

#if HYP_DEBUG_MODE
        newBuffer.buffer->SetDebugName(HYP_NAME("StagingBufferPoolTempBuffer"));
#endif

        Assert(newBuffer.buffer->Create());

        auto insertResult = cachedBuffers[frameIndex].Insert(std::move(newBuffer));
        AssertDebug(insertResult.second); // must be inserted

        return insertResult.first->buffer;
    }
};

StagingBufferPool::StagingBufferPool()
    : m_impl(MakePimpl<StagingBufferPoolImpl>())
{
}

void StagingBufferPool::Cleanup(uint32 frameIndex)
{
    m_impl->Cleanup(frameIndex);
}

GpuBuffer* StagingBufferPool::AcquireStagingBuffer(uint32 frameIndex, uint32 offset, uint32 bufferSize)
{
    return m_impl->GetOrCreateBuffer(frameIndex, offset, bufferSize);
}

#pragma endregion StagingBufferPool

#pragma region GpuBufferHolderBase

GpuBufferHolderBase::~GpuBufferHolderBase()
{
    EnqueueDeletion(std::move(m_gpuBuffer));
}

void GpuBufferHolderBase::CreateBuffers(GpuBufferType bufferType, SizeType initialCount, SizeType size)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (initialCount == 0)
    {
        initialCount = 1;
    }

    const SizeType structSize = TypeInfo_GetSize(*m_structTypeInfo);
    AssertDebug(size == structSize, "Size does not match the expected size! Size = {}, Expected = {}", size, structSize);

    const SizeType gpuBufferSize = MathUtil::NextMultiple(size * initialCount, structSize);

    m_gpuBuffer = g_renderInterface->MakeGpuBuffer(bufferType, gpuBufferSize);

#if HYP_DEBUG_MODE
    m_gpuBuffer->SetDebugName(NAME_FMT("GpuBufferHolder_{}", *m_structTypeInfo->name));
#endif
    
    m_gpuBuffer->SetRequireCpuAccessible(m_cpuAccessible);
    CheckResult(m_gpuBuffer->Create());
}

void GpuBufferHolderBase::CopyStagingToGpu(
    uint32 frameIndex, RenderQueue& renderQueue,
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

    const SizeType requiredBufferSize = rangeEnd;

    AssertDebug(m_gpuBuffer->Size() >= requiredBufferSize);

    renderQueue << InsertBarrier(m_gpuBuffer, RS_COPY_DST);

    for (SizeType i = 0; i < stagingBuffers.Size(); i++)
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

        renderQueue << InsertBarrier(stagingBuffer, RS_COPY_SRC);

        renderQueue << CopyBuffer(stagingBuffer, m_gpuBuffer, 0, chunkStart, (chunkEnd - chunkStart));
    }

    renderQueue << InsertBarrier(m_gpuBuffer, m_gpuBuffer->GetBufferType() == GpuBufferType::STORAGE_BUFFER ? RS_UNORDERED_ACCESS : RS_SHADER_RESOURCE);
}

#pragma endregion GpuBufferHolderBase

} // namespace Hyperion
