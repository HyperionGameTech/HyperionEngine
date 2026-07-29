/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/DeferredPass.hpp>
#include <Rendering/Passes/DeferredPassShared.hpp>
#include <Rendering/Passes/EnvProbePass.hpp>
#include <Rendering/Passes/UIPass.hpp>
#include <Rendering/Passes/SpritePass.hpp>
#include <Rendering/Passes/LightingPass.hpp>
#include <Rendering/Passes/TonemapPass.hpp>
#include <Rendering/Passes/LightmapPass.hpp>
#include <Rendering/Passes/FogVolumePass.hpp>
#include <Rendering/Passes/ReflectionsPass.hpp>

#ifdef HYP_EDITOR
#include <Rendering/Passes/EditorGridPass.hpp>
#endif // HYP_EDITOR

#include <Rendering/RenderGroup.hpp>
#include <Rendering/RenderSetup.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/DepthPyramidRenderer.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/Passes/SSRPass.hpp>
#include <Rendering/SSGI.hpp>
#include <Rendering/Passes/HBAOPass.hpp>
#include <Rendering/Passes/BloomPass.hpp>
#include <Rendering/DepthOfField.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/BLASCache.hpp>
#include <Rendering/AccelerationStructure.hpp>
#include <Rendering/BLASBuilder.hpp>
#include <Rendering/RayTracingReflections.hpp>
#include <Rendering/DDGI.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/RawBufferAllocator.hpp>

#include <Rendering/Shadows/ShadowMapAllocator.hpp>
#include <Rendering/Shadows/ShadowMapCache.hpp>
#include <Rendering/Shadows/ShadowMap.hpp>

#include <Rendering/Util/ShaderCompiler.hpp>
#include <Rendering/Util/DeletionQueue.hpp>

#include <Rendering/DebugDrawer.hpp>

#include <Scene/World.hpp>
#include <Scene/View.hpp>
#include <Scene/ProbeVolume.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/FogVolume.hpp>
#include <Scene/ParticleVolume.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Framework/CVarManager.hpp>

#ifdef HYP_EDITOR
#include <Framework/GameState.hpp>
#endif // HYP_EDITOR

#include <Framework/Config/EngineConfig.hpp>

#include <Core/Config/Config.hpp>

#include <Core/FileSystem/FsUtil.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/DeferredScope.hpp>
#include <Core/Utilities/Float16.hpp>

#include <System/AppContext.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineStats.hpp>

#include <DeferredPass.generated.inl>

namespace Hyperion {

const StringHash GBufferTextureNames[NumGBufferTargets] = {
    "GBufferAlbedoTexture"_sh,
    "GBufferNormalsTexture"_sh,
    "GBufferMaterialTexture"_sh,
    "GBufferVelocityTexture"_sh,
    "GBufferDepthTexture"_sh
};

static EngineStatTimer s_statClusterLights("Rendering/ClusterLights");
static EngineStatTimer s_statClusterEnvProbes("Rendering/ClusterEnvProbes");

static EngineStatGpuTimer s_statDeferredPass("Rendering/GPU/DeferredPass");
static EngineStatGpuTimer s_statDepthPrepass("Rendering/GPU/DepthPrepass");
static EngineStatGpuTimer s_statBuildHiZ("Rendering/GPU/BuildHiZ");
static EngineStatGpuTimer s_statFillOpaque("Rendering/GPU/FillOpaque");
static EngineStatGpuTimer s_statFillTranslucent("Rendering/GPU/FillTranslucent");
static EngineStatGpuTimer s_statFillDebug("Rendering/GPU/FillDebug");
static EngineStatGpuTimer s_statOcclusionCulling("Rendering/GPU/OcclusionCulling");

// Global stat counter instances
EngineStatCounter<uint32> g_statDrawCalls("Rendering/DrawCalls");
EngineStatCounter<uint32> g_statInstancedDrawCalls("Rendering/InstancedDrawCalls");
EngineStatCounter<uint32> g_statTriangles("Rendering/Triangles");
EngineStatCounter<uint32> g_statRenderGroups("Rendering/RenderGroups");
EngineStatCounter<uint32> g_statTextures("Rendering/Textures");
EngineStatCounter<uint32> g_statMaterials("Rendering/Materials");
EngineStatCounter<uint32> g_statLights("Rendering/Lights");
EngineStatCounter<uint32> g_statLightmapVolumes("Rendering/LightmapVolumes");
EngineStatCounter<uint32> g_statParticleVolumes("Rendering/ParticleVolumes");
EngineStatCounter<uint32> g_statEnvProbes("Rendering/EnvProbes");
EngineStatCounter<uint32> g_statProbeVolumes("Rendering/ProbeVolumes");
EngineStatCounter<uint32> g_statDebugDraws("Rendering/DebugDraws");

CVar<int> g_cvDeferredDebugVis { "Rendering.Deferred.DebugVis", 0 };

CVar<bool> g_cvRayTracingEnabled { "Rendering.RayTracingEnabled", true };
CVar<bool> g_cvDDGI { "Rendering.DDGI", false };
CVar<bool> g_cvRayTracedReflections { "Rendering.RayTracing.RayTracedReflections", false };
CVar<bool> g_cvPathTracing { "Rendering.PathTracing", false };

CVar<bool> g_cvSSGI { "Rendering.SSGI", true };
CVar<bool> g_cvSSR { "Rendering.SSR", true, "Rendering.SSR.Enabled" };
CVar<bool> g_cvTAA { "Rendering.TAA", true };
CVar<bool> g_cvHBAO { "Rendering.HBAO", true, "Rendering.HBAO.Enabled" };
CVar<bool> g_cvBloom { "Rendering.Bloom", true, "Rendering.Bloom.Enabled" };
CVar<bool> g_cvEnableLightmapVolumes { "Rendering.LightmapVolumes", true };
CVar<bool> g_cvClusteredShading { "Rendering.ClusteredShading", true };
CVar<float> g_cvTonemapExposure { "Rendering.Tonemap.Exposure", 1.8f };
CVar<bool> g_cvDepthPrepass { "Rendering.DepthPrepass", true };
CVar<bool> g_cvFogVolumes { "Rendering.FogVolumes", true };
CVar<bool> g_cvFogVolumesClusteredLights { "Rendering.FogVolumesClusteredLights", true };

#ifdef HYP_EDITOR
CVar<bool> g_cvEditorGrid { "Editor.ShowGrid", true };
#endif // HYP_EDITOR

extern CVar<int> g_cvSkipRendering;

namespace DeferredRendererHelpers {

void FillShadowMapData(
    ShadowMapData& outShadowMapData,
    const ShadowMap& inShadowMap,
    uint32 cascadeIndex,
    View* shadowMapViewDynamic,
    View* shadowMapViewStatic)
{
    ShadowMapAtlasElement* atlasElement = inShadowMap.GetAtlasElement();
    AssertDebug(atlasElement != nullptr);

    if (!atlasElement)
    {
        return;
    }

    RenderProxyList& rpl = GetConsumerProxyList(shadowMapViewDynamic);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    const Mat4f& viewProjMat = rpl.cachedMatrices.viewProj;
    const Mat4f& invProjMat = rpl.cachedMatrices.invProj;

    outShadowMapData = {};

    BoundingBox shadowBoundsNDC;
    shadowBoundsNDC.min = Vec3f(-1.0f);
    shadowBoundsNDC.max = Vec3f(1.0f);

    BoundingBox shadowBoundsWS = viewProjMat.Inverse() * shadowBoundsNDC;

    outShadowMapData.layerIndex = atlasElement->layerIndex;

    outShadowMapData.viewProjMat = viewProjMat;
    outShadowMapData.invProjMat = invProjMat;

    outShadowMapData.aabbMin.x = shadowBoundsWS.min.x;
    outShadowMapData.aabbMin.y = shadowBoundsWS.min.y;
    outShadowMapData.aabbMin.z = shadowBoundsWS.min.z;
    outShadowMapData.aabbMin.w = atlasElement->offsetUV.x;

    outShadowMapData.aabbMax.x = shadowBoundsWS.max.x;
    outShadowMapData.aabbMax.y = shadowBoundsWS.max.y;
    outShadowMapData.aabbMax.z = shadowBoundsWS.max.z;
    outShadowMapData.aabbMax.w = atlasElement->offsetUV.y;

    outShadowMapData.dimensionsScale = Vec4f(Vec2f(atlasElement->dimensions), atlasElement->scale);

    outShadowMapData.splitDistance = 0.0f; // @TODO
}

void FillShadowMapDataCSM(
    DirectionalLightCSMData* outCSMData,
    View** shadowMapViews,
    ShadowMap** shadowMaps,
    uint32 numCascades)
{
    for (uint32 cascadeIndex = 0; cascadeIndex < numCascades; cascadeIndex++)
    {
        View* shadowMapView = shadowMapViews[cascadeIndex];

        if (!shadowMapView)
        {
            break;
        }

        RenderProxyList& shadowViewRpl = GetConsumerProxyList(shadowMapView);
        shadowViewRpl.BeginRead();

        HYP_DEFER({ shadowViewRpl.EndRead(); });

        if (cascadeIndex == 0)
        {
            // Shared view matrix for all cascades
            outCSMData->shadowViewMat = shadowViewRpl.cachedMatrices.view;
        }

        ShadowMap* shadowMap = shadowMaps[cascadeIndex];
        AssertDebug(shadowMap != nullptr);

        const Vec2f& atlasScale = shadowMap->GetAtlasElement()->scale;
        const Vec2f& atlasOffset = shadowMap->GetAtlasElement()->offsetUV;
        const uint32 layerIndex = shadowMap->GetAtlasElement()->layerIndex;

        const BoundingBox& cascadeBounds = shadowViewRpl.cachedBounds;

        const float rawScaleX = 1.0f / (cascadeBounds.max.x - cascadeBounds.min.x);
        const float rawScaleY = -1.0f / (cascadeBounds.max.y - cascadeBounds.min.y);
        const float rawScaleZ = 1.0f / (cascadeBounds.max.z - cascadeBounds.min.z);

        const float rawOffsetX = -cascadeBounds.min.x * rawScaleX;
        const float rawOffsetY = -cascadeBounds.max.y * rawScaleY;
        const float rawOffsetZ = -cascadeBounds.min.z * rawScaleZ;

        outCSMData->atlasSlice[cascadeIndex] = layerIndex;

        outCSMData->atlasU[cascadeIndex] = atlasOffset.x;
        outCSMData->atlasV[cascadeIndex] = atlasOffset.y;

        outCSMData->atlasScaleX[cascadeIndex] = atlasScale.x;
        outCSMData->atlasScaleY[cascadeIndex] = atlasScale.y;

        outCSMData->cascadeScaleX[cascadeIndex] = rawScaleX;
        outCSMData->cascadeScaleY[cascadeIndex] = rawScaleY;
        outCSMData->cascadeScaleZ[cascadeIndex] = rawScaleZ;

        outCSMData->cascadeOffsetX[cascadeIndex] = rawOffsetX;
        outCSMData->cascadeOffsetY[cascadeIndex] = rawOffsetY;
        outCSMData->cascadeOffsetZ[cascadeIndex] = rawOffsetZ;
    }
}

} // namespace DeferredRendererHelpers

#pragma region DeferredPassData

DeferredPassData::~DeferredPassData()
{
    for (FramebufferRef& framebuffer : mipChainFramebuffers)
    {
        EnqueueDeletion(std::move(framebuffer));
    }

    EnqueueDeletion(std::move(mipChain));

    depthPyramidRenderer.Reset();

    hbao.Reset();

    taaPass.Reset();

    // m_dofBlur->Destroy();

    ssgi.Reset();

    bloomPass.Reset();

    postProcessing->Destroy();
    postProcessing.Reset();

    combinePass.Reset();

    reflectionsPass.Reset();

    lightmapPass.Reset();
    tonemapPass.Reset();
    mipChain.Reset();
    indirectLightingPass.Reset();
    directLightingPass.Reset();

    rayTracingReflections.Reset();
    ddgi.Reset();
}

#pragma endregion DeferredPassData

#pragma region RayTracingPassData

RayTracingPassData::~RayTracingPassData()
{
    EnqueueDeletion(std::move(rayTracingTlases));
}

#pragma endregion RayTracingPassData

#pragma region DeferredPass

static FramebufferRef CreateLightingFramebuffer(GBuffer* gbuffer)
{
    FramebufferDesc framebufferDesc {};
    framebufferDesc.extent = gbuffer->GetExtent();

    FramebufferRef framebuffer = RI.MakeFramebuffer(framebufferDesc);
#ifdef HYP_RHI_DEBUG_NAMES
    framebuffer->SetDebugName(NAME("LightingFramebuffer"));
#endif

    AttachmentDesc colorAttachmentDesc {};
    colorAttachmentDesc.imageType = TextureType::Texture2D;
    colorAttachmentDesc.format = TextureFormat::RGBA16F;
    colorAttachmentDesc.loadOp = LoadOperation::CLEAR;
    colorAttachmentDesc.storeOp = StoreOperation::STORE;

    Attachment* colorAttachment = framebuffer->AddAttachment(0, colorAttachmentDesc);

    // depth for stencil testing
    const GpuImageViewRef& depthImageView = gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Depth)->GetImageView();
    Assert(depthImageView.IsValid());

