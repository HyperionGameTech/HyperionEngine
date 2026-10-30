/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/Bricks/GlimmerBrickReflections.hpp>
#include <Rendering/Glimmer/Bricks/GlimmerBrickWorld.hpp>
#include <Rendering/Glimmer/Bricks/GlimmerBrickCVars.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/GlimmerPass.hpp>
#include <Rendering/Glimmer/GlimmerTechnique.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/GlimmerRelight.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statGlimmerBrickReflections("Rendering/GPU/Glimmer/BrickReflections");

static StaticShaderPropertyId s_propReflectionsModeRequests { ShaderProperty(NAME("MODE"), NAME("REQUESTS")) };
static StaticShaderPropertyId s_propReflectionsModeResolve { ShaderProperty(NAME("MODE"), NAME("RESOLVE")) };
static StaticShaderPropertyId s_propReflectionsModeFinish { ShaderProperty(NAME("MODE"), NAME("FINISH")) };
static StaticShaderPropertyId s_propReflectionsModeRays { ShaderProperty(NAME("MODE"), NAME("RAYS")) };
static StaticShaderPropertyId s_propReflectionsModeHistory { ShaderProperty(NAME("MODE"), NAME("HISTORY")) };
static StaticShaderPropertyId s_propReflectionsModeSpread { ShaderProperty(NAME("MODE"), NAME("SPREAD")) };
static StaticShaderPropertyId s_propReflectionsModeWholeList { ShaderProperty(NAME("MODE"), NAME("WHOLE_LIST")) };
static StaticShaderPropertyId s_propReflectionsModeWholeRays { ShaderProperty(NAME("MODE"), NAME("WHOLE_RAYS")) };

static StaticShaderPropertyId s_propHeightfieldModeHalf { ShaderProperty(NAME("MODE"), NAME("HALF")) };
static StaticShaderPropertyId s_propHeightfieldModeWhole { ShaderProperty(NAME("MODE"), NAME("WHOLE")) };

static constexpr uint32 SlotFrames = 16;
static constexpr uint32 RayFrames = 32;

static constexpr uint32 NumBlocks = 1u << 17;
static constexpr uint32 BlockWords = 10;
static constexpr uint32 NumClaims = 1u << 20;
static constexpr uint32 RequestBudget = 32768;
static constexpr uint32 RequestWords = 4;

// NOTE must match GlimmerBrickReflections.hlsl and GlimmerBrickReflectionsHeightfield.hlsl
static constexpr uint32 NumCounts = 4;
static constexpr uint32 TraceRow = 256;

struct GlimmerBrickReflectionsConstants
{
    GlimmerBrickShaderData bricks;

    /// xy = the view's dimensions
    /// zw = the half resolution picture's dimensions
    Vec4u dimensions;

    /// x = frame
    /// y = frames a pixel that reads slots adds up
    /// z = frames one that traces for itself adds up
    Vec4u frame;

    /// x = crossings
    /// y = blocks
    /// z = requests per frame max
    /// w = 1 when there is no last frame to read.
    Vec4u limits;
};

struct GlimmerBrickReflectionsHeightfieldConstants
{
    GlimmerBrickShaderData bricks;

    /// xy = the view's dimension
    /// zw = the half resolution picture's dimensions
    Vec4u dimensions;

    /// x = the foliage's extinction
    Vec4f params;

    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
    GlimmerRelightShaderData relight;
    GlimmerLightShaderData light;
};

struct ReflectionsPass
{
    const StaticShaderPropertyId* modeProperty = nullptr;
    Vec3u groups;

    bool usesSlots = false;

    Texture* outColor = nullptr;
    Texture* outInfo = nullptr;
    Texture* outGuide = nullptr;

    Texture* color = nullptr;
    Texture* info = nullptr;
    Texture* guide = nullptr;
    Texture* historyColor = nullptr;
    Texture* historyInfo = nullptr;
    Texture* historyGuide = nullptr;
};

#pragma region GlimmerBrickReflections

GlimmerBrickReflections::GlimmerBrickReflections() = default;

