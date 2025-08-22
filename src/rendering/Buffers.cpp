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

#pragma region PendingGpuBufferUpdate

PendingGpuBufferUpdate::PendingGpuBufferUpdate()
{
}

PendingGpuBufferUpdate::PendingGpuBufferUpdate(PendingGpuBufferUpdate&& other) noexcept
    : offset(other.offset),
      count(other.count),
      stagingBuffer(std::move(other.stagingBuffer))
{
    other.offset = 0;
    other.count = 0;
    other.stagingBuffer = nullptr;
}

PendingGpuBufferUpdate& PendingGpuBufferUpdate::operator=(PendingGpuBufferUpdate&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (stagingBuffer)
        SafeDelete(std::move(stagingBuffer));

    offset = other.offset;
    count = other.count;
    stagingBuffer = std::move(other.stagingBuffer);

    other.offset = 0;
    other.count = 0;
    other.stagingBuffer = nullptr;

    return *this;
}

PendingGpuBufferUpdate::~PendingGpuBufferUpdate()
{
    SafeDelete(std::move(stagingBuffer));
}

void PendingGpuBufferUpdate::Init(uint32 alignment)
{
    if (stagingBuffer != nullptr)
    {
        return;
    }

    stagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, s_bufferSize);
    Assert(stagingBuffer->Create());
}

#pragma endregion PendingGpuBufferUpdate

#pragma region GpuBufferHolderBase

GpuBufferHolderBase::~GpuBufferHolderBase()
{
    SafeDelete(std::move(m_gpuBuffer));
    m_pendingUpdates.Clear();
}

void GpuBufferHolderBase::CreateBuffers(GpuBufferType type, SizeType initialCount, SizeType size)
{
    HYP_SCOPE;

    if (initialCount == 0)
    {
        initialCount = 1;
    }

    m_gpuBuffer = g_renderBackend->MakeGpuBuffer(type, size * initialCount);
    DeferCreate(m_gpuBuffer);
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

    m_gpuBuffer->EnsureCapacity(requiredBufferSize);

    const ResourceState prevResourceState = m_gpuBuffer->GetResourceState();
    AssertDebug(prevResourceState == RS_UNORDERED_ACCESS || prevResourceState == RS_SHADER_RESOURCE);

    rq << InsertBarrier(m_gpuBuffer, RS_COPY_DST);

    for (PendingGpuBufferUpdate& pendingUpdate : m_pendingUpdates)
    {
        AssertDebug(pendingUpdate.stagingBuffer != nullptr);
        AssertDebug(pendingUpdate.offset + pendingUpdate.count <= m_gpuBuffer->Size());
        
        const uint32 srcOffset = pendingUpdate.offset % PendingGpuBufferUpdate::s_bufferSize;
        const uint32 dstOffset = pendingUpdate.offset;

        rq << CopyBuffer(pendingUpdate.stagingBuffer, m_gpuBuffer, srcOffset, dstOffset, pendingUpdate.count);
    }

    rq << InsertBarrier(m_gpuBuffer, prevResourceState);

    m_pendingUpdates.Clear();
}

#pragma endregion GpuBufferHolderBase

} // namespace hyperion