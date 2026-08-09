/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <UIPch.hpp>

#include <UI/Overlays/DeviceDetailsOverlay.hpp>

#include <UI/UIListView.hpp>
#include <UI/UIText.hpp>

#include <Framework/DeviceDetails.hpp>
#include <Framework/EngineStats.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/CVarManager.hpp>

#include <Rendering/RenderInterface.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Threading/AtomicVar.hpp>

#include <DeviceDetailsOverlay.generated.inl>

namespace Hyperion {

extern EngineStatGpuTimer g_statGpuFrameTime;
extern EngineStatTimer g_statGpuWaitTime;
extern EngineStatTimer g_statFrameLimiterWait;
extern CVar<bool> g_cvEnableVSync;
extern AtomicVar<uint32> g_currentFrameRateLimit;

static String GetRenderingBackendText()
{
#if HYP_VULKAN
    return "Vulkan";
#elif HYP_DX12
    return "DX12";
#else
    return "<unknown>";
#endif
}

#pragma region DeviceDetailsOverlay

DeviceDetailsOverlay::DeviceDetailsOverlay()
    : OverlayBase()
{
}

DeviceDetailsOverlay::~DeviceDetailsOverlay() = default;

Handle<UIObject> DeviceDetailsOverlay::CreateUIObject_Impl(UIObject* spawnParent)
{
    HYP_SCOPE;

    DeviceDetails& device = RI.deviceDetails;

    Handle<UIPanel> panelBackdrop = spawnParent->CreateUIObject<UIPanel>(
        NAME_FMT("DeviceDetailsOverlay_PanelBackdrop"),
        Vec2i(2, 2),
        UIObjectSize({ 430, UIObjectSize::PIXEL }, { 30, UIObjectSize::PIXEL }));

    panelBackdrop->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.7f));
    panelBackdrop->SetPadding(Vec2i(10, 5));
    panelBackdrop->SetBorderRadius(6);

    m_panel = spawnParent->CreateUIObject<UIListView>(
        NAME_FMT("DeviceDetailsOverlay_Panel"),
        Vec2i::Zero(),
        UIObjectSize(100, UIObjectSize::PERCENT));

    m_panel->SetBackgroundColor(Color::Transparent());
    m_panel->SetOrientation(UIListViewOrientation::HORIZONTAL);

    m_fpsText = m_panel->CreateUIObject<UIText>(
        Vec2i::Zero(),
        UIObjectSize(UIObjectSize::AUTO));

    m_fpsText->SetTextSize(13.0f);
    m_fpsText->SetTextColor(Color(1.0f, 0.9f, 0.2f, 1.0f));
    m_fpsText->SetPadding(Vec2i(10, 0));
    m_panel->AddChildUIObject(m_fpsText);
    
    m_boundStatusText = m_panel->CreateUIObject<UIText>(
        Vec2i::Zero(),
        UIObjectSize(UIObjectSize::AUTO));
    m_boundStatusText->SetTextSize(13.0f);
    m_boundStatusText->SetTextColor(Color(0.7f, 0.7f, 0.7f, 1.0f));
    m_boundStatusText->SetPadding(Vec2i(10, 0));
    m_boundStatusText->SetText("");

    m_panel->AddChildUIObject(m_boundStatusText);

    m_renderingBackendText = m_panel->CreateUIObject<UIText>(
        Vec2i::Zero(),
        UIObjectSize(UIObjectSize::AUTO));

    m_renderingBackendText->SetTextSize(13.0f);
    m_renderingBackendText->SetTextColor(Color(0.7f, 0.7f, 0.7f, 1.0f));
    m_renderingBackendText->SetPadding(Vec2i(10, 0));
    m_renderingBackendText->SetText(GetRenderingBackendText());
    m_panel->AddChildUIObject(m_renderingBackendText);

    m_gpuModelText = m_panel->CreateUIObject<UIText>(
        Vec2i::Zero(),
        UIObjectSize(UIObjectSize::AUTO));

    m_gpuModelText->SetTextSize(13.0f);
    m_gpuModelText->SetTextColor(Color(0.9f, 0.9f, 0.9f, 1.0f));
    m_gpuModelText->SetPadding(Vec2i(10, 0));
    m_gpuModelText->SetText(device.GetGpuModel());
    m_panel->AddChildUIObject(m_gpuModelText);

    panelBackdrop->AddChildUIObject(m_panel);

    return panelBackdrop;
}

void DeviceDetailsOverlay::Update_Impl(float delta)
{
    HYP_SCOPE;

    const EngineStats& s_stats = *g_engineStats;
    const EngineStatsSnapshot& snapshot = s_stats.GetCurrentSnapshot();

    const int fps = MathUtil::Floor(snapshot[StatIdFps].avg);

    char fpsStr[8] = { '9', '9', '9', ' ', 'F', 'P', 'S', '\0' };

    if (HYP_LIKELY(fps < 1000))
    {
        fpsStr[0] = char('0' + (fps / 100) % 10);
        fpsStr[1] = char('0' + (fps / 10) % 10);
        fpsStr[2] = char('0' + fps % 10);
    }

    m_fpsText->SetText(String(fpsStr));

    const float frameMs = snapshot[StatIdMsPerFrame].avg;
    const float gpuMs = snapshot[g_statGpuFrameTime].avg;
    const float pacedWaitMs = snapshot[g_statGpuWaitTime].avg + snapshot[g_statFrameLimiterWait].avg;
    const float cpuMs = MathUtil::Max(frameMs - pacedWaitMs, 0.0f);

    const bool isRateCapped = g_currentFrameRateLimit.Get(MemoryOrder::RELAXED) > 0;

    const bool isFrameRateLocked = (g_cvEnableVSync.Get() || isRateCapped)
        && gpuMs < frameMs * 0.9f
        && cpuMs < frameMs * 0.9f;

    if (isFrameRateLocked)
    {
        m_boundStatusText->SetText("LOCKED");
    }
    else if (gpuMs >= cpuMs)
    {
        m_boundStatusText->SetText("GPU BOUND");
    }
    else
    {
        m_boundStatusText->SetText("CPU BOUND");
    }
}

#pragma endregion

} // namespace Hyperion
