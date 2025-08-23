/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Buffers.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderFrame.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/utilities/ByteUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/threading/Threads.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

extern HYP_API const char* LookupTypeName(TypeId typeId);

#pragma region StagingBufferPool

thread_local StagingBufferPool* g_stagingBufferPool = nullptr;

struct StagingBufferPoolImpl
{
    struct CachedStagingBuffer
    {
        uint32 offset = 0;
        uint32 size = 0;
        uint32 lastUsedFrame = uint32(-1);
        GpuBufferRef buffer;
    };

    Array<CachedStagingBuffer> cachedBuffers[g_framesInFlight];
    
    ~StagingBufferPoolImpl()
    {
        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            for (CachedStagingBuffer& cachedBuffer : cachedBuffers[frameIndex])
            {
                SafeDelete(std::move(cachedBuffer.buffer));
            }
        }
    }

    void Cleanup(uint32 frameIndex)
    {
        const uint32 currFrame = RenderApi_GetFrameCounter();

        for (auto it = cachedBuffers[frameIndex].Begin(); it != cachedBuffers[frameIndex].End();)
        {
            const int64 frameDiff = int64(currFrame) - int64(it->lastUsedFrame);

            if (frameDiff >= 10 / g_framesInFlight)
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
        const uint32 currFrame = RenderApi_GetFrameCounter();

        // unused one (different frame)
        for (CachedStagingBuffer& cachedBuffer : cachedBuffers[frameIndex])
        {
            if (cachedBuffer.size == bufferSize && cachedBuffer.lastUsedFrame != currFrame)
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
        CachedStagingBuffer& cachedBuffer = cachedBuffers[frameIndex].EmplaceBack();
        cachedBuffer.offset = offset;
        cachedBuffer.size = bufferSize;
        cachedBuffer.lastUsedFrame = currFrame;
        cachedBuffer.buffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, bufferSize);
        HYP_GFX_ASSERT(cachedBuffer.buffer->Create());

        return cachedBuffer.buffer;
    }
};

StagingBufferPool::StagingBufferPool()
    : m_impl(MakePimpl<StagingBufferPoolImpl>())
{
}

StagingBufferPool& StagingBufferPool::GetInstance()
{
    if (!g_stagingBufferPool)
    {
        g_stagingBufferPool = new StagingBufferPool();

        Threads::CurrentThreadObject()->AtExit([]()
            {
                delete g_stagingBufferPool;
            });
    }

    return *g_stagingBufferPool;
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
    //Threads::AssertOnThread(g_renderThread);

    if (initialCount == 0)
    {
        initialCount = 1;
    }

    AssertDebug(m_structSize > 0);

    const SizeType gpuBufferSize = MathUtil::NextMultiple(size * initialCount, m_structSize);

    m_gpuBuffer = g_renderBackend->MakeGpuBuffer(bufferType, gpuBufferSize);
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