    AttachmentDesc depthAttachmentDesc {};
    depthAttachmentDesc.imageType = TextureType::Texture2D;
    depthAttachmentDesc.format = depthImageView->GetImage()->GetTextureFormat();
    depthAttachmentDesc.loadOp = LoadOperation::LOAD;
    depthAttachmentDesc.storeOp = StoreOperation::NONE;
    depthAttachmentDesc.onlyStencil = true;

    Attachment* depthAttachment = framebuffer->AddAttachment(
        1,
        depthAttachmentDesc,
        depthImageView);

    Check(framebuffer->Create());

#ifdef HYP_RHI_DEBUG_NAMES
    colorAttachment->GetGpuImage()->SetDebugName(NAME("DeferredShadingTarget_Color"));
#endif

    return framebuffer;
}

static FramebufferRef CreateDepthPrepassFramebuffer(GBuffer* gbuffer)
{
    FramebufferDesc framebufferDesc {};
    framebufferDesc.extent = gbuffer->GetExtent();

    FramebufferRef framebuffer = RI.MakeFramebuffer(framebufferDesc);
#ifdef HYP_RHI_DEBUG_NAMES
    framebuffer->SetDebugName(NAME("DepthPrepassFramebuffer"));
#endif

    const GpuImageViewRef& depthImageView = gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Depth)->GetImageView();
    Assert(depthImageView.IsValid());

    AttachmentDesc depthAttachmentDesc {};
    depthAttachmentDesc.imageType = TextureType::Texture2D;
    depthAttachmentDesc.format = depthImageView->GetImage()->GetTextureFormat();
    depthAttachmentDesc.loadOp = LoadOperation::CLEAR;
    depthAttachmentDesc.storeOp = StoreOperation::STORE;

    Attachment* depthAttachment = framebuffer->AddAttachment(
        0,
        depthAttachmentDesc,
        depthImageView);

    Check(framebuffer->Create());

#ifdef HYP_RHI_DEBUG_NAMES
    depthAttachment->GetGpuImage()->SetDebugName(NAME("DepthPrepassAttachment"));
#endif

    return framebuffer;
}

class TileProcessor
{
public:
    static constexpr uint32 MaxEnvProbesPerTile = 8;
    static constexpr uint32 MaxLightsPerTile = 16;

    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    struct TileGridData
    {
        uint32 indexOffset;
        uint16 numLights;
        uint16 numEnvProbes;
    };

    struct Tile
    {
        uint16 numEnvProbes;
        uint16 numLights;
        uint16 envProbeIndices[MaxEnvProbesPerTile];
        uint16 lightIndices[MaxLightsPerTile];
    };

    struct TileDataAllocation
    {
        size_t gridBufferSize = 0;
        size_t indexBufferSize = 0;
        uint32 lastUsedFrame = UINT32_MAX;

        Array<Tile, RenderAllocator> tempTiles;

        Array<TileGridData, RenderAllocator> gridData;
        Array<uint16, RenderAllocator> indexData;
    };

    Array<TileDataAllocation, RenderAllocator> tileDataPerView;

    TileProcessor() = default;

    TileProcessor(const TileProcessor& other) = delete;
    TileProcessor& operator=(const TileProcessor& other) = delete;

    ~TileProcessor() = default;

    void ProcessView(const Viewport& viewport, View* view, ByteAddressBuffer*& outGridBuffer, ByteAddressBuffer*& outIndexBuffer)
    {
        Assert(view != nullptr);

        outGridBuffer = nullptr;
        outIndexBuffer = nullptr;

        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(view->GetCamera()));
        if (!cameraProxy)
        {
            return;
        }

        // @TODO VP offset
        const Vec2u extent = cameraProxy->bufferData.dimensions.GetXY();

        const uint32 numTilesX = (extent.x + TileSize - 1) / TileSize;
        const uint32 numTilesY = (extent.y + TileSize - 1) / TileSize;
        const uint32 totalTiles = numTilesX * numTilesY * TileZBins;

        Assert(totalTiles != 0);

        if (tileDataPerView.Size() <= view->Id().ToIndex())
        {
            tileDataPerView.Resize(view->Id().ToIndex() + 1);
        }

        TileDataAllocation& allocation = tileDataPerView[view->Id().ToIndex()];
        allocation.lastUsedFrame = GetFrameCounter();

        Array<Tile, RenderAllocator>& tempTiles = allocation.tempTiles;
        if (tempTiles.Size() < totalTiles)
        {
            tempTiles.ResizeZeroed(totalTiles);
        }
        else
        {
            Memory::Zero(tempTiles.Data(), totalTiles * sizeof(Tile));
        }

        RenderProxyList& rpl = GetConsumerProxyList(view);
        rpl.BeginRead();
        HYP_DEFER({ rpl.EndRead(); });

        const float cameraNear = cameraProxy->bufferData.cameraNear;
        const float cameraFar = cameraProxy->bufferData.cameraFar;

        const float logFarOverNear = std::log2(cameraFar / cameraNear);

        const float scale = float(TileZBins) / logFarOverNear;
        const float bias = -(float(TileZBins) * std::log2(cameraNear)) / logFarOverNear;

        const Mat4f& viewMatrix = cameraProxy->bufferData.viewMat;

        Mat4f projMatrix = cameraProxy->bufferData.projMat;

        auto calculateZBin = [scale, bias](float viewSpaceZ) -> int32
        {
            const float z = MathUtil::Max(viewSpaceZ, 0.0001f);
            const int32 zBin = int32(std::log2(z) * scale + bias);

            return MathUtil::Clamp(zBin, 0, int32(TileZBins) - 1);
        };

        auto projectSphereToScreenAABB = [&projMatrix, &extent, cameraNear, numTilesX, numTilesY](
            const Vec3f& centerVS,
            float radius,
            uint32& outMinX, uint32& outMinY,
            uint32& outMaxX, uint32& outMaxY) -> bool
        {
            const float dist = centerVS.z;

            if (dist + radius < cameraNear)
            {
                return false;
            }

            const float projScaleX = projMatrix[0][0];
            const float projScaleY = projMatrix[1][1];

            const float effectiveZ = MathUtil::Max(dist, cameraNear);
            const float invZ = 1.0f / effectiveZ;

            const float ndcCenterX = centerVS.x * projScaleX * invZ;
            const float ndcCenterY = centerVS.y * projScaleY * invZ;

            const float nearestZ = MathUtil::Max(dist - radius, cameraNear);
            const float invNearestZ = 1.0f / nearestZ;
            const float ndcRadiusX = radius * std::abs(projScaleX) * invNearestZ;
            const float ndcRadiusY = radius * std::abs(projScaleY) * invNearestZ;

            const float halfW = float(extent.x) * 0.5f;
            const float halfH = float(extent.y) * 0.5f;

            const float pixMinX = (ndcCenterX - ndcRadiusX) * halfW + halfW;
            const float pixMaxX = (ndcCenterX + ndcRadiusX) * halfW + halfW;
            const float pixMinY = (1.0f - (ndcCenterY + ndcRadiusY)) * halfH;
            const float pixMaxY = (1.0f - (ndcCenterY - ndcRadiusY)) * halfH;

            const int32 minX = MathUtil::Max(int32(pixMinX) / int32(TileSize), 0);
            const int32 minY = MathUtil::Max(int32(pixMinY) / int32(TileSize), 0);
            const int32 maxX = MathUtil::Min(int32(pixMaxX) / int32(TileSize), int32(numTilesX - 1));
            const int32 maxY = MathUtil::Min(int32(pixMaxY) / int32(TileSize), int32(numTilesY - 1));

            if (minX > maxX || minY > maxY)
            {
                return false;
            }

            outMinX = uint32(minX);
            outMinY = uint32(minY);
            outMaxX = uint32(maxX);
            outMaxY = uint32(maxY);

            return true;
        };

