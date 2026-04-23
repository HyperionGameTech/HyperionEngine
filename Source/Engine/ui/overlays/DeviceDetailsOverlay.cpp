/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <UIPch.hpp>

#include <ui/overlays/DeviceDetailsOverlay.hpp>

#include <ui/UIListView.hpp>
#include <ui/UIText.hpp>

#include <engine/DeviceDetails.hpp>
#include <engine/EngineStats.hpp>
#include <engine/EngineGlobals.hpp>

#include <rendering/RenderInterface.hpp>

#include <Core/math/MathUtil.hpp>

#include <DeviceDetailsOverlay.generated.inl>

namespace Hyperion {

#pragma region DeviceDetailsOverlay

DeviceDetailsOverlay::DeviceDetailsOverlay()
    : OverlayBase()
{
}

DeviceDetailsOverlay::~DeviceDetailsOverlay() = default;

Handle<UIObject> DeviceDetailsOverlay::CreateUIObject_Impl(UIObject* spawnParent)
{
    HYP_SCOPE;

    DeviceDetails& device = g_renderInterface->deviceDetails;

    Handle<UIPanel> panelBackdrop = spawnParent->CreateUIObject<UIPanel>(
        NAME_FMT("DeviceDetailsOverlay_PanelBackdrop"),
        Vec2i(2, 2),
        UIObjectSize({ 450, UIObjectSize::PIXEL }, { 30, UIObjectSize::PIXEL }));

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

    m_gpuModelText = m_panel->CreateUIObject<UIText>(
        Vec2i::Zero(),
        UIObjectSize(UIObjectSize::AUTO));

    m_gpuModelText->SetTextSize(13.0f);
    m_gpuModelText->SetTextColor(Color(0.9f, 0.9f, 0.9f, 1.0f));
    m_gpuModelText->SetPadding(Vec2i(10, 0));
    m_gpuModelText->SetText(device.GetGpuModel());
    m_panel->AddChildUIObject(m_gpuModelText);

    m_gpuVendorText = m_panel->CreateUIObject<UIText>(
        Vec2i::Zero(),
        UIObjectSize(UIObjectSize::AUTO));

    m_gpuVendorText->SetTextSize(13.0f);
    m_gpuVendorText->SetTextColor(Color(0.7f, 0.7f, 0.7f, 1.0f));
    m_gpuVendorText->SetPadding(Vec2i(10, 0));
    m_gpuVendorText->SetText(device.GetGpuVendorName());
    m_panel->AddChildUIObject(m_gpuVendorText);

    m_gpuTypeText = m_panel->CreateUIObject<UIText>(
        Vec2i::Zero(),
        UIObjectSize(UIObjectSize::AUTO));

    m_gpuTypeText->SetTextSize(13.0f);
    m_gpuTypeText->SetTextColor(Color(0.7f, 0.7f, 0.7f, 1.0f));
    m_gpuTypeText->SetPadding(Vec2i(10, 0));
    m_gpuTypeText->SetText(device.IsDiscreteGpu() ? String("Dedicated") : String("Integrated"));
    m_panel->AddChildUIObject(m_gpuTypeText);

    panelBackdrop->AddChildUIObject(m_panel);

    return panelBackdrop;
}

void DeviceDetailsOverlay::Update_Impl(float delta)
{
    HYP_SCOPE;

    const EngineStats& s_stats = *g_engineStats;
    const EngineStatsSnapshot& snapshot = s_stats.GetCurrentSnapshot();

    const int fps = MathUtil::Floor(snapshot[StatIdFps].avg);
    
    char fpsStr[8] = { ' ', ' ', ' ', ' ', 'F', 'P', 'S', '\0' };
    fpsStr[0] = char('0' + (fps / 100) % 10);
    fpsStr[1] = char('0' + (fps / 10) % 10);
    fpsStr[2] = char('0' + fps % 10);
    
    m_fpsText->SetText(String(fpsStr));
}

#pragma endregion

} // namespace Hyperion