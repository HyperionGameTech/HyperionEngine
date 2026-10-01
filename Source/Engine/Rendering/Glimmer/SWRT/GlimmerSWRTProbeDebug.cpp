/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/SWRT/GlimmerSWRTProbeDebug.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTProbeVolume.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHOccupancy.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static constexpr uint32 NumProbeDebugRecords = GlimmerProbePoolProbes;
static constexpr uint32 ProbeDebugGroupSize = 64;

struct GlimmerProbeDebugConstants
{
    GlimmerProbeVolumeShaderData volume;
    GlimmerGroundShaderData ground;
    GlimmerSHOccupancyShaderData occupancy;
};

#pragma region GlimmerSWRTProbeDebug

GlimmerSWRTProbeDebug::GlimmerSWRTProbeDebug()
{
    for (bool& isPending : m_isPending)
    {
        isPending = false;
    }
}

GlimmerSWRTProbeDebug::~GlimmerSWRTProbeDebug()
{
    Reset();
}

void GlimmerSWRTProbeDebug::CreateResources()
{
    m_recordsBuffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, NumProbeDebugRecords * sizeof(GlimmerProbeDebugRecord), alignof(Vec4f));
    Check(m_recordsBuffer->Create());

    for (GpuBufferRef& readbackBuffer : m_readbackBuffers)
    {
        readbackBuffer = RI.MakeGpuBuffer(GpuBufferType::ReadbackBuffer, NumProbeDebugRecords * sizeof(GlimmerProbeDebugRecord));
        readbackBuffer->SetIsCpuAccessible(true);
        Check(readbackBuffer->Create());
    }

    for (bool& isPending : m_isPending)
    {
        isPending = false;
    }
}

void GlimmerSWRTProbeDebug::Reset()
{
    if (!m_recordsBuffer.IsValid())
    {
        return;
    }

    EnqueueDeletion(std::move(m_recordsBuffer));

    for (GpuBufferRef& readbackBuffer : m_readbackBuffers)
    {
        EnqueueDeletion(std::move(readbackBuffer));
    }

    for (bool& isPending : m_isPending)
    {
        isPending = false;
    }
}

void GlimmerSWRTProbeDebug::Update(Frame* frame, const GlimmerSWRTProbeVolume& probeVolume, const GlimmerSurfaceCache& surfaceCache, const GlimmerSHOccupancy& occupancy, GlimmerChannel& channel)
{
    HYP_SCOPE;

    if (!probeVolume.IsReady())
    {
        return;
    }

    if (!m_recordsBuffer.IsValid())
    {
        CreateResources();
    }

    const uint32 slot = frame->GetFrameIndex() % NumFramesInFlight;
    GpuBufferRef& readbackBuffer = m_readbackBuffers[slot];

    // this frame slot's last submission has finished by now, so what it copied can be read
    if (m_isPending[slot])
    {
        Array<GlimmerProbeDebugRecord> records;
        records.Resize(NumProbeDebugRecords);

        // readback memory is cached, so the GPU's writes aren't visible until the range is invalidated
        readbackBuffer->Invalidate();
        readbackBuffer->Read(records.Size() * sizeof(GlimmerProbeDebugRecord), records.Data());

        channel.PublishProbeDebug(std::move(records));

        m_isPending[slot] = false;
    }

    GlimmerProbeDebugConstants constants {};
    constants.volume = probeVolume.GetShaderData();
    constants.ground = surfaceCache.GetGroundShaderData();
    constants.occupancy = occupancy.GetShaderData();

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(m_recordsBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSWRTProbeDebug")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSHBuffer"_sh, probeVolume.GetSHBuffer().Get(), ShaderDataOffset(0, sizeof(Vec4f)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStatesBuffer"_sh, probeVolume.GetStatesBuffer().Get(), ShaderDataOffset(0, sizeof(Vec4u)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSlotsBuffer"_sh, probeVolume.GetSlotsBuffer().Get(), ShaderDataOffset(0, sizeof(Vec4i)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, surfaceCache.GetGroundImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyTexture"_sh, occupancy.GetImageView());
    cr << SetShaderUniform(uniformIndex++, "OutRecords"_sh, m_recordsBuffer.Get(), ShaderDataOffset(0, sizeof(GlimmerProbeDebugRecord)));

    cr << DispatchCompute(Vec3u { (NumProbeDebugRecords + ProbeDebugGroupSize - 1) / ProbeDebugGroupSize, 1, 1 });

    cr << InsertBarrier(m_recordsBuffer.Get(), ResourceState::CopySrc, ShaderModuleType::Compute);
    cr << InsertBarrier(readbackBuffer.Get(), ResourceState::CopyDst, ShaderModuleType::Compute);

    cr << CopyBuffer(m_recordsBuffer.Get(), readbackBuffer.Get(), uint32(NumProbeDebugRecords * sizeof(GlimmerProbeDebugRecord)));

    m_isPending[slot] = true;
}

#pragma endregion GlimmerSWRTProbeDebug

} // namespace Hyperion