        auto projectAABBToScreenTiles = [&viewMatrix, &projMatrix, &extent, cameraNear, numTilesX, numTilesY](
            const Vec3f& aabbMinWS, const Vec3f& aabbMaxWS,
            uint32& outMinX, uint32& outMinY,
            uint32& outMaxX, uint32& outMaxY,
            float& outMinVSZ, float& outMaxVSZ) -> bool
        {
            const Vec3f corners[8] = {
                { aabbMinWS.x, aabbMinWS.y, aabbMinWS.z },
                { aabbMaxWS.x, aabbMinWS.y, aabbMinWS.z },
                { aabbMinWS.x, aabbMaxWS.y, aabbMinWS.z },
                { aabbMaxWS.x, aabbMaxWS.y, aabbMinWS.z },
                { aabbMinWS.x, aabbMinWS.y, aabbMaxWS.z },
                { aabbMaxWS.x, aabbMinWS.y, aabbMaxWS.z },
                { aabbMinWS.x, aabbMaxWS.y, aabbMaxWS.z },
                { aabbMaxWS.x, aabbMaxWS.y, aabbMaxWS.z },
            };

            const float projScaleX = projMatrix[0][0];
            const float projScaleY = projMatrix[1][1];

            const float halfW = float(extent.x) * 0.5f;
            const float halfH = float(extent.y) * 0.5f;

            float ndcMinX = MathUtil::MaxSafeValue<float>();
            float ndcMinY = MathUtil::MaxSafeValue<float>();
            float ndcMaxX = MathUtil::MinSafeValue<float>();
            float ndcMaxY = MathUtil::MinSafeValue<float>();

            outMinVSZ = MathUtil::MaxSafeValue<float>();
            outMaxVSZ = MathUtil::MinSafeValue<float>();

            bool anyInFront = false;
            bool anyBehind = false;

            for (const Vec3f& corner : corners)
            {
                const Vec3f cornerVS = viewMatrix.TransformVector(corner);

                outMinVSZ = MathUtil::Min(outMinVSZ, cornerVS.z);
                outMaxVSZ = MathUtil::Max(outMaxVSZ, cornerVS.z);

                if (cornerVS.z < cameraNear)
                {
                    anyBehind = true;
                    continue;
                }

                anyInFront = true;

                const float invZ = 1.0f / cornerVS.z;
                const float ndcX = cornerVS.x * projScaleX * invZ;
                const float ndcY = cornerVS.y * projScaleY * invZ;

                ndcMinX = MathUtil::Min(ndcMinX, ndcX);
                ndcMinY = MathUtil::Min(ndcMinY, ndcY);
                ndcMaxX = MathUtil::Max(ndcMaxX, ndcX);
                ndcMaxY = MathUtil::Max(ndcMaxY, ndcY);
            }

            if (!anyInFront)
            {
                return false;
            }

            // If the AABB straddles the near plane, conservatively cover the full screen
            if (anyBehind)
            {
                ndcMinX = -1.0f;
                ndcMinY = -1.0f;
                ndcMaxX = 1.0f;
                ndcMaxY = 1.0f;
            }

            const float pixMinX = ndcMinX * halfW + halfW;
            const float pixMaxX = ndcMaxX * halfW + halfW;
            const float pixMinY = (1.0f - ndcMaxY) * halfH;
            const float pixMaxY = (1.0f - ndcMinY) * halfH;

            const int32 minX = MathUtil::Max(int32(pixMinX) / int32(TileSize), 0);
            const int32 minY = MathUtil::Max(int32(pixMinY) / int32(TileSize), 0);
            const int32 maxX = MathUtil::Min(int32(pixMaxX) / int32(TileSize), int32(numTilesX - 1));
            const int32 maxY = MathUtil::Min(int32(pixMaxY) / int32(TileSize), int32(numTilesY - 1));

            if (minX > maxX || minY > maxY)
            {
                return false;
            }

            outMinX = uint32(minX);
            outMinY = uint32(minY);
            outMaxX = uint32(maxX);
            outMaxY = uint32(maxY);

            return true;
        };

        {
            ENGINE_STAT_SCOPE(&s_statClusterLights);
            for (Light* light : rpl.GetLights())
            {
                const LightType lightType = light->GetLightType();

                if (!DeferredRendererHelpers::CanClusterLight(lightType))
                {
                    continue;
                }

                const uint32 lightBindingIndex = Resources::GetBinding(light);

                if (lightBindingIndex == ~0u)
                {
                    continue;
                }

                RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
                AssertDebug(lightProxy != nullptr);

                const Vec3f lightPosWS = lightProxy->bufferData.positionIntensity.GetXYZ();
                const Vec3f lightPosVS = viewMatrix.TransformVector(lightPosWS);
                const float lightRadius = float(Float16::FromRaw(uint16(lightProxy->bufferData.radiusFalloffPacked & 0xFFFFu)));

                uint32 tileMinX;
                uint32 tileMinY;
                uint32 tileMaxX;
                uint32 tileMaxY;

                if (!projectSphereToScreenAABB(lightPosVS, lightRadius, tileMinX, tileMinY, tileMaxX, tileMaxY))
                {
                    continue;
                }

                const float lightDistVS = lightPosVS.z;
                const int32 zBinMin = calculateZBin(MathUtil::Max(lightDistVS - lightRadius, cameraNear));
                const int32 zBinMax = calculateZBin(MathUtil::Min(lightDistVS + lightRadius, cameraFar));

                for (int32 z = zBinMin; z <= zBinMax; z++)
                {
                    for (uint32 y = tileMinY; y <= tileMaxY; y++)
                    {
                        for (uint32 x = tileMinX; x <= tileMaxX; x++)
                        {
                            const uint32 clusterIndex = (uint32(z) * numTilesY + y) * numTilesX + x;

                            // yikes.. What?
                            AssertDebug(tempTiles.Size() >= clusterIndex);

                            Tile& tile = tempTiles[clusterIndex];

                            if (tile.numLights < MaxLightsPerTile)
                            {
                                tile.lightIndices[tile.numLights++] = uint16(lightBindingIndex);
                            }
                        }
                    }
                }
            }
        }

        // Start env probes
        ENGINE_STAT_SCOPE(&s_statClusterEnvProbes);

        Array<Tuple<EnvProbe*, EnvProbeShaderData*, uint32>, RenderTempAllocator> envProbes;
        envProbes.Reserve(rpl.GetEnvProbes().NumCurrent());

        for (EnvProbe* envProbe : rpl.GetEnvProbes())
        {
            const uint32 envProbeBindingIndex = Resources::GetBinding(envProbe);

            if (envProbeBindingIndex == ~0u)
            {
                continue;
            }

            RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(envProbe));
            AssertDebug(envProbeProxy != nullptr);

            envProbes.EmplaceBack(envProbe, &envProbeProxy->bufferData, envProbeBindingIndex);
        }

        Vec3f cameraPosition = cameraProxy->bufferData.cameraPosition.GetXYZ();

        // Sort env probes in reverse order
        // They are applied with a reverse loop -- sky is always first in the array if present.
        std::sort(envProbes.Begin(), envProbes.End(),
                  [&cameraPosition](const Tuple<EnvProbe*, EnvProbeShaderData*, uint32>& a, const Tuple<EnvProbe*, EnvProbeShaderData*, uint32>& b)
                  {
                      const auto& aData = *a.GetElement<1>();
                      const auto& bData = *b.GetElement<1>();

                      const bool aIsSky = (aData.typeAndFlags & 0x7) == EPT_SKY;
                      const bool bIsSky = (bData.typeAndFlags & 0x7) == EPT_SKY;

                      if (aIsSky && !bIsSky)
                      {
                          return true;
                      }

                      if (!aIsSky && bIsSky)
                      {
                          return false;
                      }

                      if (aIsSky && bIsSky)
                      {
                          return true;
                      }

                      // both are reflection probes, sort by distance to camera
                      const Vec3f aProbePosition = aData.worldPosition.GetXYZ();
                      const Vec3f bProbePosition = bData.worldPosition.GetXYZ();

                      const float aDistSq = (aProbePosition - cameraPosition).LengthSquared();
                      const float bDistSq = (bProbePosition - cameraPosition).LengthSquared();

                      return aDistSq >= bDistSq;
                  });

        for (size_t envProbeIndex = 0; envProbeIndex < envProbes.Size(); envProbeIndex++)
        {
            const Tuple<EnvProbe*, EnvProbeShaderData*, uint32>& tup = envProbes[envProbeIndex];

            const EnvProbe& envProbe = *tup.GetElement<0>();
            const EnvProbeShaderData& envProbeData = *tup.GetElement<1>();
            const uint32 envProbeBindingIndex = tup.GetElement<2>();

            const Vec3f aabbMinWS = envProbeData.aabbMin.GetXYZ();
            const Vec3f aabbMaxWS = envProbeData.aabbMax.GetXYZ();

            switch (envProbeData.typeAndFlags & 0x7)
            {
            case EPT_SKY:
                for (Tile& tile : tempTiles)
                {
                    if (tile.numEnvProbes < MaxEnvProbesPerTile)
                    {
                        tile.envProbeIndices[tile.numEnvProbes++] = uint16(envProbeBindingIndex);
                    }
                }

                break;
            case EPT_AMBIENT:
                AssertDebug(envProbe.IsA<IrradianceProbe>());
                if (StaticCast<IrradianceProbe>(&envProbe)->IsAttachedToProbeVolume())
                {
                    // Skip irradiance probes that are handled by ProbeVolumes
                    // Entities affected by them have their spherical harmonics data calculated on CPU
                    continue;
                }

                break;
            default:
                break;
            }

            uint32 tileMinX;
            uint32 tileMinY;
            uint32 tileMaxX;
            uint32 tileMaxY;
            float probeVSZMin;
            float probeVSZMax;

            if (!projectAABBToScreenTiles(aabbMinWS, aabbMaxWS, tileMinX, tileMinY, tileMaxX, tileMaxY, probeVSZMin, probeVSZMax))
            {
                continue;
            }

            const int32 zBinMin = calculateZBin(MathUtil::Max(probeVSZMin, cameraNear));
            const int32 zBinMax = calculateZBin(MathUtil::Min(probeVSZMax, cameraFar));

            for (int32 z = zBinMin; z <= zBinMax; z++)
            {
                for (uint32 y = tileMinY; y <= tileMaxY; y++)
                {
                    for (uint32 x = tileMinX; x <= tileMaxX; x++)
                    {
                        const uint32 clusterIndex = (uint32(z) * numTilesY + y) * numTilesX + x;

                        AssertDebug(tempTiles.Size() >= clusterIndex);

                        Tile& tile = tempTiles[clusterIndex];

                        if (tile.numEnvProbes < MaxEnvProbesPerTile)
                        {
                            tile.envProbeIndices[tile.numEnvProbes++] = uint16(envProbeBindingIndex);
                        }
                    }
                }
            }
        }

        // Continue timing lights (env probes already going)
        ENGINE_STAT_SCOPE(&s_statClusterLights);

        Array<TileGridData, RenderAllocator>& gridData = allocation.gridData;
        gridData.Resize(totalTiles);

        Array<uint16, RenderAllocator>& flatIndexData = allocation.indexData;
        flatIndexData.Resize(0);
        flatIndexData.Reserve(totalTiles * 4);

        uint32 offset = 0;

        for (uint32 i = 0; i < totalTiles; ++i)
        {
            const Tile& tile = tempTiles[i];

            gridData[i].indexOffset = offset;
            gridData[i].numLights = tile.numLights;
            gridData[i].numEnvProbes = tile.numEnvProbes;

            const size_t minRequiredSize = offset + tile.numLights + tile.numEnvProbes;

            if (flatIndexData.Size() < minRequiredSize)
            {
                flatIndexData.Resize(minRequiredSize);
            }

            for (uint16 j = 0; j < tile.numLights; j++)
            {
                flatIndexData[offset + j] = tile.lightIndices[j];
            }

            offset += tile.numLights;

            for (uint16 j = 0; j < tile.numEnvProbes; j++)
            {
                flatIndexData[offset + j] = tile.envProbeIndices[j];
            }

            offset += tile.numEnvProbes;
        }

        if (flatIndexData.Empty())
        {
            flatIndexData.Resize(2);
        }
        else if (flatIndexData.Size() % 2 != 0)
        {
            flatIndexData.PushBack(0); // Align to 4 bytes for ByteAddressBuffer
        }

        ByteAddressBuffer& gridBuffer = RI.bufferAllocator->AcquireByteAddressBuffer(gridData.Size() * sizeof(TileGridData));