GlimmerBrickReflections::~GlimmerBrickReflections()
{
    m_blocksBuffer.Shutdown();
    m_claimsBuffer.Shutdown();
    m_requestsBuffer.Shutdown();
    m_wantedBuffer.Shutdown();
    m_listBuffer.Shutdown();
    m_countsBuffer.Shutdown();
}

const GpuImageViewRef& GlimmerBrickReflections::GetImageView() const
{
    return RI.textureViewCache->GetOrCreate(m_isValid ? m_fullTexture : RI.placeholderData->textureSolidBlack);
}

static RWStructuredBuffer CreateWordBuffer(size_t numWords, Name name)
{
    RWStructuredBuffer buffer = GlimmerHelpers::CreateGlimmerRWBuffer(sizeof(uint32), MathUtil::Max(numWords, size_t(4)));

#ifdef HYP_RHI_DEBUG_NAMES
    buffer.gpuBuffer->SetDebugName(name);
#endif

    return buffer;
}

void GlimmerBrickReflections::CreateTextures(const Vec2u& extent)
{
    const Vec2u halfExtent = Vec2u((extent.x + 1) / 2, (extent.y + 1) / 2);

    const auto createTexture = [](const Vec2u& textureExtent, Name name)
    {
        return GlimmerHelpers::CreateGlimmerStorageTexture(
            TextureType::Texture2D,
            TextureFormat::RGBA16F,
            Vec3u(textureExtent, 1),
            1,
            name,
            TextureWrapMode::ClampToEdge);
    };

    m_rawColorTexture = createTexture(halfExtent, NAME("GlimmerBrickReflectionsRawColor"));
    m_rawInfoTexture = createTexture(halfExtent, NAME("GlimmerBrickReflectionsRawInfo"));

    for (uint32 index = 0; index < 2; index++)
    {
        m_guideTextures[index] = createTexture(halfExtent, NAME("GlimmerBrickReflectionsGuide"));
        m_keptColorTextures[index] = createTexture(halfExtent, NAME("GlimmerBrickReflectionsKeptColor"));
        m_keptInfoTextures[index] = createTexture(halfExtent, NAME("GlimmerBrickReflectionsKeptInfo"));
    }

    m_fullTexture = createTexture(extent, NAME("GlimmerBrickReflections"));
    m_unusedOutputTexture = createTexture(Vec2u(1, 1), NAME("GlimmerBrickReflectionsUnused"));

    m_wantedBuffer.Shutdown();
    m_listBuffer.Shutdown();

    const size_t numHalfPixels = size_t(halfExtent.x) * halfExtent.y;

    m_wantedBuffer = CreateWordBuffer(numHalfPixels * RequestWords, NAME("GlimmerBrickSlotWanted"));
    m_listBuffer = CreateWordBuffer(numHalfPixels, NAME("GlimmerBrickSlotList"));

    m_extent = extent;
    m_hasHistory = false;
}

void GlimmerBrickReflections::CreateSlotBuffers(uint32 numCrossings)
{
    m_blocksBuffer.Shutdown();
    m_claimsBuffer.Shutdown();
    m_requestsBuffer.Shutdown();
    m_countsBuffer.Shutdown();

    m_blocksBuffer = CreateWordBuffer(size_t(NumBlocks) * BlockWords, NAME("GlimmerBrickSlotBlocks"));
    m_claimsBuffer = CreateWordBuffer(NumClaims, NAME("GlimmerBrickSlotClaims"));
    m_requestsBuffer = CreateWordBuffer(size_t(RequestBudget) * RequestWords, NAME("GlimmerBrickSlotRequests"));
    m_countsBuffer = CreateWordBuffer(NumCounts, NAME("GlimmerBrickSlotCounts"));

    m_numCrossings = numCrossings;
    m_hasRequests = false;
}

