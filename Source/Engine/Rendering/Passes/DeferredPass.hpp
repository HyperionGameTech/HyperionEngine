/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/Pass.hpp>
#include <Rendering/FullScreenPass.hpp>
#include <Rendering/PostFX.hpp>
#include <Rendering/IndirectDraw.hpp>
#include <Rendering/TAAPass.hpp>
#include <Rendering/GraphicsPipelineCache.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RawBuffer.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>
#include <Core/Utilities/ClockTimer.hpp>

#include <Scene/Light/Light.hpp> // For LightType

namespace Hyperion {

class IndirectDrawState;
class GBuffer;
class Texture;
class DepthPyramidRenderer;
class SSRPass;
class SSGI;
class View;
class DeferredPass;
class GBuffer;
class EnvProbe;
class EditorWireframePass;
class TAAPass;
class PostProcessing;
class GTAOPass;
class BloomPass;
class SSAO;
class DOFBlur;
class Texture;
class RayTracingReflections;
class DDGI;
class RenderProxyList;
class RenderCollector;
class TileProcessor;
class StructuredBuffer;
class ByteAddressBuffer;

class LightingPass;
class ReflectionsPass;
class LightmapPass;
class HeightFogPass;
class VolumetricFogPass;
class CloudPass;
class TonemapPass;
class GlimmerIrradiancePass;
#ifdef HYP_EDITOR
class EditorGridPass;
#endif

struct RenderSetup;

enum class LightType : uint32;
enum EnvProbeType : uint32;

struct EyeAdaptationState
{
    RWStructuredBuffer buffer;
    ClockTimer timer;
    bool restartNext = true;
};

HYP_CLASS(NoScriptBindings)
class DeferredPassData : public PassData
{
    HYP_OBJECT_BODY(DeferredPassData);

public:
    virtual ~DeferredPassData() override;

    const GpuImageViewRef& GetSceneColorView() const;

    int priority = 0;

    Vec2u outputExtent;

    Handle<Texture> mipChain;
    
    /// One framebuffer per mip level for downsampling
    Array<FramebufferRef, RenderAllocator> mipChainFramebuffers;

    EyeAdaptationState eyeAdaptationState;

    UniquePtr<LightingPass> indirectLightingPass;
    UniquePtr<LightingPass> directLightingPass;
    
    /// Writes clustered env-probe reflections (pre-multiplied by coverage)
    UniquePtr<LightingPass> reflectionsOnlyPass;

    FramebufferRef lightingFramebuffer;
    FramebufferRef depthPrepassFramebuffer;

    UniquePtr<ReflectionsPass> reflectionsPass;

    UniquePtr<LightmapPass> lightmapPass;

    UniquePtr<HeightFogPass> heightFogPass;

    UniquePtr<VolumetricFogPass> volumetricFogPass;

    UniquePtr<CloudPass> cloudPass;

#ifdef HYP_EDITOR
    UniquePtr<EditorGridPass> editorGridPass;
#endif

    UniquePtr<TonemapPass> tonemapPass;

    UniquePtr<GTAOPass> gtao;
    UniquePtr<GlimmerIrradiancePass> glimmerIrradiancePass;
    // UniquePtr<SSAO> ssao;

    UniquePtr<FullScreenPass> combinePass;
    UniquePtr<PostProcessing> postProcessing;
    UniquePtr<TAAPass> taaPass;
    UniquePtr<SSGI> ssgi;
    UniquePtr<BloomPass> bloomPass;
    UniquePtr<DepthPyramidRenderer> depthPyramidRenderer;
    UniquePtr<DOFBlur> dofBlur;

    UniquePtr<RayTracingReflections> rayTracingReflections;
    UniquePtr<DDGI> ddgi;

    ByteAddressBuffer* gridTilesBuffer = nullptr;
    ByteAddressBuffer* gridIndexBuffer = nullptr;

    ByteAddressBuffer* clusteredShadowMapIndexBuffer = nullptr;
    ShadowMapData clusteredShadowMaps[MaxClusteredShadowMaps] {};
    uint32 numClusteredShadowMaps = 0;
};

HYP_CLASS(NoScriptBindings)
class RayTracingPassData : public PassData
{
    HYP_OBJECT_BODY(RayTracingPassData);

public:
    // Set only while rendering to this pass
    DeferredPassData* parentPass = nullptr;

    FixedArray<TopLevelASRef, NumFramesInFlight> rayTracingTlases;

    virtual ~RayTracingPassData() override;
};

class DeferredPass final : public PassBase
{
public:
    struct RenderedViewOutput
    {
        View* view = nullptr;
        GpuImageViewRef finalImageView;
        int priority = 0;
        bool isUpdated = true;
    };

    struct RenderedViewOutputs
    {
        uint32 frameIndex = ~0u;
        Array<RenderedViewOutput, RenderAllocator> items;
    };

    DeferredPass();
    
    DeferredPass(const DeferredPass& other) = delete;
    DeferredPass& operator=(const DeferredPass& other) = delete;

    virtual ~DeferredPass() override;

    HYP_FORCE_INLINE const RenderedViewOutputs& GetRenderedViewOutputs() const
    {
        return m_renderedViewOutputs;
    }

    /*! rief True if a view composited into p windowId produced a new image this frame. */
    bool HasUpdatedOutputForWindow(uint32 windowId) const;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(Frame* frame, const RenderSetup& rs) override;

private:
    void RenderFrameForView(Frame* frame, const RenderSetup& rs);
    void UpdateRayTracingView(Frame* frame, const RenderSetup& rs);

    // Called on initialization or when the view changes
    virtual PassData* CreateViewPassData(View* view, PassDataExt&) override;

    void CreateViewRayTracingPasses(View* view, DeferredPassData& passData);

    void CreateViewTopLevelAccelerationStructures(View* view, RayTracingPassData& passData);

    void ResizeView(Viewport viewport, Vec2u outputExtent, View* view, DeferredPassData& passData);

    void ExecuteDrawCalls(Frame* frame, const RenderSetup& rs, RenderCollector& renderCollector, uint32 bucketMask);
    void GenerateMipChain(Frame* frame, const RenderSetup& rs, RenderCollector& renderCollector, GpuImage* srcImage);
    void MeterEyeAdaptation(Frame* frame, const RenderSetup& rs);

    void BeginRenderedViewOutputs();
    void AddRenderedViewOutput(View* view, GpuImageViewRef&& imageView, int priority, bool isUpdated = true);

    RenderedViewOutputs m_renderedViewOutputs;

    Handle<Mesh> m_quadMesh;

    UniquePtr<TileProcessor> m_tileProcessor;

#ifdef HYP_EDITOR
    UniquePtr<EditorWireframePass> m_editorWireframePass;
#endif
};

} // namespace Hyperion