#ifdef HYP_RHI_DEBUG_NAMES
        if (!gridBuffer.gpuBuffer->GetDebugName().IsValid())
        {
            gridBuffer.gpuBuffer->SetDebugName(NAME("ClusterGridBuffer"));
        }
#endif

        ByteAddressBuffer& indexBuffer = RI.bufferAllocator->AcquireByteAddressBuffer(flatIndexData.Size() * sizeof(uint16));
#ifdef HYP_RHI_DEBUG_NAMES
        if (!indexBuffer.gpuBuffer->GetDebugName().IsValid())
        {
            indexBuffer.gpuBuffer->SetDebugName(NAME("ClusterIndexBuffer"));
        }
#endif

        allocation.gridBufferSize = gridBuffer.gpuBuffer->Size();
        allocation.indexBufferSize = indexBuffer.gpuBuffer->Size();

        gridBuffer.Write(0, gridData.Size() * sizeof(TileGridData), gridData.Data());
        gridBuffer.FlushBatched();

        indexBuffer.Write(0, flatIndexData.Size() * sizeof(uint16), flatIndexData.Data());
        indexBuffer.FlushBatched();

        outGridBuffer = &gridBuffer;
        outIndexBuffer = &indexBuffer;
    }
};

DeferredPass::DeferredPass()
    : m_tileProcessor(MakeUnique<TileProcessor>())
{
}

DeferredPass::~DeferredPass()
{
}

void DeferredPass::Initialize()
{
}

void DeferredPass::Shutdown()
{
    m_quadMesh.Reset();
}

PassData* DeferredPass::CreateViewPassData(View* view, PassDataExt&)
{
    Assert(view != nullptr);

    if (view->GetFlags() & ViewFlags::GBUFFER)
    {
        DeferredPassData* pd = new DeferredPassData();
        DeferredPassData& passData = *pd;

        passData.view = MakeWeakRef(view);

        GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
        Assert(gbuffer != nullptr);

        gbuffer->Create();

        AssertDebug(gbuffer->IsCreated());

        HYP_LOG(Rendering, Verbose, "Creating renderer for view '{}' with GBuffer '{}'", view->Id(), gbuffer->GetExtent());

        Framebuffer* opaquePassFramebuffer = view->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);

        passData.ssgi = MakeUnique<SSGI>(gbuffer);
        passData.ssgi->Create();

        passData.bloomPass = MakeUnique<BloomPass>(gbuffer->GetExtent(), gbuffer);
        passData.bloomPass->Create();

        passData.postProcessing = MakeUnique<PostProcessing>();
        passData.postProcessing->Create();

        passData.lightingFramebuffer = CreateLightingFramebuffer(gbuffer);
        passData.depthPrepassFramebuffer = CreateDepthPrepassFramebuffer(gbuffer);

        passData.indirectLightingPass = MakeUnique<LightingPass>(DPM_INDIRECT_LIGHTING, gbuffer->GetExtent(), gbuffer, passData.lightingFramebuffer);
        passData.indirectLightingPass->Create();

        passData.directLightingPass = MakeUnique<LightingPass>(DPM_DIRECT_LIGHTING, gbuffer->GetExtent(), gbuffer, passData.lightingFramebuffer);
        passData.directLightingPass->Create();

        passData.depthPyramidRenderer = MakeUnique<DepthPyramidRenderer>(gbuffer);
        passData.depthPyramidRenderer->Create();

        passData.mipChain = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            opaquePassFramebuffer->GetAttachment(0)->GetFormat(),
            Vec3u(opaquePassFramebuffer->GetExtent(), 1),
            TFM_LINEAR_MIPMAP,
            TFM_LINEAR_MIPMAP,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_ATTACHMENT });

        passData.mipChain->SetName(NAME("DeferredPassMipChain"));
        Check(passData.mipChain->Create());

        // Create framebuffers for each mip level (for downsampling)
        {
            const uint32 numMips = passData.mipChain->GetTextureDesc().NumMips();
            passData.mipChainFramebuffers.Resize(numMips);

            for (uint32 mipLevel = 0; mipLevel < numMips; ++mipLevel)
            {
                const Vec3u& baseExtent = passData.mipChain->GetTextureDesc().extent;
                Vec2u mipExtent {
                    MathUtil::Max(baseExtent.x >> mipLevel, 1u),
                    MathUtil::Max(baseExtent.y >> mipLevel, 1u)
                };

                FramebufferDesc framebufferDesc {};
                framebufferDesc.extent = mipExtent;

                passData.mipChainFramebuffers[mipLevel] = RI.MakeFramebuffer(framebufferDesc);
#ifdef HYP_RHI_DEBUG_NAMES
                passData.mipChainFramebuffers[mipLevel]->SetDebugName(NAME_FMT("DeferredPassMipChain_FB{}", mipLevel));
#endif

                GpuImageViewRef mipImageView = RI.MakeImageView(
                    passData.mipChain->GetGpuImage(),
                    uint8(mipLevel),
                    1, // mip level, 1 mip
                    0,
                    1 // layer 0, 1 layer
                );

                passData.mipChainFramebuffers[mipLevel]->AddAttachment(
                    0,
                    AttachmentDesc {
                        TextureType::Texture2D,
                        passData.mipChain->GetTextureDesc().format,
                        LoadOperation::CLEAR,
                        StoreOperation::STORE },
                    mipImageView);

                Check(passData.mipChainFramebuffers[mipLevel]->Create());
            }
        }

        passData.hbao = MakeUnique<HBAO>(gbuffer->GetExtent(), gbuffer);
        passData.hbao->Create();

        // m_dofBlur = MakeUnique<DOFBlur>(gbuffer->GetResolution(), gbuffer);
        // m_dofBlur->Create();

        passData.reflectionsPass = MakeUnique<ReflectionsPass>(gbuffer->GetExtent(), gbuffer);
        passData.reflectionsPass->Create();

        passData.tonemapPass = MakeUnique<TonemapPass>(gbuffer->GetExtent(), gbuffer);
        passData.tonemapPass->Create();

        // We'll render the lightmap pass into the translucent framebuffer after deferred shading has been applied to OPAQUE objects.
        passData.lightmapPass = MakeUnique<LightmapPass>();
        passData.lightmapPass->Create();

        passData.fogVolumePass = MakeUnique<FogVolumePass>(gbuffer->GetExtent(), gbuffer);
        passData.fogVolumePass->Create();

#ifdef HYP_EDITOR
        passData.editorGridPass = MakeUnique<EditorGridPass>();
        passData.editorGridPass->Create();
#endif // HYP_EDITOR

        passData.taaPass = MakeUnique<TAAPass>(passData.tonemapPass->GetFinalImageView(), gbuffer->GetExtent(), gbuffer);
        passData.taaPass->Create();

        CreateViewRayTracingPasses(view, passData);

        return pd;
    }
    else if ((view->GetFlags() & ViewFlags::RAY_TRACING) && RI.GetRenderConfig().rayTracing)
    {
        RayTracingPassData* pd = new RayTracingPassData();
        RayTracingPassData& passData = *pd;

        passData.view = MakeWeakRef(view);

        return pd;
    }

    HYP_LOG(Rendering, Fatal, "Cannot create PassData for View {}! View does not have any flags set that would allow us to create PassData for it. View flags: {}", view->Id(), uint32(view->GetFlags()));

    return nullptr;
}

