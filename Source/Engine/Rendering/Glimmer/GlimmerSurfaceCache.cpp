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
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static constexpr uint32 GroundUploadGroupSize = 64;
static constexpr uint32 MaxGroupsPerDimension = 65535;

static constexpr uint32 GroundAlbedoGroupSize = 8;

struct GlimmerGroundUploadConstants
{
    Vec4i texelMinExtent; // xy = absolute texel min, zw = extent
    Vec4u info;           // x = level, y = offset into the heights buffer, z = groups along x
};

// Must match GlimmerGroundAlbedoConstants in Shaders/Glimmer/GlimmerGroundAlbedo.hlsl
struct GlimmerGroundAlbedoConstants
{
    GlimmerGroundShaderData ground;
    Vec4i windowOrigins; // xy = the level's window origin, zw = its origin at the level's last fill
    Vec4u info;          // x = level, y = number of terrain patches, z = 1 when the last fill's origin is valid
};

GlimmerSurfaceCache::GlimmerSurfaceCache()
    : m_groundShaderData {},
      m_albedoGeneration(0),
      m_albedoFillCounter(0)
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
    m_groundAlbedo = Handle<Texture>();

    EnqueueDeletion(std::move(m_terrainPatchesBuffer));
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

    m_groundAlbedo = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2DArray,
        TextureFormat::RGBA8,
        Vec3u(GlimmerGroundResolution, GlimmerGroundResolution, 1),
        TextureFilterMode::Nearest,
        TextureFilterMode::Nearest,
        TextureWrapMode::Repeat,
        uint16(GlimmerGroundLevels),
        ImageUsage::Storage | ImageUsage::Sampled });

    m_groundAlbedo->SetIsTransient(true);
    m_groundAlbedo->SetName(NAME("GlimmerGroundAlbedo"));
    Check(m_groundAlbedo->Create());

    m_terrainPatchesBuffer = RI.MakeGpuBuffer(GpuBufferType::StructuredBuffer, GlimmerMaxTerrainPatches * sizeof(GlimmerTerrainPatchShaderData), alignof(Vec4f));
    Check(m_terrainPatchesBuffer->Create());
}

const GpuImageViewRef& GlimmerSurfaceCache::GetGroundImageView() const
{
    return RI.textureViewCache->GetOrCreate(m_ground);
}

const GpuImageViewRef& GlimmerSurfaceCache::GetGroundAlbedoImageView() const
{
    return RI.textureViewCache->GetOrCreate(m_groundAlbedo);
}

void GlimmerSurfaceCache::UploadTerrainPatches(Frame* frame, Span<const GlimmerTerrainPatchShaderData> terrainPatches)
{
    const size_t numPatches = MathUtil::Min(terrainPatches.Size(), size_t(GlimmerMaxTerrainPatches));
    const size_t byteSize = numPatches * sizeof(GlimmerTerrainPatchShaderData);

    if (numPatches == m_uploadedTerrainPatches.Size()
        && (byteSize == 0 || Memory::Compare(reinterpret_cast<const ubyte*>(m_uploadedTerrainPatches.Data()), reinterpret_cast<const ubyte*>(terrainPatches.Data()), byteSize) == 0))
    {
        return;
    }

    m_uploadedTerrainPatches.Resize(numPatches);

    if (numPatches == 0)
    {
        return;
    }

    Memory::Copy(reinterpret_cast<ubyte*>(m_uploadedTerrainPatches.Data()), reinterpret_cast<const ubyte*>(terrainPatches.Data()), byteSize);

    GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(byteSize);
    Assert(stagingBuffer != nullptr);

    stagingBuffer->Copy(0, byteSize, m_uploadedTerrainPatches.Data());
    stagingBuffer->Flush(0, byteSize);

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);
    cr << InsertBarrier(m_terrainPatchesBuffer.Get(), ResourceState::CopyDst);
    cr << CopyBuffer(stagingBuffer, m_terrainPatchesBuffer.Get(), uint32(byteSize));
    cr << InsertBarrier(m_terrainPatchesBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
}

void GlimmerSurfaceCache::UpdateGroundAlbedo(Frame* frame, const GlimmerChannelState& state)
{
    HYP_SCOPE;

    if (state.groundGeneration != m_albedoGeneration)
    {
        m_albedoGeneration = state.groundGeneration;

        for (AlbedoLevel& albedoLevel : m_albedoLevels)
        {
            albedoLevel = AlbedoLevel {};
        }
    }

    // a level is refilled on its turn, and straight away when its window moves so no slot keeps a texel that scrolled out
    const uint32 scheduledLevel = m_albedoFillCounter++ % GlimmerGroundLevels;

    CommandRecorder& cr = frame->cr;

    bool isFirstDispatch = true;

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        AlbedoLevel& albedoLevel = m_albedoLevels[levelIndex];

        const Vec2i windowOrigin = state.groundLevels[levelIndex].windowOrigin;
        const bool hasMoved = !albedoLevel.hasFilled || albedoLevel.filledOrigin != windowOrigin;

        if (levelIndex != scheduledLevel && !hasMoved)
        {
            continue;
        }

        if (isFirstDispatch)
        {
            isFirstDispatch = false;

            cr << InsertBarrier(m_ground->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
            cr << InsertBarrier(m_groundAlbedo->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
            cr << SetCurrentShader(ShaderDesc(NAME("GlimmerGroundAlbedo")));
        }

        GlimmerGroundAlbedoConstants constants {};
        constants.ground = m_groundShaderData;
        constants.windowOrigins = Vec4i(windowOrigin.x, windowOrigin.y, albedoLevel.filledOrigin.x, albedoLevel.filledOrigin.y);
        constants.info = Vec4u(levelIndex, uint32(m_uploadedTerrainPatches.Size()), albedoLevel.hasFilled ? 1u : 0u, 0);

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
        cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, RI.textureViewCache->GetOrCreate(m_ground));
        cr << SetShaderUniform(uniformIndex++, "TerrainPatchesBuffer"_sh, m_terrainPatchesBuffer.Get(), ShaderDataOffset(0, sizeof(GlimmerTerrainPatchShaderData)));
        cr << SetShaderUniform(uniformIndex++, "OutGroundAlbedo"_sh, RI.textureViewCache->GetOrCreate(m_groundAlbedo));

        cr << DispatchCompute(Vec3u { GlimmerGroundResolution / GroundAlbedoGroupSize, GlimmerGroundResolution / GroundAlbedoGroupSize, 1 });

        albedoLevel.hasFilled = true;
        albedoLevel.filledOrigin = windowOrigin;
    }

    if (!isFirstDispatch)
    {
        cr << InsertBarrier(m_groundAlbedo->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    }
}

void GlimmerSurfaceCache::Update(Frame* frame, const GlimmerChannelState& state, Span<const GlimmerGroundUpload> groundUploads, Span<const GlimmerTerrainPatchShaderData> terrainPatches)
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

    UploadTerrainPatches(frame, terrainPatches);
    UpdateGroundAlbedo(frame, state);
}

} // namespace Hyperion
