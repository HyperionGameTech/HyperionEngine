/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/SkyVisibilityPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderSetup.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RendererMain.hpp>
#include <Rendering/RenderBucket.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Frame.hpp>

#include <Scene/View.hpp>
#include <Scene/Sky/DynamicSkySystem.hpp>
#include <Scene/Camera/Camera.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/CVarManager.hpp>

#include <Core/Utilities/DeferredScope.hpp>
#include <Core/Math/MathUtil.hpp>

#include <SkyVisibilityPass.generated.inl>

namespace Hyperion {

static EngineStatGpuTimer s_statSkyVisibility("Rendering/GPU/SkyVisibility");

static constexpr uint32 BucketMask = RenderBucketMask<RenderBucket::Opaque, RenderBucket::Lightmapped, RenderBucket::Translucent>;

static CVar<bool> s_cvSkyVisibilityTimeSlicingEnabled("Rendering.SkyVisibility.TimeSlicingEnabled", true);
static CVar<int> s_cvSkyVisibilityMaxStaleFrames("Rendering.SkyVisibility.MaxStaleFrames", 30);

SkyVisibilityPass::SkyVisibilityPass()
    : m_viewProjectionMatrix(Mat4f::Identity()),
      m_isValid(false),
      m_lastRenderedFrame(0)
{
}

SkyVisibilityPass::~SkyVisibilityPass()
{
}

void SkyVisibilityPass::Initialize()
{
}

void SkyVisibilityPass::Shutdown()
{
}

PassData* SkyVisibilityPass::CreateViewPassData(View* view, PassDataExt&)
{
    SkyVisibilityPassData* passData = new SkyVisibilityPassData();
    passData->view = MakeWeakRef(view);

    return passData;
}

const GpuImageViewRef& SkyVisibilityPass::GetDepthImageView() const
{
    if (!m_isValid || !m_depthImageView.IsValid())
    {
        return RI.placeholderData->GetImageView2D1x1R8();
    }

    return m_depthImageView;
}

void SkyVisibilityPass::WriteShaderData(CBufferAllocator& cbufferAllocator) const
{
    SkyVisibilityShaderData shaderData {};

    if (m_isValid)
    {
        shaderData.viewProjectionMatrix = m_viewProjectionMatrix;
        shaderData.params = Vec4f(
            DynamicSkySystem::SkyVisibilityWorldExtent,
            DynamicSkySystem::SkyVisibilityDepthRange,
            1.0f,
            0.0f);
    }

    cbufferAllocator.Write(&shaderData);
}

void SkyVisibilityPass::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    View* view = renderSetup.view;
    AssertDebug(view != nullptr && (view->GetFlags() & ViewFlags::SKY_VISIBILITY_VIEW));

    // nothing samples the map when the world turns sky occlusion off, so don't pay for the capture
    if (GetWorldBufferData()->skyOcclusionParams.x <= 0.0f)
    {
        m_isValid = false;
        return;
    }

    const FramebufferRef& framebuffer = view->GetOutputTarget().GetFramebuffer();

    if (!framebuffer.IsValid())
    {
        return;
    }

    if (GetRenderCollector(view).isFallback)
    {
        return;
    }

    RenderProxyList& rpl = GetConsumerProxyList(view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    // the camera is only used for its buffer binding; the capture's matrices live on the view
    if (!GetRenderProxy(view->GetCamera()))
    {
        return;
    }

    if (m_isValid && s_cvSkyVisibilityTimeSlicingEnabled.Get())
    {
        const bool viewProjChanged = rpl.cachedMatrices.viewProj != m_viewProjectionMatrix;
        const bool contentDirty = rpl.GetMeshEntities().GetDiff().NeedsUpdate() || rpl.GetSkeletons().GetDiff().NeedsUpdate();
        const uint32 maxStaleFrames = uint32(MathUtil::Max(s_cvSkyVisibilityMaxStaleFrames.Get(), 1));
        const bool isStale = (GetFrameCounter() - m_lastRenderedFrame) >= maxStaleFrames;

        if (!viewProjChanged && !contentDirty && !isStale)
        {
            return;
        }
    }

    ENGINE_STAT_GPU_SCOPE(&s_statSkyVisibility);

    Attachment* depthAttachment = framebuffer->GetAttachment(0);
    AssertDebug(depthAttachment != nullptr);

    RenderSetup rs = renderSetup.Fork();
    rs.view = view;
    rs.framebuffer = framebuffer;
    rs.passData = FetchViewPassData(view);
    rs.viewport = Viewport { framebuffer->GetExtent(), Vec2i::Zero() };

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(depthAttachment->GetGpuImage(), ResourceState::DepthStencil);

    // ExecuteDrawCalls clears the framebuffer when there is nothing to draw, which reads as an unoccluded sky
    GetRenderCollector(view).ExecuteDrawCalls(frame, rs, BucketMask);

    cr << InsertBarrier(depthAttachment->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Pixel);

    m_viewProjectionMatrix = rpl.cachedMatrices.viewProj;
    m_depthImageView = depthAttachment->GetImageView();
    m_isValid = true;
    m_lastRenderedFrame = GetFrameCounter();
}

} // namespace Hyperion
