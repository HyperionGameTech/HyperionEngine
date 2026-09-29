/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static constexpr uint32 GroundUploadGroupSize = 64;
static constexpr uint32 MaxGroupsPerDimension = 65535;

struct GlimmerGroundUploadConstants
{
    Vec4i texelMinExtent; // xy = absolute texel min, zw = extent
    Vec4u info;           // x = level, y = offset into the heights buffer, z = groups along x
};

GlimmerSurfaceCache::GlimmerSurfaceCache()
    : m_groundShaderData {}
{
    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        const float texelSize = GetGlimmerGroundTexelSize(levelIndex);
        m_groundShaderData.levels[levelIndex].params = Vec4f(texelSize, 1.0f / texelSize, 0.0f, 0.0f);
    }
}

GlimmerSurfaceCache::~GlimmerSurfaceCache()
{
    m_ground = Handle<Texture>();
}

void GlimmerSurfaceCache::CreateTextures()
{
    m_ground = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2DArray,
        TextureFormat::R32F,
        Vec3u(GlimmerGroundResolution, GlimmerGroundResolution, 1),
        TextureFilterMode::Nearest,
        TextureFilterMode::Nearest,
        TextureWrapMode::Repeat,
        uint16(GlimmerGroundLevels),
        ImageUsage::Storage | ImageUsage::Sampled });

    m_ground->SetIsTransient(true);
    m_ground->SetName(NAME("GlimmerGround"));
    Check(m_ground->Create());
}

const GpuImageViewRef& GlimmerSurfaceCache::GetGroundImageView() const
{
    return RI.textureViewCache->GetOrCreate(m_ground);
}

void GlimmerSurfaceCache::Update(Frame* frame, const GlimmerChannelState& state, Span<const GlimmerGroundUpload> groundUploads)
{
    HYP_SCOPE;

    if (!m_ground.IsValid())
    {
        CreateTextures();
    }

    CommandRecorder& cr = frame->cr;

    if (groundUploads.Size() != 0)
    {
        size_t totalHeights = 0;

        for (const GlimmerGroundUpload& upload : groundUploads)
        {
            totalHeights += upload.heights.Size();
        }

        GpuBufferRef heightsBuffer = RI.MakeGpuBuffer(GpuBufferType::StructuredBuffer, MathUtil::Max(totalHeights, size_t(1)) * sizeof(float), alignof(float));
        Check(heightsBuffer->Create());

        GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(totalHeights * sizeof(float));
        Assert(stagingBuffer != nullptr);

        size_t writeOffset = 0;

        for (const GlimmerGroundUpload& upload : groundUploads)
        {
            stagingBuffer->Copy(writeOffset * sizeof(float), upload.heights.ByteSize(), upload.heights.Data());
            writeOffset += upload.heights.Size();
        }

        stagingBuffer->Flush(0, totalHeights * sizeof(float));

        cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);
        cr << InsertBarrier(heightsBuffer.Get(), ResourceState::CopyDst);
        cr << CopyBuffer(stagingBuffer, heightsBuffer.Get(), uint32(totalHeights * sizeof(float)));
        cr << InsertBarrier(heightsBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);

        cr << InsertBarrier(m_ground->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerGroundUpload")));

        size_t readOffset = 0;

        for (const GlimmerGroundUpload& upload : groundUploads)
        {
            const uint32 numTexels = upload.extent.x * upload.extent.y;
            const uint32 numGroups = MathUtil::Max((numTexels + GroundUploadGroupSize - 1) / GroundUploadGroupSize, 1u);
            const uint32 groupsX = MathUtil::Min(numGroups, MaxGroupsPerDimension);
            const uint32 groupsY = (numGroups + groupsX - 1) / groupsX;

            GlimmerGroundUploadConstants constants {};
            constants.texelMinExtent = Vec4i(upload.texelMin.x, upload.texelMin.y, int32(upload.extent.x), int32(upload.extent.y));
            constants.info = Vec4u(upload.level, uint32(readOffset), groupsX, 0);

            GpuBuffer* cbuffer = nullptr;
            size_t cbufferOffset = 0;
            size_t cbufferSize = 0;

            RI.cbufferAllocator->Write(&constants);
            RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

            cr << SetShaderUniform(0, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
            cr << SetShaderUniform(1, "HeightsBuffer"_sh, heightsBuffer.Get(), ShaderDataOffset(0, sizeof(float)));
            cr << SetShaderUniform(2, "OutGround"_sh, RI.textureViewCache->GetOrCreate(m_ground));

            cr << DispatchCompute(Vec3u { groupsX, groupsY, 1 });

            readOffset += upload.heights.Size();
        }

        cr << InsertBarrier(m_ground->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);

        EnqueueDeletion(std::move(heightsBuffer));
    }

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        const GlimmerGroundLevelState& levelState = state.groundLevels[levelIndex];

        m_groundShaderData.levels[levelIndex].validRect = Vec4i(levelState.validMin.x, levelState.validMin.y, levelState.validMax.x, levelState.validMax.y);
    }
}

} // namespace Hyperion