void DeferredPass::CreateViewRayTracingPasses(View* view, DeferredPassData& passData)
{
    AssertOnThread(g_renderThread);

    if (!RI.GetRenderConfig().rayTracing)
    {
        return;
    }

    const bool shouldEnableRayTracingForView = view->GetRayTracingView().IsValid()
        && g_cvRayTracingEnabled.Get();

    if (!shouldEnableRayTracingForView)
    {
        passData.rayTracingReflections.Reset();
        passData.ddgi.Reset();

        return;
    }

    GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
    AssertDebug(gbuffer != nullptr);

    passData.rayTracingReflections = MakeUnique<RayTracingReflections>(gbuffer);
    passData.rayTracingReflections->Create();

    /// FIXME: Proper AABB for DDGI
    passData.ddgi = MakeUnique<DDGI>(DDGIInfo { .aabb = { { -30.0f, -5.0f, -30.0f }, { 30.0f, 35.0f, 30.0f } } });
    passData.ddgi->Create();
}

void DeferredPass::CreateViewTopLevelAccelerationStructures(View* view, RayTracingPassData& passData)
{
    EnqueueDeletion(std::move(passData.rayTracingTlases));

    // Hack to fix driver crash when building TLAS with no meshes
    Handle<Mesh> defaultMesh = MeshBuilder::Cube(true);
    defaultMesh->SetIsTransient(true);
    defaultMesh->SetFlags(MeshFlags::ViewIndependent);
    defaultMesh->UploadGpuData();

    BottomLevelASRef blas = BLASBuilder::Build(defaultMesh);
    Check(blas->Create());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        TopLevelASRef& tlas = passData.rayTracingTlases[frameIndex];

        tlas = RI.MakeTLAS();
        tlas->AddBLAS(0, blas);

        Check(tlas->Create());
    }
}

void DeferredPass::ResizeView(Viewport viewport, View* view, DeferredPassData& passData)
{
    AssertOnThread(g_renderThread);

    if (viewport.extent.Volume() == 0)
    {
        return;
    }

    HYP_LOG(Rendering, Verbose, "Resizing View '{}' to {}x{}", view->Id(), viewport.extent.x, viewport.extent.y);

    const Vec2u newSize = Vec2u(viewport.extent);

    GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
    Assert(gbuffer != nullptr && gbuffer->IsCreated());

    gbuffer->Resize(newSize);

    Framebuffer* opaquePassFramebuffer = view->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);

    if (passData.lightingFramebuffer.IsValid())
    {
        EnqueueDeletion(std::move(passData.lightingFramebuffer));
    }

    if (passData.depthPrepassFramebuffer.IsValid())
    {
        EnqueueDeletion(std::move(passData.depthPrepassFramebuffer));
    }

    passData.lightingFramebuffer = CreateLightingFramebuffer(gbuffer);
    passData.depthPrepassFramebuffer = CreateDepthPrepassFramebuffer(gbuffer);

    passData.directLightingPass->Resize(newSize);
    passData.indirectLightingPass->Resize(newSize);

    passData.depthPyramidRenderer.Reset();
    passData.depthPyramidRenderer = MakeUnique<DepthPyramidRenderer>(gbuffer);
    passData.depthPyramidRenderer->Create();

    EnqueueDeletion(std::move(passData.mipChain));
    for (FramebufferRef& framebuffer : passData.mipChainFramebuffers)
    {
        EnqueueDeletion(std::move(framebuffer));
    }
    passData.mipChainFramebuffers.Clear();

    passData.mipChain = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2D,
        opaquePassFramebuffer->GetAttachment(0)->GetFormat(),
        Vec3u(opaquePassFramebuffer->GetExtent(), 1),
        TFM_LINEAR_MIPMAP,
        TFM_LINEAR_MIPMAP,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED | IU_ATTACHMENT });
    passData.mipChain->SetName(NAME("DeferredPassMipChain"));
    Check(passData.mipChain->Create());

    // Recreate framebuffers for each mip level
    {
        const uint32 numMips = passData.mipChain->GetTextureDesc().NumMips();
        passData.mipChainFramebuffers.Resize(numMips);

        for (uint32 mipLevel = 0; mipLevel < numMips; ++mipLevel)
        {
            const Vec3u& baseExtent = passData.mipChain->GetTextureDesc().extent;
            Vec2u mipExtent {
                MathUtil::Max(baseExtent.x >> mipLevel, 1u),
                MathUtil::Max(baseExtent.y >> mipLevel, 1u)
            };

            FramebufferDesc framebufferDesc {};
            framebufferDesc.extent = mipExtent;

            passData.mipChainFramebuffers[mipLevel] = RI.MakeFramebuffer(framebufferDesc);
#ifdef HYP_RHI_DEBUG_NAMES
            passData.mipChainFramebuffers[mipLevel]->SetDebugName(NAME_FMT("DeferredPassMipChain_FB{}", mipLevel));
#endif

            GpuImageViewRef mipImageView = RI.MakeImageView(
                passData.mipChain->GetGpuImage(),
                uint8(mipLevel),
                1, // mip level, 1 mip
                0,
                1 // layer 0, 1 layer
            );

            Attachment* attachment = passData.mipChainFramebuffers[mipLevel]->AddAttachment(
                0,
                AttachmentDesc {
                    TextureType::Texture2D,
                    passData.mipChain->GetTextureDesc().format,
                    LoadOperation::CLEAR,
                    StoreOperation::STORE },
                mipImageView);

            Check(passData.mipChainFramebuffers[mipLevel]->Create());
        }
    }

    passData.hbao = MakeUnique<HBAO>(newSize, gbuffer);
    passData.hbao->Create();

    passData.ssgi = MakeUnique<SSGI>(gbuffer);
    passData.ssgi->Create();

    passData.bloomPass = MakeUnique<BloomPass>(newSize, gbuffer);
    passData.bloomPass->Create();

    passData.reflectionsPass = MakeUnique<ReflectionsPass>(newSize, gbuffer);
    passData.reflectionsPass->Create();

    passData.tonemapPass = MakeUnique<TonemapPass>(newSize, gbuffer);
    passData.tonemapPass->Create();

    passData.lightmapPass = MakeUnique<LightmapPass>();
    passData.lightmapPass->Create();

    passData.fogVolumePass = MakeUnique<FogVolumePass>(newSize, gbuffer);
    passData.fogVolumePass->Create();

#ifdef HYP_EDITOR
    passData.editorGridPass = MakeUnique<EditorGridPass>();
    passData.editorGridPass->Create();
#endif

    passData.taaPass = MakeUnique<TAAPass>(passData.tonemapPass->GetFinalImageView(), newSize, gbuffer);
    passData.taaPass->Create();

    CreateViewRayTracingPasses(view, passData);

    passData.view = MakeWeakRef(view);
}

