/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <UIPch.hpp>

#include <ui/overlays/BaseStatsOverlay.hpp>

#include <ui/UIListView.hpp>
#include <ui/UIText.hpp>

#include <scene/World.hpp>

#include <Framework/EngineStats.hpp>

#include <BaseStatsOverlay.generated.inl>

namespace Hyperion {

extern EngineStatCounter<uint32> g_statDrawCalls;
extern EngineStatCounter<uint32> g_statInstancedDrawCalls;
extern EngineStatCounter<uint32> g_statTriangles;
extern EngineStatCounter<uint32> g_statRenderGroups;
extern EngineStatCounter<uint32> g_statViews;
extern EngineStatCounter<uint32> g_statMaterials;
extern EngineStatCounter<uint32> g_statTextures;
extern EngineStatCounter<uint32> g_statLights;
extern EngineStatCounter<uint32> g_statLightmapVolumes;
extern EngineStatCounter<uint32> g_statParticleVolumes;
extern EngineStatCounter<uint32> g_statEnvProbes;
extern EngineStatCounter<uint32> g_statEnvGrids;
extern EngineStatCounter<uint32> g_statDebugDraws;

extern EngineStatTimer g_statSimUpdate;
extern EngineStatTimer g_statRenderUpdate;
extern EngineStatTimer g_statRenderThreadSync;
extern EngineStatTimer g_statScriptUpdate;

#pragma region BaseStatsOverlay

const Array<Pair<int, Color>> BaseStatsOverlay::s_fpsColors = {
    { 30, Color(1.0f, 0.0f, 0.0f, 1.0f) },
    { 60, Color(1.0f, 1.0f, 0.0f, 1.0f) },
    { INT32_MAX, Color(0.0f, 1.0f, 0.0f, 1.0f) }
};

BaseStatsOverlay::BaseStatsOverlay()
    : m_deltaAccumGame(0.0f),
      m_numTicksGame(0)
{
    m_timer = ClockTimer { 0.0333f }; // update max. 30hz/s
}

BaseStatsOverlay::~BaseStatsOverlay() = default;

Handle<UIObject> BaseStatsOverlay::CreateUIObject_Impl(UIObject* spawnParent)
{
    HYP_SCOPE;

    Handle<UIListView> panel = spawnParent->CreateUIObject<UIListView>(
        NAME("FpsCounter_Panel"),
        Vec2i(0, 0),
        UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

    panel->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));

    Handle<UIListView> renderListView = spawnParent->CreateUIObject<UIListView>(
        NAME("FpsCounter_RenderListView"),
        Vec2i(0, 0),
        UIObjectSize(UIObjectSize::AUTO));

    renderListView->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
    renderListView->SetOrientation(UIListViewOrientation::HORIZONTAL);;

    Handle<UIText> fpsTextElement = renderListView->CreateUIObject<UIText>(
        NAME("FpsCounter_Fps"),
        Vec2i(0, 0),
        UIObjectSize(UIObjectSize::AUTO));

    fpsTextElement->SetText("0 fps, 0.00 ms/frame (avg: 0.00, min: 0.00, max: 0.00)");
    fpsTextElement->SetTextColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    fpsTextElement->SetPadding(Vec2i(5, 5));
    renderListView->AddChildUIObject(fpsTextElement);
    m_fpsTextElement = fpsTextElement;

    Handle<UIText> countersTextElement = renderListView->CreateUIObject<UIText>(
        NAME("FpsCounter_Counters"),
        Vec2i(0, 0),
        UIObjectSize(UIObjectSize::AUTO));

    countersTextElement->SetText("draw calls: 0, Tris: 0");
    countersTextElement->SetTextColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    countersTextElement->SetPadding(Vec2i(5, 5));
    renderListView->AddChildUIObject(countersTextElement);
    m_countersTextElement = countersTextElement;

    Handle<UIText> memoryUsageTextElement = renderListView->CreateUIObject<UIText>(
        NAME("FpsCounter_MemoryUsage"),
        Vec2i(0, 0),
        UIObjectSize(UIObjectSize::AUTO));

