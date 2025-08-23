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

#pragma region GpuBufferHolderBase

GpuBufferHolderBase::~GpuBufferHolderBase()
{
    SafeDelete(std::move(m_gpuBuffer));
    SafeDelete(std::move(m_stagingBuffers));
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

    for (GpuBufferRef& stagingBuffer : m_stagingBuffers)
    {
        stagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, gpuBufferSize);
        DeferCreate(stagingBuffer);
    }

    m_gpuBuffer = g_renderBackend->MakeGpuBuffer(bufferType, gpuBufferSize);
    DeferCreate(m_gpuBuffer);
}

void GpuBufferHolderBase::CopyToGpuBuffer(FrameBase* frame, uint32 rangeStart, uint32 rangeEnd)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    if (rangeStart >= rangeEnd)
    {
        return;
    }

    const uint32 frameIndex = frame->GetFrameIndex();
    RenderQueue& rq = frame->renderQueue;

    Assert(m_gpuBuffer != nullptr);
    Assert(m_stagingBuffers[frameIndex] != nullptr);

    const SizeType requiredBufferSize = rangeEnd;

    bool resized = false;
    HYP_GFX_ASSERT(m_gpuBuffer->EnsureCapacity(requiredBufferSize, &resized));
    
    HYP_GFX_ASSERT(rangeEnd <= requiredBufferSize, "Range start: %u, range end: %u, req. buffered size: %u",
        rangeStart, rangeEnd, requiredBufferSize);

    rq << InsertBarrier(m_gpuBuffer, RS_COPY_DST);
    rq << InsertBarrier(m_stagingBuffers[frameIndex], RS_COPY_SRC);
    rq << CopyBuffer(m_stagingBuffers[frameIndex], m_gpuBuffer, rangeStart, rangeStart, (rangeEnd - rangeStart));

    rq << InsertBarrier(m_gpuBuffer, m_gpuBuffer->GetBufferType() == GpuBufferType::SSBO ? RS_UNORDERED_ACCESS : RS_SHADER_RESOURCE);
}

#pragma endregion GpuBufferHolderBase

} // namespace hyperion