void DeferredPass::RenderFrame(Frame* frame, const RenderSetup& rs)
{
    AssertDebug(rs.world);

    Array<RenderProxyList*, RenderTempAllocator> renderProxyLists;
    renderProxyLists.Reserve(rs.world->GetViews().Size());

    HYP_DEFER({
        for (RenderProxyList* rpl : renderProxyLists)
        {
            rpl->EndRead();
        }
    });

    if (!m_quadMesh)
    {
        m_quadMesh = MeshBuilder::Quad();
        m_quadMesh->SetName(NAME("DeferredPassQuad"));
        m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
        m_quadMesh->SetIsTransient(true);
        m_quadMesh->UploadGpuData();
    }

    struct DrawShadowMapParams
    {
        Light* light;
        View* view;
    };

    // --- Collect view-independent renderable types from all views, binned ---
    FixedArray<Set<EnvProbe*, RenderTempAllocator>, EPT_MAX> envProbes;
    Set<ProbeVolume*, RenderTempAllocator> probeVolumes;

    // For rendering ProbeVolumes and EnvProbes, we use a directional light from one of the Views that references it (if found)
    Map<ProbeVolume*, Light*, RenderTempAllocator> probeVolumeLights;
    Map<EnvProbe*, Light*, RenderTempAllocator> envProbeLights;

    FixedArray<Set<Light*, RenderTempAllocator>, NumLightTypes> lights;
    Map<ShadowMapCacheKey, DrawShadowMapParams, RenderTempAllocator> lightsForShadow;
    // ---

    // init view pass data and collect global rendering resources
    // (env probes, env grids)
    for (View* view : rs.world->GetViews())
    {
        AssertDebug(view != nullptr);

        RenderProxyList& rpl = GetConsumerProxyList(view);
        rpl.BeginRead();

        renderProxyLists.PushBack(&rpl);

        if (view->GetFlags() & ViewFlags::GBUFFER)
        {
            GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();

            if (!gbuffer || gbuffer->GetExtent() != rs.viewport.extent)
            {
                PassData* pd = FetchViewPassData(view);
                Assert(pd != nullptr);

                DeferredPassData* pdCasted = DynamicCast<DeferredPassData>(pd);
                Assert(pdCasted != nullptr);

                pdCasted->priority = view->GetPriority();

                if (gbuffer->GetExtent() != rs.viewport.extent)
                {
                    ResizeView(rs.viewport, view, *pdCasted);
                }
            }
        }
        else if ((view->GetFlags() & ViewFlags::RAY_TRACING) && RI.GetRenderConfig().rayTracing)
        {
            PassData* pd = FetchViewPassData(view);
            Assert(pd != nullptr);

            RayTracingPassData* pdCasted = DynamicCast<RayTracingPassData>(pd);
            Assert(pdCasted != nullptr);

            RenderSetup newRenderSetup = rs.Fork();
            newRenderSetup.passData = pd;
            newRenderSetup.view = view;

            UpdateRayTracingView(frame, newRenderSetup);
        }

        const bool shouldCollectLightsForShadow = view->ShouldCollectShadowViews();

        // Collect lights
        for (Light* light : rpl.GetLights())
        {
            AssertDebug(light != nullptr);

            lights[uint32(light->GetLightType())].Add(light);

            if (shouldCollectLightsForShadow && (light->GetLightFlags() & LightFlags::ShadowCaster))
            {
                const ShadowMapCacheKey cacheKey = MakeShadowMapCacheKey(light, view);

                lightsForShadow[cacheKey] = {
                    light,
                    cacheKey.IsCameraDependent() ? view : nullptr
                };
            }
        }

        for (EnvProbe* envProbe : rpl.GetEnvProbes())
        {
            if (envProbes[envProbe->GetEnvProbeType()].Contains(envProbe))
            {
                continue;
            }

            if (!envProbeLights.Contains(envProbe))
            {
                for (Light* light : rpl.GetLights())
                {
                    AssertDebug(light != nullptr);

                    if (light->GetLightType() == LightType::Directional)
                    {
                        envProbeLights[envProbe] = light;

                        break;
                    }
                }
            }

            envProbes[envProbe->GetEnvProbeType()].Add(envProbe);
        }

        for (ProbeVolume* probeVolume : rpl.GetProbeVolumes())
        {
            if (probeVolumes.Contains(probeVolume))
            {
                continue;
            }

            if (!probeVolumeLights.Contains(probeVolume))
            {
                for (Light* light : rpl.GetLights())
                {
                    if (light->GetLightType() == LightType::Directional)
                    {
                        probeVolumeLights[probeVolume] = light;

                        break;
                    }
                }
            }

            probeVolumes.Add(probeVolume);
        }
    }

    { // Render shadow maps for all collected lights
        for (const auto& pair : lightsForShadow)
        {
            const ShadowMapCacheKey& key = pair.first;
            const DrawShadowMapParams& params = pair.second;

            Light* light = params.light;
            View* view = params.view;

            const uint32 lightTypeIndex = uint32(light->GetLightType());

            AssertDebug(lightTypeIndex < RI.namedPasses[NamedPass::ShadowMap].Size());

            PassBase* shadowRenderer = RI.namedPasses[NamedPass::ShadowMap][lightTypeIndex];

            if (!shadowRenderer)
            {
                continue;
            }

            RenderSetup shadowRs = rs.Fork();
            shadowRs.light = light;
            shadowRs.view = view;

            shadowRenderer->RenderFrame(frame, shadowRs);
        }
    }

    { // Render dynamic probes (realtime or queued for rerender)
        RenderSetup envProbeSetup = rs.Fork();

        if (lights[uint32(LightType::Directional)].Any())
        {
            envProbeSetup.light = lights[uint32(LightType::Directional)].Front();
        }

        if (envProbes.Any())
        {
            // check for dynamic probes to render
            for (uint32 envProbeType = 0; envProbeType < EPT_MAX; envProbeType++)
            {
                if (envProbes[envProbeType].Empty())
                {
                    continue;
                }

                PassBase* pass = RI.namedPasses[NamedPass::EnvProbe][envProbeType];
                AssertDebug(pass != nullptr);

                uint32 numRendered = 0;

                for (EnvProbe* envProbe : envProbes[envProbeType])
                {
                    if (envProbe->IsBaked())
                    {
                        continue; // skip baked
                    }

                    RenderSetup currentEnvProbeSetup = envProbeSetup.Fork();
                    currentEnvProbeSetup.envProbe = envProbe;

                    pass->RenderFrame(frame, currentEnvProbeSetup);

                    ++numRendered;
                }
            }
        }
    }

    for (View* view : rs.world->GetViews())
    {
        if (!(view->GetFlags() & ViewFlags::GBUFFER))
        {
            continue;
        }

        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(view->GetCamera()));
        
        if (!cameraProxy)
        {
            continue; // Not yet ready
        }

        DeferredPassData* pd = DynamicCast<DeferredPassData>(FetchViewPassData(view));
        AssertDebug(pd != nullptr);

        RenderSetup currentViewSetup = rs.Fork();
        currentViewSetup.view = view;
        currentViewSetup.passData = pd;

        RenderFrameForView(frame, currentViewSetup);

        RenderProxyList& rpl = GetConsumerProxyList(view);

        g_statTextures += rpl.GetTextures().NumCurrent();
        g_statMaterials += rpl.GetMaterials().NumCurrent();
        g_statLightmapVolumes += rpl.GetLightmapVolumes().NumCurrent();
        g_statParticleVolumes += rpl.GetParticleVolumes().NumCurrent();
        g_statLights += rpl.GetLights().NumCurrent();
        g_statProbeVolumes += rpl.GetProbeVolumes().NumCurrent();
        g_statEnvProbes += rpl.GetEnvProbes().NumCurrent();

#if 0
        HYP_LOG(Rendering, Verbose, "View '{}' used {} textures, {} materials, {} lightmap volumes, {} lights, {} env grids and {} env probes.",
            view->Id(),
            rpl.GetTextures().NumCurrent(),
            rpl.GetMaterials().NumCurrent(),
            rpl.GetLightmapVolumes().NumCurrent(),
            rpl.GetLights().NumCurrent(),
            rpl.GetProbeVolumes().NumCurrent(),
            rpl.GetEnvProbes().NumCurrent());
#endif
    }
}

