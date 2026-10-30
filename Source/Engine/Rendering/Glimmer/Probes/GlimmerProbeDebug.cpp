/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/Probes/GlimmerProbeDebug.hpp>
#include <Rendering/Glimmer/Probes/GlimmerProbeVolume.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
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

static constexpr uint32 ProbeDebugGroupSize = 64;

struct GlimmerProbeDebugConstants
{
    GlimmerProbeVolumeShaderData volume;
    GlimmerGroundShaderData ground;
    GlimmerSHOccupancyShaderData occupancy;
};

#pragma region GlimmerProbeDebug

GlimmerProbeDebug::GlimmerProbeDebug()
    : m_numRecords(0)
{
    for (bool& isPending : m_isPending)
    {
        isPending = false;
    }
}

GlimmerProbeDebug::~GlimmerProbeDebug()
{
    Reset();
}

void GlimmerProbeDebug::CreateResources(uint32 numRecords)
{
    m_numRecords = numRecords;

    m_recordsBuffer = GlimmerHelpers::CreateGlimmerRWBuffer(sizeof(GlimmerProbeDebugRecord), m_numRecords);

    for (GpuBufferRef& readbackBuffer : m_readbackBuffers)
    {
        readbackBuffer = RI.MakeGpuBuffer(GpuBufferType::ReadbackBuffer, m_numRecords * sizeof(GlimmerProbeDebugRecord));
        readbackBuffer->SetIsCpuAccessible(true);
        Check(readbackBuffer->Create());
    }

    for (bool& isPending : m_isPending)
    {
        isPending = false;
    }
}

void GlimmerProbeDebug::Reset()
{
    if (!m_recordsBuffer.IsInitialized())
    {
        return;
    }

    m_recordsBuffer.Shutdown();

    for (GpuBufferRef& readbackBuffer : m_readbackBuffers)
    {
        EnqueueDeletion(std::move(readbackBuffer));
    }

    for (bool& isPending : m_isPending)
    {
        isPending = false;
    }
}

void GlimmerProbeDebug::Update(Frame* frame, const GlimmerProbeVolume& probeVolume, const GlimmerSurfaceCache& surfaceCache, const GlimmerSHOccupancy& occupancy, GlimmerChannel& channel)
{
    HYP_SCOPE;

    if (!probeVolume.IsReady() || !occupancy.GetMaskBuffer().IsInitialized())
    {
        return;
    }

    const uint32 numRecords = probeVolume.GetPoolBlocks() * GlimmerProbesPerBlock;

    if (m_recordsBuffer.IsInitialized() && m_numRecords != numRecords)
    {
        Reset();
    }

    if (!m_recordsBuffer.IsInitialized())
    {
        CreateResources(numRecords);
    }

    const uint32 slot = frame->GetFrameIndex() % NumFramesInFlight;
    GpuBufferRef& readbackBuffer = m_readbackBuffers[slot];

    // this frame slot's last submission has finished by now, so what it copied can be read
    if (m_isPending[slot])
    {
        Array<GlimmerProbeDebugRecord, GlimmerAllocator> records;
        records.Resize(m_numRecords);

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

    cr << InsertBarrier(m_recordsBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerProbeDebug")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSHBuffer"_sh, probeVolume.GetSHBuffer());
    cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStatesBuffer"_sh, probeVolume.GetStatesBuffer());
    cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSlotsBuffer"_sh, probeVolume.GetSlotsBuffer());
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, surfaceCache.GetGroundImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyTexture"_sh, occupancy.GetImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyMaskBuffer"_sh, occupancy.GetMaskBuffer());
    cr << SetShaderUniform(uniformIndex++, "OutRecords"_sh, m_recordsBuffer);

    cr << DispatchCompute(Vec3u { (m_numRecords + ProbeDebugGroupSize - 1) / ProbeDebugGroupSize, 1, 1 });

    cr << InsertBarrier(m_recordsBuffer, ResourceState::CopySrc, ShaderModuleType::Compute);
    cr << InsertBarrier(readbackBuffer.Get(), ResourceState::CopyDst, ShaderModuleType::Compute);

    cr << CopyBuffer(m_recordsBuffer.gpuBuffer, readbackBuffer.Get(), uint32(m_numRecords * sizeof(GlimmerProbeDebugRecord)));

    m_isPending[slot] = true;
}

#pragma endregion GlimmerProbeDebug

} // namespace Hyperion