void GlimmerBrickReflections::Render(Frame* frame, View* view, GBuffer* gbuffer, const Vec2u& extent, const GlimmerScenePassData* scene, EnvProbe* skyProbe)
{
    HYP_SCOPE;

    m_isValid = false;

    const GlimmerTechnique* technique = scene ? scene->technique.Get() : nullptr;
    const GlimmerBrickWorld* brickWorld = technique ? technique->GetBrickWorld() : nullptr;

    if (!brickWorld || !brickWorld->IsReady() || !brickWorld->IsLit()
        || !g_cvGlimmerBricksReflections.Get()
        || !view || !view->GetCamera()
        || extent.Volume() == 0)
    {
        m_hasHistory = false;
        m_hasRequests = false;

        return;
    }

    if (!m_fullTexture.IsValid() || m_extent != extent)
    {
        CreateTextures(extent);
    }

    if (!m_blocksBuffer.IsInitialized() || brickWorld->GetNumCrossings() < m_numCrossings)
    {
        CreateSlotBuffers(brickWorld->GetNumCrossings());
    }
    else
    {
        // the pools grew in place, so every crossing keeps its block
        m_numCrossings = brickWorld->GetNumCrossings();
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickReflections);

    const Vec2u halfExtent = (extent + 1) / 2;

    GlimmerBrickReflectionsConstants constants {};
    constants.bricks = brickWorld->GetShaderData();
    constants.dimensions = Vec4u(extent.x, extent.y, halfExtent.x, halfExtent.y);
    constants.frame = Vec4u(m_frame, SlotFrames, RayFrames, 0u);
    constants.limits = Vec4u(m_numCrossings, NumBlocks, RequestBudget, m_hasHistory ? 0u : 1u);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = frame->cr;

    const GBufferTarget& opaquePass = gbuffer->GetPass(GBufferPass::Opaque);

    GpuImage* gbufferImages[] = {
        opaquePass.GetAttachment(GBufferTarget::Normals)->GetGpuImage(),
        opaquePass.GetAttachment(GBufferTarget::Depth)->GetGpuImage(),
        opaquePass.GetAttachment(GBufferTarget::MatData)->GetGpuImage()
    };

    for (GpuImage* gbufferImage : gbufferImages)
    {
        cr << InsertBarrier(gbufferImage, ResourceState::ShaderResource, ShaderModuleType::Compute);
    }

    const auto bindView = [&](uint32 uniformIndex)
    {
        cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, opaquePass.GetAttachment(GBufferTarget::Normals)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, opaquePass.GetAttachment(GBufferTarget::Depth)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferMaterialTexture"_sh, opaquePass.GetAttachment(GBufferTarget::MatData)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(view->GetCamera()));

        return uniformIndex;
    };

    const Pair<StringHash, const RWStructuredBuffer*> slotBuffers[] = {
        { "SlotBlocksBuffer"_sh, &m_blocksBuffer },
        { "SlotClaimsBuffer"_sh, &m_claimsBuffer },
        { "SlotRequestsBuffer"_sh, &m_requestsBuffer },
        { "SlotWantedBuffer"_sh, &m_wantedBuffer },
        { "SlotListBuffer"_sh, &m_listBuffer },
        { "SlotCountsBuffer"_sh, &m_countsBuffer }
    };

    const auto dispatchPass = [&](const ReflectionsPass& pass)
    {
        ShaderPropertySet shaderProperties;
        shaderProperties.Add(*pass.modeProperty);

        for (Texture* output : { pass.outColor, pass.outInfo, pass.outGuide })
        {
            if (output)
            {
                cr << InsertBarrier(output->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
            }
        }

        for (Texture* input : { pass.color, pass.info, pass.guide, pass.historyColor, pass.historyInfo, pass.historyGuide })
        {
            if (input)
            {
                cr << InsertBarrier(input->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
            }
        }

        if (pass.usesSlots)
        {
            for (const auto& slotBuffer : slotBuffers)
            {
                cr << InsertBarrier(*slotBuffer.second, ResourceState::UnorderedAccess, ShaderModuleType::Compute);
            }
        }

        cr << InsertBarrier(m_unusedOutputTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerBrickReflections"), shaderProperties));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        uniformIndex = bindView(uniformIndex);
        uniformIndex = brickWorld->BindBuffers(cr, uniformIndex, GlimmerBrickBindings::Light);

        for (const auto& slotBuffer : slotBuffers)
        {
            cr << SetShaderUniform(uniformIndex++, slotBuffer.first, *slotBuffer.second);
        }

        const auto setOutput = [&](StringHash name, Texture* texture)
        {
            cr << SetShaderUniform(uniformIndex++, name, RI.textureViewCache->GetOrCreate(texture ? texture : m_unusedOutputTexture.Get()));
        };

        const auto setInput = [&](StringHash name, Texture* texture)
        {
            cr << SetShaderUniform(uniformIndex++, name, RI.textureViewCache->GetOrCreate(texture ? texture : RI.placeholderData->textureSolidBlack.Get()));
        };

        setOutput("OutColor"_sh, pass.outColor);
        setOutput("OutInfo"_sh, pass.outInfo);
        setOutput("OutGuide"_sh, pass.outGuide);
        setInput("ColorTexture"_sh, pass.color);
        setInput("InfoTexture"_sh, pass.info);
        setInput("GuideTexture"_sh, pass.guide);
        setInput("HistoryColorTexture"_sh, pass.historyColor);
        setInput("HistoryInfoTexture"_sh, pass.historyInfo);
        setInput("HistoryGuideTexture"_sh, pass.historyGuide);

        cr << DispatchCompute(pass.groups);
    };

    const uint32 current = m_parity;
    const uint32 previous = 1 - m_parity;

    const Vec3u halfGroups = Vec3u((halfExtent.x + 7) / 8, (halfExtent.y + 7) / 8, 1);
    const Vec3u fullGroups = Vec3u((extent.x + 7) / 8, (extent.y + 7) / 8, 1);

    const auto rowGroups = [](uint32 numThreads)
    {
        return Vec3u(TraceRow / 8, MathUtil::Max((numThreads + TraceRow - 1) / TraceRow + 7, 8u) / 8, 1);
    };

    if (m_hasRequests)
    {
        dispatchPass({ .modeProperty = &s_propReflectionsModeRequests, .groups = rowGroups(RequestBudget), .usesSlots = true });
    }

    dispatchPass({ .modeProperty = &s_propReflectionsModeResolve, .groups = halfGroups, .usesSlots = true, .outColor = m_rawColorTexture.Get(), .outInfo = m_rawInfoTexture.Get(), .outGuide = m_guideTextures[current].Get() });
    dispatchPass({ .modeProperty = &s_propReflectionsModeFinish, .groups = halfGroups, .usesSlots = true, .outInfo = m_rawInfoTexture.Get(), .outGuide = m_guideTextures[current].Get() });
    dispatchPass({ .modeProperty = &s_propReflectionsModeRays, .groups = rowGroups(halfExtent.x * halfExtent.y), .usesSlots = true, .outColor = m_rawColorTexture.Get(), .outInfo = m_rawInfoTexture.Get() });

    GlimmerRelight* relight = technique ? technique->GetRelight() : nullptr;

    const bool tracesHeightfield = technique && technique->IsReady()
        && scene->surfaceCache && scene->spanCache && relight
        && scene->surfaceCache->GetGroundImageView().IsValid()
        && scene->surfaceCache->GetGroundAlbedoImageView().IsValid()
        && scene->spanCache->GetSpansBuffer().IsInitialized()
        && scene->spanCache->GetHeightBoundsBuffer().IsInitialized()
        && relight->GetGpuImage().IsValid();

    // what the bricks left uncovered, from the heightfield
    const auto dispatchHeightfield = [&](const StaticShaderPropertyId& modeProperty, Texture* outColor, Texture* outInfo, const Vec3u& groups)
    {
        GlimmerApplyShaderData applyShaderData {};
        applyShaderData.params = Vec4u(0u, 1u, 0u, 0u);
        applyShaderData.settings = Vec4f(MathUtil::Max(g_cvGlimmerIntensity.Get(), 0.0f), 0.0f, 0.0f, 0.0f);

        GlimmerBrickReflectionsHeightfieldConstants heightfieldConstants {};
        heightfieldConstants.bricks = constants.bricks;
        heightfieldConstants.dimensions = constants.dimensions;
        heightfieldConstants.params = Vec4f(GlimmerHelpers::GetGlimmerFoliageExtinction(), 0.0f, 0.0f, 0.0f);
        heightfieldConstants.ground = scene->surfaceCache->GetGroundShaderData();
        heightfieldConstants.spans = scene->spanCache->GetShaderData();
        heightfieldConstants.relight = relight->GetShaderData();

        GlimmerHelpers::GetGlimmerLightShaderData(skyProbe, heightfieldConstants.light);

        GpuBuffer* heightfieldCbuffer = nullptr;
        size_t heightfieldCbufferOffset = 0;
        size_t heightfieldCbufferSize = 0;

        RI.cbufferAllocator->Write(&applyShaderData);
        GlimmerTechnique::WriteApplyShaderData(*RI.cbufferAllocator, technique);
        RI.cbufferAllocator->Write(&heightfieldConstants);
        RI.cbufferAllocator->Commit(heightfieldCbuffer, heightfieldCbufferOffset, heightfieldCbufferSize);

        ShaderPropertySet shaderProperties;
        shaderProperties.Add(modeProperty);

        cr << InsertBarrier(outColor->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier((outInfo ? outInfo : m_unusedOutputTexture.Get())->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(relight->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
        cr << InsertBarrier(m_wantedBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);
        cr << InsertBarrier(m_countsBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerBrickReflectionsHeightfield"), shaderProperties));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, heightfieldCbuffer, ShaderDataOffset(heightfieldCbufferOffset, heightfieldCbufferSize));

        uniformIndex = bindView(uniformIndex);

        cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, scene->surfaceCache->GetGroundImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, scene->surfaceCache->GetGroundAlbedoImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, scene->spanCache->GetSpansBuffer());
        cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, scene->spanCache->GetHeightBoundsBuffer());
        cr << SetShaderUniform(uniformIndex++, "GlimmerRelightTexture"_sh, relight->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "OutColor"_sh, RI.textureViewCache->GetOrCreate(outColor));
        cr << SetShaderUniform(uniformIndex++, "OutInfo"_sh, RI.textureViewCache->GetOrCreate(outInfo ? outInfo : m_unusedOutputTexture.Get()));
        cr << SetShaderUniform(uniformIndex++, "SlotWholeBuffer"_sh, m_wantedBuffer);
        cr << SetShaderUniform(uniformIndex++, "SlotCountsBuffer"_sh, m_countsBuffer);

        uniformIndex = GlimmerTechnique::BindApplyResources(cr, uniformIndex, technique);

        cr << DispatchCompute(groups);
    };

    if (tracesHeightfield)
    {
        dispatchHeightfield(s_propHeightfieldModeHalf, m_rawColorTexture.Get(), m_rawInfoTexture.Get(), halfGroups);
    }

    dispatchPass({
        .modeProperty = &s_propReflectionsModeHistory,
        .groups = halfGroups,
        .outColor = m_keptColorTextures[current].Get(),
        .outInfo = m_keptInfoTextures[current].Get(),
        .color = m_rawColorTexture.Get(),
        .info = m_rawInfoTexture.Get(),
        .guide = m_guideTextures[current].Get(),
        .historyColor = m_keptColorTextures[previous].Get(),
        .historyInfo = m_keptInfoTextures[previous].Get(),
        .historyGuide = m_guideTextures[previous].Get() });

    dispatchPass({ .modeProperty = &s_propReflectionsModeSpread, .groups = fullGroups, .outColor = m_fullTexture.Get(), .color = m_keptColorTextures[current].Get() });

    const Vec3u wholeGroups = rowGroups(extent.x * extent.y / 4 * 3);

    dispatchPass({ .modeProperty = &s_propReflectionsModeWholeList, .groups = fullGroups, .usesSlots = true, .guide = m_guideTextures[current].Get() });
    dispatchPass({ .modeProperty = &s_propReflectionsModeWholeRays, .groups = wholeGroups, .usesSlots = true, .outColor = m_fullTexture.Get() });

    if (tracesHeightfield)
    {
        dispatchHeightfield(s_propHeightfieldModeWhole, m_fullTexture.Get(), nullptr, wholeGroups);
    }

    cr << InsertBarrier(m_fullTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Pixel);

    for (GpuImage* gbufferImage : gbufferImages)
    {
        cr << InsertBarrier(gbufferImage, ResourceState::ShaderResource, ShaderModuleType::Pixel);
    }

    m_frame++;
    m_parity = previous;
    m_hasHistory = true;
    m_hasRequests = true;
    m_isValid = true;
}

#pragma endregion GlimmerBrickReflections

} // namespace Hyperion
