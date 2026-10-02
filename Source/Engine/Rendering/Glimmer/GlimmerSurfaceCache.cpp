/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Framework/EngineStats.hpp>

#include <Core/Containers/Set.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/HashCode.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statGlimmerSurface("Rendering/GPU/Glimmer/Surface");

static constexpr uint32 GroundUploadGroupSize = 64;

static constexpr uint32 GroundAlbedoGroupSize = 8;

struct GlimmerGroundUploadConstants
{
    Vec4i texelMinExtent; // xy = absolute texel min, zw = extent
    Vec4u info;           // x = level, y = offset into the heights buffer, z = groups along x
};

struct GlimmerGroundAlbedoConstants
{
    GlimmerGroundShaderData ground;
    Vec4i windowOrigins; // xy = the level's window origin, zw = its origin at the level's last fill
    Vec4u info;          // x = level, y = number of terrain patches, z = 1 when the last fill's origin is valid
    Vec4f groundCover;   // per splat layer, how much of the ground its plants hide where the layer is full
    Vec4i fillMax;       // xy = absolute texel past the rect being filled (exclusive)
};

static constexpr uint32 MaxAlbedoRects = 4;

static constexpr uint32 AlbedoRefreshFrames = 64;

struct GlimmerGroundCoverConstants
{
    Vec4u materials[GlimmerGroundCoverLayers]; // ~0 where unused or not bound this frame
    Vec4f weights[GlimmerGroundCoverLayers];
};

static_assert(GlimmerGroundCoverMaxMaterials == 4, "GlimmerGroundCoverConstants packs a layer's materials in a Vec4u");

#pragma region GlimmerSurfaceCache

GlimmerSurfaceCache::GlimmerSurfaceCache()
    : m_groundShaderData {},
      m_hasClearedGroundCover(false),
      m_groundCoverCoverage(Vec4f::Zero()),
      m_groundCoverKey {},
      m_hasGroundCoverChanged(true),
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
    EnqueueDeletion(std::move(m_groundCoverAlbedoBuffer));
}