void DeferredPass::RenderFrameForView(Frame* frame, const RenderSetup& rs)
{
    AssertDebug(rs.world && rs.view);

    View* view = rs.view;
    Assert(view->GetFlags() & ViewFlags::GBUFFER);

    RenderProxyList& rpl = GetConsumerProxyList(view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    RenderCollector& renderCollector = GetRenderCollector(view);

    DeferredPassData* passDataCasted = DynamicCast<DeferredPassData>(rs.passData);
    AssertDebug(passDataCasted != nullptr);

    DeferredPassData& passData = *passDataCasted;

    const uint32 frameIndex = frame->GetFrameIndex();

    if ((g_cvSkipRendering.Get() != 0) || rs.viewport.extent.Volume() == 0)
    {
        return;
    }

    if (m_renderedViewOutputs.frameIndex != frameIndex)
    {
        m_renderedViewOutputs.frameIndex = frameIndex;
        m_renderedViewOutputs.items.Resize(0);
    }

    // Assign lights and envprobes to tiles.
    // Must happen before any draw calls are returned as they will need to bind
    // the cluster tile / index buffers.
    m_tileProcessor->ProcessView(
        rs.viewport,
        view,
        passData.gridTilesBuffer,
        passData.gridIndexBuffer);

    Framebuffer* opaquePassFramebuffer = view->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);
    Framebuffer* depthPrepassFramebuffer = passData.depthPrepassFramebuffer;

    static const bool s_indirectRendering = RI.GetRenderConfig().indirectRendering;
    const bool performDepthPrepass = s_indirectRendering && g_cvDepthPrepass.Get();

    constexpr uint32 PrepassRenderBucketsMask = RenderBucketMask<RenderBucket::Opaque, RenderBucket::Lightmapped>;

    if (performDepthPrepass)
    {
        AssertDebug(depthPrepassFramebuffer != nullptr);

        renderCollector.BeginRecordDrawCalls(frame, rs, PrepassRenderBucketsMask, true);
        
        { // Render prepass
            ENGINE_STAT_GPU_SCOPE(&s_statDepthPrepass);

            if (renderCollector.HasDrawCalls(RenderBucket::Opaque)
                || renderCollector.HasDrawCalls(RenderBucket::Lightmapped))
            {
                renderCollector.ExecuteDrawCalls(frame, rs, depthPrepassFramebuffer, PrepassRenderBucketsMask, true);
            }
            else
            {
                frame->cr << SetCurrentFramebuffer(depthPrepassFramebuffer);
                frame->cr << ClearFramebuffer(depthPrepassFramebuffer);
                frame->cr << SetCurrentFramebuffer(nullptr);
            }
        }
    }
    
    { // Build hi-z with depth from prepass or prev frame depth
        ENGINE_STAT_GPU_SCOPE(&s_statBuildHiZ);

        passData.depthPyramidRenderer->Render(frame);
    }

    {
        ENGINE_STAT_GPU_SCOPE(&s_statOcclusionCulling);

        renderCollector.PerformOcclusionCulling(frame, rs, AllRenderBucketsMask);
    }
            
    renderCollector.BeginRecordDrawCalls(frame, rs, AllRenderBucketsMask);

    Framebuffer* lightmapPassFramebuffer = view->GetOutputTarget().GetFramebuffer(GBufferPass::Lightmapped);
    Framebuffer* translucentPassFramebuffer = view->GetOutputTarget().GetFramebuffer(GBufferPass::Translucent);
    Framebuffer* effectPassFramebuffer = view->GetOutputTarget().GetFramebuffer(GBufferPass::Effect);
    Framebuffer* debugPassFramebuffer = view->GetOutputTarget().GetFramebuffer(GBufferPass::Debug);

    const bool useRayTracingReflections = (g_cvPathTracing.Get() || g_cvRayTracedReflections.Get())
        && view->GetRayTracingView().IsValid()
        && passData.rayTracingReflections != nullptr;

    const bool useRayTracingGlobalIllumination = g_cvDDGI.Get()
        && view->GetRayTracingView().IsValid()
        && passData.ddgi != nullptr;

    // render opaque objects into separate framebuffer
    frame->cr << SetCurrentFramebuffer(opaquePassFramebuffer);

    if (performDepthPrepass)
    {
        frame->cr << SetDepthCompareOp(DCO_LESS_OR_EQUAL);
    }

    frame->cr << ClearFramebuffer(opaquePassFramebuffer, 0x1);

    if (renderCollector.HasDrawCalls(RenderBucket::Opaque)
        || (!g_cvEnableLightmapVolumes.Get() && renderCollector.HasDrawCalls(RenderBucket::Lightmapped)))
    {
        ENGINE_STAT_GPU_SCOPE(&s_statFillOpaque);

        renderCollector.ExecuteDrawCalls(frame, rs, RenderBucketMask<RenderBucket::Opaque>);

        if (!g_cvEnableLightmapVolumes.Get())
        {
            renderCollector.ExecuteDrawCalls(frame, rs, RenderBucketMask<RenderBucket::Lightmapped>);
        }
    }

    if (performDepthPrepass)
    {
        frame->cr << SetDepthCompareOp(DCO_LESS);
    }

    // unset opaque target
    frame->cr << SetCurrentFramebuffer(nullptr);

    if (g_cvEnableLightmapVolumes.Get())
    {
        // render objects to be lightmapped, separate from the opaque objects.
        // The lightmap bucket's framebuffer has a color attachment that will write into the opaque framebuffer's color attachment.
        if (renderCollector.HasDrawCalls(RenderBucket::Lightmapped))
        {
            ENGINE_STAT_GPU_SCOPE(&s_statFillOpaque);

            frame->cr << SetCurrentFramebuffer(lightmapPassFramebuffer);

            if (performDepthPrepass)
            {
                frame->cr << SetDepthCompareOp(DCO_LESS_OR_EQUAL);
            }

            renderCollector.ExecuteDrawCalls(frame, rs, RenderBucketMask<RenderBucket::Lightmapped>);

            if (performDepthPrepass)
            {
                frame->cr << SetDepthCompareOp(DCO_LESS);
            }

            frame->cr << SetCurrentFramebuffer(nullptr);
        }
    }

    if (renderCollector.HasDrawCalls(RenderBucket::Sky))
    {
        frame->cr << SetCurrentFramebuffer(translucentPassFramebuffer);

        renderCollector.ExecuteDrawCalls(frame, rs, translucentPassFramebuffer, RenderBucketMask<RenderBucket::Sky>);

        frame->cr << SetCurrentFramebuffer(nullptr);
    }

    if ((useRayTracingGlobalIllumination || useRayTracingReflections) && view->GetRayTracingView().IsValid())
    {
        Handle<View> rayTracingView = view->GetRayTracingView().Lock();

        if (rayTracingView != nullptr)
        {
            RayTracingPassData* rayTracingPassData = DynamicCast<RayTracingPassData>(FetchViewPassData(rayTracingView));
            Assert(rayTracingPassData != nullptr);

            const TopLevelASRef& tlas = rayTracingPassData->rayTracingTlases[frameIndex];

            if (tlas && tlas->IsCreated())
            {
                rayTracingPassData->parentPass = &passData;

                RenderSetup rayTracingRS = rs.Fork();
                rayTracingRS.passData = rayTracingPassData;

                // set sky as fallback

                // Set first found sky probe as fallback probe
                auto& skyProbes = rpl.GetEnvProbes().GetElements<SkyProbe>();
                if (skyProbes.Any())
                {
                    rayTracingRS.envProbe = *skyProbes.Begin();
                }

                if (useRayTracingReflections)
                {
                    AssertDebug(passData.rayTracingReflections != nullptr);
                    passData.rayTracingReflections->Render(frame, rayTracingRS);
                }

                if (useRayTracingGlobalIllumination)
                {
                    AssertDebug(passData.ddgi != nullptr);
                    passData.ddgi->Render(frame, rayTracingRS);
                }

                // unset parent pass after using it
                rayTracingPassData->parentPass = nullptr;
            }
        }
    }

    if (g_cvHBAO.Get())
    {
        passData.hbao->Render(frame, rs);
    }

    if (g_cvSSGI.Get())
    {
        passData.ssgi->Render(frame, rs);

        if (Texture* ssgiResultTexture = passData.ssgi->GetFinalResultTexture())
        {
            // make sure it is in a state for reading, we don't want any transitions between lightmap -> deferred indirect pass.
            frame->cr << InsertBarrier(
                ssgiResultTexture->GetGpuImage(),
                RS_SHADER_RESOURCE,
                ShaderModuleType::Pixel);
        }
    }

    if (g_cvSSR.Get())
    {
        passData.reflectionsPass->ssrPass->Render(frame, rs);

        if (Texture* ssrResultTexture = passData.reflectionsPass->ssrPass->GetFinalResultTexture())
        {
            frame->cr << InsertBarrier(
                ssrResultTexture->GetGpuImage(),
                RS_SHADER_RESOURCE,
                ShaderModuleType::Pixel);
        }
    }

    passData.postProcessing->RenderPre(frame, rs);

    { // deferred lighting on opaque objects
        ENGINE_STAT_GPU_SCOPE(&s_statDeferredPass);

        // Pre-transition resources to avoid breaking the render pass for barriers
        frame->cr << InsertBarrier(
            passData.lightingFramebuffer->GetAttachment(1)->GetGpuImage(),
            RS_RENDER_TARGET,
            ShaderModuleType::Pixel,
            /* onlyDepth */ false,
            /* onlyStencil */ true);

        // Transition shadow map atlas to shader resource before the pass
        frame->cr << InsertBarrier(RI.shadowMapCache->GetAtlasImage(), RS_SHADER_RESOURCE, ShaderModuleType::Pixel);
        // Transition point light shadow map atlas to shader resource before the pass
        frame->cr << InsertBarrier(RI.shadowMapCache->GetPointLightShadowMapImage(), RS_SHADER_RESOURCE, ShaderModuleType::Pixel);

        frame->cr << SetCurrentFramebuffer(passData.lightingFramebuffer);
        
        // We need to use NONE because we draw lightmap volumes as boxes, not quads,
        // and we need the camera to be able to see the inside of the box.
        // Changing cull mode during lightmap volume drawing will break the render pass.
        frame->cr << SetFaceCullMode(FCM_NONE);
        frame->cr << SetCurrentBlendFunction(BlendFunction::Additive());

        const bool isPathTracer = g_cvPathTracing.Get();

        passData.indirectLightingPass->RenderToFramebuffer(frame, rs, passData.lightingFramebuffer);

        if (g_cvEnableLightmapVolumes.Get() && rpl.GetLightmapVolumes().NumCurrent() != 0 && !isPathTracer)
        {
            // Render the objects to have lightmaps applied into the translucent pass framebuffer with a full screen quad.
            // Apply lightmaps over the now shaded opaque objects.
            passData.lightmapPass->RenderToFramebuffer(frame, rs, passData.lightingFramebuffer);
        }

        if (!isPathTracer)
        {
            passData.directLightingPass->RenderToFramebuffer(frame, rs, passData.lightingFramebuffer);
        }
        
        frame->cr << SetFaceCullMode(FCM_BACK);
        frame->cr << SetCurrentBlendFunction(BlendFunction::None());
        frame->cr << SetCurrentFramebuffer(nullptr);
    }

    { // generate mipchain after rendering opaque objects' lighting, now we can use it for transmission
        const GpuImageRef& srcImage = passData.lightingFramebuffer->GetAttachment(0)->GetGpuImage();
        GenerateMipChain(frame, rs, renderCollector, srcImage);
    }

    { // Render the deferred lighting into the color target with a full screen quad.
        frame->cr << SetCurrentFramebuffer(effectPassFramebuffer);

        frame->cr << SetCurrentViewport(rs.viewport);

        frame->cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
        frame->cr << SetFaceCullMode(FCM_BACK);
        frame->cr << SetFillMode(FM_FILL);
        frame->cr << SetTopology(TOP_TRIANGLES);
        frame->cr << SetDepthTest(false);
        frame->cr << SetDepthWrite(false);

        static constexpr uint8 StencilFilterMask = SkyStencilMask;

        // frame->cr << SetStencilTest(false);
        frame->cr << SetStencilTest(true);
        frame->cr << SetStencilFunction(StencilFunction { SO_KEEP, SO_KEEP, SO_KEEP, SCO_EQUAL });
        frame->cr << SetStencilState(0, StencilFilterMask, 0x0);

        frame->cr << SetCurrentShader(ShaderDesc(NAME("BlitTexture")));

        frame->cr << SetShaderUniform(0, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
        frame->cr << SetShaderUniform(1, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        frame->cr << SetShaderUniform(2, "InTexture"_sh, passData.lightingFramebuffer->GetAttachment(0)->GetImageView());

        frame->cr << CommitDrawState();

        frame->cr << BindVertexBuffer(m_quadMesh->GetVertexBuffer());
        frame->cr << BindIndexBuffer(m_quadMesh->GetIndexBuffer());

        frame->cr << DrawIndexed(6);

        // reset
        frame->cr << SetDepthTest(true);
        frame->cr << SetDepthWrite(true);
        frame->cr << SetStencilTest(false);

        frame->cr << SetCurrentFramebuffer(nullptr);
    }

    { // Translucent, forward lit
        ENGINE_STAT_GPU_SCOPE(&s_statFillTranslucent);

        frame->cr << SetCurrentFramebuffer(translucentPassFramebuffer);

        // begin translucent with forward rendering
        if (renderCollector.HasDrawCalls(RenderBucket::Translucent))
        {
            renderCollector.ExecuteDrawCalls(frame, rs, RenderBucketMask<RenderBucket::Translucent>);
        }

        // render fog volumes
        if (g_cvFogVolumes.Get())
        {
            RenderSetup fogVolumeRS = rs.Fork();
            fogVolumeRS.framebuffer = effectPassFramebuffer;

            passData.fogVolumePass->Render(frame, fogVolumeRS);
        }

#ifdef HYP_EDITOR
        if (g_cvEditorGrid.Get() && rs.view && (rs.view->GetFlags() & ViewFlags::EDITOR_VIEW))
        {
            RenderSetup gridRS = rs.Fork();
            gridRS.framebuffer = effectPassFramebuffer;

            passData.editorGridPass->Render(frame, gridRS);
        }
#endif // HYP_EDITOR

        // render particles
        if (rpl.GetParticleVolumes().NumCurrent())
        {
            for (ParticleVolume* particleVolume : rpl.GetParticleVolumes())
            {
                RenderSetup particleVolumeRS = rs.Fork();
                particleVolumeRS.volume = particleVolume;

                RI.namedPasses[NamedPass::ParticleVolume][0]->RenderFrame(frame, particleVolumeRS);
            }
        }

        { // draw sprites
            SpritePass* spriteRenderer = static_cast<SpritePass*>(RI.namedPasses[NamedPass::Sprite][0]);

            if (spriteRenderer != nullptr)
            {
                spriteRenderer->RenderFrame(frame, rs);
            }
        }

        frame->cr << SetCurrentFramebuffer(nullptr);
    }

    if (g_cvBloom.Get())
    {
        passData.bloomPass->Render(frame, rs);
    }
    
#ifdef HYP_EDITOR
    if (rs.view && (rs.view->GetFlags() & ViewFlags::EDITOR_VIEW))
    {
        // debug draw - editor only
        if (renderCollector.HasDrawCalls(RenderBucket::Debug)
            || DebugDrawer::GetInstance().NumEnqueuedDrawCommands() > 0)
        {
            ENGINE_STAT_GPU_SCOPE(&s_statFillDebug);

            frame->cr << SetCurrentFramebuffer(debugPassFramebuffer);

            ExecuteDrawCalls(frame, rs, renderCollector, RenderBucketMask<RenderBucket::Debug>);

            DebugDrawer::GetInstance().Render(frame, rs);

            frame->cr << SetCurrentFramebuffer(nullptr);
        }
    }
#endif // HYP_EDITOR

    passData.postProcessing->RenderPost(frame, rs);

    passData.tonemapPass->Render(frame, rs);

    if (passData.taaPass != nullptr && g_cvTAA.Get())
    {
        passData.taaPass->Render(frame, rs);
    }

    // depth of field
    // m_dofBlur->Render(frame);

    GpuImageViewRef finalImageView = (passData.taaPass != nullptr && g_cvTAA.Get())
        ? RI.textureViewCache->GetOrCreate(passData.taaPass->GetResultTexture())
        : passData.tonemapPass->GetFinalImageView();

    // Ordered by View priority
    auto outputsIt = std::lower_bound(
        m_renderedViewOutputs.items.Begin(),
        m_renderedViewOutputs.items.End(),
        passData.priority,
        [](const RenderedViewOutput& a, int priority)
        {
            return a.priority < priority;
        });

    m_renderedViewOutputs.items.Insert(outputsIt, RenderedViewOutput { view, std::move(finalImageView), passData.priority });
}

void DeferredPass::UpdateRayTracingView(Frame* frame, const RenderSetup& rs)
{
    View* view = rs.view;
    AssertDebug(view != nullptr);

    if (!(view->GetFlags() & ViewFlags::RAY_TRACING) || !RI.GetRenderConfig().rayTracing)
    {
        return;
    }

    const uint32 currentFrameIndex = GetFrameCounter() % NumFramesInFlight;

    RayTracingPassData* pd = DynamicCast<RayTracingPassData>(rs.passData);

    RenderProxyList& rpl = GetConsumerProxyList(rs.view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    if (!pd->rayTracingTlases[currentFrameIndex])
    {
        for (TopLevelASRef& tlas : pd->rayTracingTlases)
        {
            tlas = RI.MakeTLAS();
        }
    }

    bool hasBlas = false;

    Array<ObjId<Entity>, RenderTempAllocator> removed;
    rpl.GetMeshEntities().GetRemoved(removed, /* includeChanged */ false);

    // Remove BLASes for mesh entities that were removed from the list.
    for (const ObjId<Entity>& entityId : removed)
    {
        RenderProxyMesh* meshProxy = rpl.GetMeshEntities().GetProxy(entityId);
        Assert(meshProxy != nullptr);

        AssertDebug(meshProxy->mesh != nullptr);
        AssertDebug(meshProxy->material != nullptr);

        const RenderBucket bucket = meshProxy->material->GetAttributes().bucket;

        if (bucket != RenderBucket::Opaque
            && bucket != RenderBucket::Lightmapped
            && bucket != RenderBucket::Translucent)
        {
            continue;
        }

        const uint64 key = BLASCache::MakeKey(entityId, meshProxy->mesh->Id(), meshProxy->material->Id());

        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            const bool removed = pd->rayTracingTlases[frameIndex]->RemoveBLAS(key);

            if (!removed)
            {
                HYP_LOG(Rendering, Error, "Failed to remove BLAS for Mesh Entity {} from top level acceleration structure!",
                        entityId.Value());
            }
        }

        RI.blasCache->RemoveBLAS(entityId, key);
    }

    for (Entity* entity : rpl.GetMeshEntities())
    {
        AssertDebug(entity != nullptr);

        RenderProxyMesh* meshProxy = rpl.GetMeshEntities().GetProxy(entity->Id());
        Assert(meshProxy != nullptr);

        AssertDebug(meshProxy->mesh != nullptr);
        AssertDebug(meshProxy->material != nullptr);

        const RenderBucket bucket = meshProxy->material->GetAttributes().bucket;

        if (bucket != RenderBucket::Opaque
            && bucket != RenderBucket::Lightmapped
            && bucket != RenderBucket::Translucent)
        {
            continue;
        }

        uint64 newKey;
        uint64 oldKey;
        BottomLevelAS* blas;

        RI.blasCache->GetOrCreateBLAS(
            entity->Id(), meshProxy->mesh, meshProxy->material,
            newKey, oldKey,
            blas);

        if (!blas)
        {
            HYP_LOG(Rendering, Error, "Failed to build BLAS for Mesh Entity {}", entity->GetName());
            continue;
        }

        if (oldKey != 0 && oldKey != newKey)
        {
            for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
            {
                const bool removed = pd->rayTracingTlases[frameIndex]->RemoveBLAS(oldKey);
                AssertDebug(removed);

                if (!removed)
                {
                    HYP_LOG(Rendering, Error, "Failed to remove BLAS for Mesh {} from top level acceleration structure!",
                            meshProxy->mesh->GetName());
                }
            }
        }

        if (!blas->IsCreated())
        {
            blas->SetTransform(meshProxy->bufferData.modelMatrix);

            const uint32 materialBinding = Resources::GetBinding(meshProxy->material);
            blas->SetMaterialBinding(materialBinding);

            Check(blas->Create());
        }
        else
        {
            const uint32 materialBinding = Resources::GetBinding(meshProxy->material);

            blas->SetMaterialBinding(materialBinding);
            blas->SetTransform(meshProxy->bufferData.modelMatrix);
        }

        if (!pd->rayTracingTlases[currentFrameIndex]->ContainsBLAS(newKey))
        {
            for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
            {
                pd->rayTracingTlases[frameIndex]->AddBLAS(newKey, blas);
            }

            hasBlas = true;
        }
    }

    if (!pd->rayTracingTlases[currentFrameIndex]->IsCreated())
    {
        if (hasBlas)
        {
            for (TopLevelASRef& tlas : pd->rayTracingTlases)
            {
                Check(tlas->Create());
            }
        }

        return;
    }

    RTUpdateStateFlags updateStateFlags;
    pd->rayTracingTlases[currentFrameIndex]->UpdateStructure(updateStateFlags);
}

void DeferredPass::ExecuteDrawCalls(
    Frame* frame,
    const RenderSetup& rs,
    RenderCollector& renderCollector,
    uint32 bucketMask)
{
    renderCollector.ExecuteDrawCalls(frame, rs, bucketMask);
}

void DeferredPass::GenerateMipChain(Frame* frame, const RenderSetup& rs, RenderCollector& renderCollector, const GpuImageRef& srcImage)
{
    DeferredPassData* pd = DynamicCast<DeferredPassData>(rs.passData);

    const Handle<Texture>& mipChainTexture = pd->mipChain;
    AssertDebug(mipChainTexture.IsValid());

    const uint8 numMips = mipChainTexture->GetTextureDesc().NumMips();

    CommandRecorder& cr = frame->cr;

    // Copy the source image to mip 0 of the mip chain
    cr << InsertBarrier(srcImage, RS_COPY_SRC);
    cr << InsertBarrier(mipChainTexture->GetGpuImage(), RS_COPY_DST);

    cr << CopyImage(
        srcImage,
        mipChainTexture->GetGpuImage(),
        mipChainTexture->GetTextureDesc().extent,
        ImageSubResource { .baseMipLevel = 0, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 },
        ImageSubResource { .baseMipLevel = 0, .numLevels = 1, .baseArrayLayer = 0, .numLayers = 1 });

    // Transition mip 0 to shader resource for reading (source for first downsample)
    cr << InsertBarrier(mipChainTexture->GetGpuImage(), RS_SHADER_RESOURCE);

    for (uint8 mipLevel = 1; mipLevel < numMips; ++mipLevel)
    {
        const uint32 srcMip = mipLevel - 1;

        // Calculate dimensions for source and destination mip levels
        const Vec3u& baseExtent = mipChainTexture->GetTextureDesc().extent;
        Vec2u srcExtent {
            MathUtil::Max(baseExtent.x >> srcMip, 1u),
            MathUtil::Max(baseExtent.y >> srcMip, 1u)
        };

        // Create image view for the source mip level
        const GpuImageViewRef& srcMipView = RI.textureViewCache->GetOrCreate(
            mipChainTexture,
            uint8(srcMip),
            1, // mip level, 1 mip
            0,
            1 // layer 0, 1 layer
        );

        cr << InsertBarrier(mipChainTexture->GetGpuImage(), RS_RENDER_TARGET, ImageSubResource { mipLevel, 1, 0, 1 });

        // Set up render target (destination mip level) - use pre-created framebuffer
        Framebuffer* dstFramebuffer = pd->mipChainFramebuffers[mipLevel];
        AssertDebug(dstFramebuffer != nullptr);

        // Begin rendering to the destination mip
        cr << SetCurrentFramebuffer(dstFramebuffer);
        cr << SetCurrentViewport(Viewport { dstFramebuffer->GetExtent() });

        // Set up the BlitTexture shader for downsampling
        cr << SetCurrentShader(ShaderDesc(NAME("BlitTexture")));

        cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
        cr << SetTopology(TOP_TRIANGLES);
        cr << SetFillMode(FM_FILL);
        cr << SetDepthTest(false);
        cr << SetDepthWrite(false);
        cr << SetFaceCullMode(FCM_NONE);

        // Set shader uniforms
        cr << SetShaderUniform(0, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(1, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
        cr << SetShaderUniform(2, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(3, "InTexture"_sh, srcMipView);

        // Render fullscreen quad
        cr << CommitDrawState();

        // Get the fullscreen quad mesh (same as used by FullScreenPass)
        if (!m_quadMesh)
        {
            m_quadMesh = MeshBuilder::Quad();
            m_quadMesh->SetName(NAME("DeferredPassQuad"));
            m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
            m_quadMesh->SetIsTransient(true);
            m_quadMesh->UploadGpuData();
        }

        cr << BindVertexBuffer(m_quadMesh->GetVertexBuffer());
        cr << BindIndexBuffer(m_quadMesh->GetIndexBuffer());
        cr << DrawIndexed(6);

        // End rendering to this mip
        cr << SetCurrentFramebuffer(nullptr);

        cr << InsertBarrier(mipChainTexture->GetGpuImage(), RS_SHADER_RESOURCE, ImageSubResource { mipLevel, 1, 0, 1 });
    }

    // Reset depth state
    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);

    // Transition source image back to render target
    cr << InsertBarrier(srcImage, RS_RENDER_TARGET);
}

#pragma endregion DeferredPass

} // namespace Hyperion
