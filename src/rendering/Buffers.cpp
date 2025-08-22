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

#pragma region GpuBufferHolderBase

GpuBufferHolderBase::~GpuBufferHolderBase()
{
    SafeDelete(std::move(m_gpuBuffer));

    for (CachedStagingBuffer& it : m_cachedStagingBuffers)
    {
        SafeDelete(std::move(it.stagingBuffer));
    }
}

void GpuBufferHolderBase::CreateBuffers(GpuBufferType type, SizeType initialCount, SizeType size)
{
    HYP_SCOPE;
    //Threads::AssertOnThread(g_renderThread);

    if (initialCount == 0)
    {
        initialCount = 1;
    }

    AssertDebug(m_structSize > 0);

    const SizeType gpuBufferSize = MathUtil::NextMultiple(size * initialCount, m_structSize);

    m_gpuBuffer = g_renderBackend->MakeGpuBuffer(type, gpuBufferSize);
    DeferCreate(m_gpuBuffer);
}

GpuBufferRef GpuBufferHolderBase::CreateStagingBuffer(uint32 size)
{
    HYP_SCOPE;

    GpuBufferRef stagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, size);
    Assert(stagingBuffer->Create());

    return stagingBuffer;
}

void GpuBufferHolderBase::ApplyPendingUpdates(FrameBase* frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    if (m_pendingUpdates.Empty())
    {
        return;
    }

    RenderQueue& rq = frame->renderQueue;

    Assert(m_gpuBuffer != nullptr);

    // ensure the updates are sorted, so the last update gives us the necessary buffer size
    std::sort(m_pendingUpdates.Begin(), m_pendingUpdates.End(),
        [](const PendingGpuBufferUpdate& a, const PendingGpuBufferUpdate& b)
        {
            return a.offset + a.count < b.offset + b.count;
        });

    const SizeType requiredBufferSize = m_pendingUpdates.Back().offset + m_pendingUpdates.Back().count;

    HYP_GFX_ASSERT(m_gpuBuffer->EnsureCapacity(requiredBufferSize));

    rq << InsertBarrier(m_gpuBuffer, RS_COPY_DST);

    for (PendingGpuBufferUpdate& pendingUpdate : m_pendingUpdates)
    {
        AssertDebug(pendingUpdate.stagingBuffer != nullptr);
        AssertDebug(pendingUpdate.offset + pendingUpdate.count <= m_gpuBuffer->Size());
        
        const SizeType startOffset = pendingUpdate.offset - (pendingUpdate.offset % pendingUpdate.stagingBuffer->Size());
        
        const uint32 srcOffset = 0;
        const uint32 dstOffset = pendingUpdate.offset;

        rq << InsertBarrier(pendingUpdate.stagingBuffer, RS_COPY_SRC);

        rq << CopyBuffer(pendingUpdate.stagingBuffer, m_gpuBuffer, srcOffset, dstOffset, pendingUpdate.count);
    }

    rq << InsertBarrier(
        m_gpuBuffer,
        m_gpuBuffer->GetBufferType() == GpuBufferType::SSBO
            ? RS_UNORDERED_ACCESS
            : RS_SHADER_RESOURCE);

    const uint32 currFrame = RenderApi_GetFrameCounter();

    m_pendingUpdates.Clear();

    // Remove cached staging buffers that are older than 10 frames
    for (auto it = m_cachedStagingBuffers.Begin(); it != m_cachedStagingBuffers.End();)
    {
        CachedStagingBuffer& cachedStagingBuffer = *it;

        const int64 frameDiff = int64(currFrame) - int64(it->lastFrame);

        if (frameDiff >= 10)
        {
            SafeDelete(std::move(cachedStagingBuffer.stagingBuffer));

            it = m_cachedStagingBuffers.Erase(it);
        }
        else
        {
            ++it;
        }
    }
}

#pragma endregion GpuBufferHolderBase

} // namespace hyperion