void GlimmerSurfaceCache::CreateTextures()
{
    m_ground = CreateGlimmerStorageTexture(TextureType::Texture2DArray, TextureFormat::R32F, Vec3u(GlimmerGroundResolution, GlimmerGroundResolution, 1), uint16(GlimmerGroundLevels), NAME("GlimmerGround"));
    m_groundAlbedo = CreateGlimmerStorageTexture(TextureType::Texture2DArray, TextureFormat::RGBA8, Vec3u(GlimmerGroundResolution, GlimmerGroundResolution, 1), uint16(GlimmerGroundLevels), NAME("GlimmerGroundAlbedo"));

    m_terrainPatchesBuffer = CreateGlimmerStructuredBuffer(sizeof(GlimmerTerrainPatchShaderData), GlimmerMaxTerrainPatches);

    m_groundCoverAlbedoBuffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, GlimmerGroundCoverLayers * sizeof(Vec4f), alignof(Vec4f));
    Check(m_groundCoverAlbedoBuffer->Create());
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

    {
        Set<uint64> previousPatches;

        for (const GlimmerTerrainPatchShaderData& patch : m_uploadedTerrainPatches)
        {
            previousPatches.Insert(FNV1::DoHashWords(&patch, sizeof(patch), 0));
        }

        for (size_t patchIndex = 0; patchIndex < numPatches; patchIndex++)
        {
            const GlimmerTerrainPatchShaderData& patch = terrainPatches[patchIndex];

            if (!previousPatches.Contains(FNV1::DoHashWords(&patch, sizeof(patch), 0)))
            {
                m_albedoDirtyWorldRects.PushBack(patch.boundsXZ);
            }
        }
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

void GlimmerSurfaceCache::UpdateGroundCover(Frame* frame, const GlimmerChannelState& state)
{
    HYP_SCOPE;

    CommandRecorder& cr = frame->cr;

    if (!m_hasClearedGroundCover)
    {
        m_hasClearedGroundCover = true;

        const FixedArray<Vec4f, GlimmerGroundCoverLayers> unknown {};

        GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(sizeof(unknown));
        Assert(stagingBuffer != nullptr);

        stagingBuffer->Copy(0, sizeof(unknown), unknown.Data());
        stagingBuffer->Flush(0, sizeof(unknown));

        cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);
        cr << InsertBarrier(m_groundCoverAlbedoBuffer.Get(), ResourceState::CopyDst);
        cr << CopyBuffer(stagingBuffer, m_groundCoverAlbedoBuffer.Get(), uint32(sizeof(unknown)));
        cr << InsertBarrier(m_groundCoverAlbedoBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    }

    GlimmerGroundCoverConstants constants {};
    bool hasAnyMaterial = false;

    for (uint32 layerIndex = 0; layerIndex < GlimmerGroundCoverLayers; layerIndex++)
    {
        const GlimmerGroundCoverLayerState& coverState = state.groundCover[layerIndex];

        m_groundCoverCoverage[layerIndex] = coverState.numMaterials != 0 ? coverState.coverage : 0.0f;

        for (uint32 materialIndex = 0; materialIndex < GlimmerGroundCoverMaxMaterials; materialIndex++)
        {
            const uint32 binding = materialIndex < coverState.numMaterials && coverState.materials[materialIndex].IsValid()
                ? Resources::GetBinding(coverState.materials[materialIndex])
                : ~0u;

            constants.materials[layerIndex][materialIndex] = binding;
            constants.weights[layerIndex][materialIndex] = binding != ~0u ? coverState.weights[materialIndex] : 0.0f;

            hasAnyMaterial |= binding != ~0u;
        }
    }

    GlimmerGroundCoverConstantsKey key {};
    Memory::Copy(key.materials, constants.materials, sizeof(key.materials));
    Memory::Copy(key.weights, constants.weights, sizeof(key.weights));
    key.coverage = m_groundCoverCoverage;

    if (Memory::Compare(reinterpret_cast<const ubyte*>(&key), reinterpret_cast<const ubyte*>(&m_groundCoverKey), sizeof(key)) != 0)
    {
        m_groundCoverKey = key;
        m_hasGroundCoverChanged = true;
    }

    if (!hasAnyMaterial)
    {
        return;
    }

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    cr << InsertBarrier(m_groundCoverAlbedoBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerGroundCover")));

    cr << SetShaderUniform(0, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(1, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
    cr << SetShaderUniform(2, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(3, "OutGroundCoverAlbedo"_sh, m_groundCoverAlbedoBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4f)));

    cr << DispatchCompute(Vec3u { 1, 1, 1 });

    cr << InsertBarrier(m_groundCoverAlbedoBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
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

    const uint32 frameIndex = m_albedoFillCounter++;
    const uint32 refreshLevel = frameIndex % AlbedoRefreshFrames == 0 ? (frameIndex / AlbedoRefreshFrames) % GlimmerGroundLevels : ~0u;

    CommandRecorder& cr = frame->cr;

    bool isFirstDispatch = true;

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        AlbedoLevel& albedoLevel = m_albedoLevels[levelIndex];

        const Vec2i windowOrigin = state.groundLevels[levelIndex].windowOrigin;
        const GlimmerTexelRect window = GetGlimmerGroundWindow(windowOrigin);

        const bool isJump = !albedoLevel.hasFilled
            || MathUtil::Abs(windowOrigin.x - albedoLevel.filledOrigin.x) >= int32(GlimmerGroundResolution)
            || MathUtil::Abs(windowOrigin.y - albedoLevel.filledOrigin.y) >= int32(GlimmerGroundResolution);

        bool isWholeWindow = isJump || m_hasGroundCoverChanged || levelIndex == refreshLevel;

        const GlimmerTexelRect& uploadedRect = m_uploadedRects[levelIndex];

        if (!uploadedRect.IsEmpty())
        {
            albedoLevel.unfilledUploads = GlimmerTexelRect::Union(
                albedoLevel.unfilledUploads,
                GlimmerTexelRect { uploadedRect.min - Vec2i(1, 1), uploadedRect.max + Vec2i(1, 1) });
        }

        albedoLevel.unfilledUploads = GlimmerTexelRect::Intersect(albedoLevel.unfilledUploads, window);

        const GlimmerGroundLevelState& levelState = state.groundLevels[levelIndex];
        const GlimmerTexelRect validRect { levelState.validMin, levelState.validMax };
        const GlimmerTexelRect validUploads = GlimmerTexelRect::Intersect(albedoLevel.unfilledUploads, validRect);

        const bool canFillUploads = !albedoLevel.unfilledUploads.IsEmpty()
            && validUploads.min == albedoLevel.unfilledUploads.min
            && validUploads.max == albedoLevel.unfilledUploads.max;

        Array<GlimmerTexelRect> rects;

        const auto addRect = [&](const GlimmerTexelRect& rect)
        {
            const GlimmerTexelRect clipped = GlimmerTexelRect::Intersect(rect, window);

            if (!clipped.IsEmpty())
            {
                rects.PushBack(clipped);
            }
        };

        if (!isWholeWindow)
        {
            if (albedoLevel.filledOrigin != windowOrigin)
            {
                GlimmerTexelRect columns;
                GlimmerTexelRect rows;
                GetGlimmerScrolledRects(GetGlimmerGroundWindow(albedoLevel.filledOrigin), window, columns, rows);

                addRect(columns);
                addRect(rows);
            }

            if (canFillUploads)
            {
                addRect(albedoLevel.unfilledUploads);
            }

            const float invTexelSize = 1.0f / GetGlimmerGroundTexelSize(levelIndex);

            for (const Vec4f& worldRect : m_albedoDirtyWorldRects)
            {
                addRect(GlimmerTexelRect {
                    Vec2i(int32(MathUtil::Floor(worldRect.x * invTexelSize)) - 1, int32(MathUtil::Floor(worldRect.y * invTexelSize)) - 1),
                    Vec2i(int32(MathUtil::Floor(worldRect.z * invTexelSize)) + 2, int32(MathUtil::Floor(worldRect.w * invTexelSize)) + 2) });
            }

            isWholeWindow = rects.Size() > MaxAlbedoRects;
        }

        if (isWholeWindow)
        {
            rects.Clear();
            rects.PushBack(window);
        }

        for (const GlimmerTexelRect& rect : rects)
        {
            if (isFirstDispatch)
            {
                isFirstDispatch = false;

                cr << InsertBarrier(m_ground->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
                cr << InsertBarrier(m_groundAlbedo->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
                cr << SetCurrentShader(ShaderDesc(NAME("GlimmerGroundAlbedo")));
            }

            GlimmerGroundAlbedoConstants constants {};
            constants.ground = m_groundShaderData;
            constants.windowOrigins = Vec4i(rect.min.x, rect.min.y, albedoLevel.filledOrigin.x, albedoLevel.filledOrigin.y);
            constants.info = Vec4u(levelIndex, uint32(m_uploadedTerrainPatches.Size()), albedoLevel.hasFilled ? 1u : 0u, 0);
            constants.groundCover = m_groundCoverCoverage;
            constants.fillMax = Vec4i(rect.max.x, rect.max.y, 0, 0);

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
            cr << SetShaderUniform(uniformIndex++, "GroundCoverAlbedoBuffer"_sh, m_groundCoverAlbedoBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4f)));
            cr << SetShaderUniform(uniformIndex++, "OutGroundAlbedo"_sh, RI.textureViewCache->GetOrCreate(m_groundAlbedo));

            const Vec2u extent = Vec2u(uint32(rect.max.x - rect.min.x), uint32(rect.max.y - rect.min.y));

            cr << DispatchCompute(Vec3u { (extent.x + GroundAlbedoGroupSize - 1) / GroundAlbedoGroupSize, (extent.y + GroundAlbedoGroupSize - 1) / GroundAlbedoGroupSize, 1 });
        }

        if (canFillUploads)
        {
            albedoLevel.unfilledUploads = GlimmerTexelRect {};
        }

        albedoLevel.hasFilled = true;
        albedoLevel.filledOrigin = windowOrigin;
    }

    m_albedoDirtyWorldRects.Clear();
    m_hasGroundCoverChanged = false;

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

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSurface);

    for (GlimmerTexelRect& uploadedRect : m_uploadedRects)
    {
        uploadedRect = GlimmerTexelRect {};
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
            const Vec3u groups = helpers::WrapComputeGroupCount((numTexels + GroundUploadGroupSize - 1) / GroundUploadGroupSize);

            GlimmerGroundUploadConstants constants {};
            constants.texelMinExtent = Vec4i(upload.texelMin.x, upload.texelMin.y, int32(upload.extent.x), int32(upload.extent.y));
            constants.info = Vec4u(upload.level, uint32(readOffset), groups.x, 0);

            GpuBuffer* cbuffer = nullptr;
            size_t cbufferOffset = 0;
            size_t cbufferSize = 0;

            RI.cbufferAllocator->Write(&constants);
            RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

            cr << SetShaderUniform(0, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
            cr << SetShaderUniform(1, "HeightsBuffer"_sh, heightsBuffer.Get(), ShaderDataOffset(0, sizeof(float)));
            cr << SetShaderUniform(2, "OutGround"_sh, RI.textureViewCache->GetOrCreate(m_ground));

            cr << DispatchCompute(groups);

            readOffset += upload.heights.Size();

            const GlimmerTexelRect uploadRect { upload.texelMin, upload.texelMin + Vec2i(int32(upload.extent.x), int32(upload.extent.y)) };
            m_uploadedRects[upload.level] = GlimmerTexelRect::Union(m_uploadedRects[upload.level], uploadRect);
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
    UpdateGroundCover(frame, state);
    UpdateGroundAlbedo(frame, state);
}

#pragma endregion GlimmerSurfaceCache

} // namespace Hyperion