    memoryUsageTextElement->SetText(".NET Memory Usage: 0 MB");
    memoryUsageTextElement->SetTextColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    memoryUsageTextElement->SetPadding(Vec2i(5, 5));
    renderListView->AddChildUIObject(memoryUsageTextElement);
    m_memoryUsageTextElement = memoryUsageTextElement;

    panel->AddChildUIObject(renderListView);

    return panel;
}

void BaseStatsOverlay::Update_Impl(float delta)
{
    HYP_SCOPE;

    m_deltaAccumGame += delta;
    m_numTicksGame++;

    if (m_deltaAccumGame >= 1.0f)
    {
        if (m_memoryUsageTextElement.IsValid())
        {
            /// \todo
        }

        m_deltaAccumGame = 0.0f;
        m_numTicksGame = 0;
    }

    const EngineStatsSnapshot& snapshot = g_engineStats->GetCurrentSnapshot();

    if (m_fpsTextElement.IsValid())
    {
        // Display average FPS for smoother reading, with instantaneous ms/frame
        const int avgFps = int(snapshot[StatIdFps].avg);

        m_fpsTextElement->SetText(HYP_FORMAT(
            "{} fps, {} ms/frame (avg: {}, min: {}, max: {})  Sim: {}ms  Render: {}ms",
            avgFps,
            snapshot[StatIdMsPerFrame].value,
            snapshot[StatIdMsPerFrame].avg,
            snapshot[StatIdMsPerFrame].min,
            snapshot[StatIdMsPerFrame].max,
            MathUtil::Round(g_statSimUpdate.GetValue(), 2),
            MathUtil::Round(g_statRenderUpdate.GetValue(), 2)));

        m_fpsTextElement->SetTextColor(GetFpsColor(avgFps));
    }

    if (m_countersTextElement.IsValid())
    {
        String countersText;
        countersText += HYP_FORMAT("Tris: {}", uint64(snapshot[g_statTriangles].value));
        countersText += HYP_FORMAT(", DrawCalls: {}", snapshot[g_statDrawCalls].value);

        if (snapshot[g_statInstancedDrawCalls].value > 0)
        {
            countersText += HYP_FORMAT(", Instanced: {}", snapshot[g_statInstancedDrawCalls].value);
        }

        /*if (snapshot[g_statDebugDraws].value > 0)
        {
            countersText += HYP_FORMAT(", DebugDraw: {}", snapshot[g_statDebugDraws].value);
        }*/

        /*countersText += HYP_FORMAT(", Tris: {}", uint64(snapshot[g_statTriangles].value));
        countersText += HYP_FORMAT(", RenderGroups: {}", snapshot[g_statRenderGroups].value);
        countersText += HYP_FORMAT(", Views: {}", snapshot[g_statViews].value);
        countersText += HYP_FORMAT(", Textures: {}", snapshot[g_statTextures].value);
        countersText += HYP_FORMAT(", Materials: {}", snapshot[g_statMaterials].value);

        if (snapshot[g_statLights].value > 0)
        {
            countersText += HYP_FORMAT(", Lights: {}", snapshot[g_statLights].value);
        }

        if (snapshot[g_statLightmapVolumes].value > 0)
        {
            countersText += HYP_FORMAT(", LightmapVolumes: {}", snapshot[g_statLightmapVolumes].value);
        }

        if (snapshot[g_statParticleVolumes].value > 0)
        {
            countersText += HYP_FORMAT(", ParticleVolumes: {}", snapshot[g_statParticleVolumes].value);
        }

        if (snapshot[g_statEnvProbes].value > 0)
        {
            countersText += HYP_FORMAT(", EnvProbes: {}", snapshot[g_statEnvProbes].value);
        }*/

        m_countersTextElement->SetText(countersText);
    }
}

Color BaseStatsOverlay::GetFpsColor(int fps)
{
    for (const Pair<int, Color>& pair : s_fpsColors)
    {
        if (fps <= pair.first)
        {
            return pair.second;
        }
    }

    return s_fpsColors.Back().second;
}

#pragma endregion BaseStatsOverlay

} // namespace Hyperion
