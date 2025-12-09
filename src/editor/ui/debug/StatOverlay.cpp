/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <editor/ui/debug/StatOverlay.hpp>

#include <ui/UIListView.hpp>
#include <ui/UIText.hpp>

#include <scene/World.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineStats.hpp>

#include <StatOverlay.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

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

extern EngineStatTimer g_gameThreadUpdateTimer;
extern EngineStatTimer g_renderThreadUpdateTimer;
extern EngineStatTimer g_renderCpuSyncTimer;
extern EngineStatTimer g_scriptUpdateTimer;

#pragma region StatOverlay

const Array<Pair<int, Color>> StatOverlay::s_fpsColors = {
    { 30, Color(1.0f, 0.0f, 0.0f, 1.0f) },
    { 60, Color(1.0f, 1.0f, 0.0f, 1.0f) },
    { INT32_MAX, Color(0.0f, 1.0f, 0.0f, 1.0f) }
};

StatOverlay::StatOverlay()
    : m_deltaAccumGame(0.0f),
      m_numTicksGame(0)
{
}

StatOverlay::~StatOverlay() = default;

Handle<UIObject> StatOverlay::CreateUIObject_Impl(UIObject* spawnParent)
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
    renderListView->SetOrientation(UIListViewOrientation::HORIZONTAL);
    renderListView->SetTextSize(8);

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

void StatOverlay::Update_Impl(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    m_deltaAccumGame += delta;
    m_numTicksGame++;

    if (m_deltaAccumGame >= 1.0f)
    {
        if (m_memoryUsageTextElement.IsValid())
        {
            // @TODO
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
            "{} fps, {} ms/frame (avg: {}, min: {}, max: {})  GT: {}ms  RT: {}ms  RT(sync): {}ms  SCR: {}ms",
            avgFps,
            snapshot[StatIdMsPerFrame].value,
            snapshot[StatIdMsPerFrame].avg,
            snapshot[StatIdMsPerFrame].min,
            snapshot[StatIdMsPerFrame].max,
            g_gameThreadUpdateTimer.GetValue(),
            g_renderThreadUpdateTimer.GetValue(),
            g_renderCpuSyncTimer.GetValue(),
            g_scriptUpdateTimer.GetValue()));

        m_fpsTextElement->SetTextColor(GetFpsColor(avgFps));
    }

    if (m_countersTextElement.IsValid())
    {
        String countersText;
        countersText += HYP_FORMAT("DrawCalls: {}", snapshot[g_statDrawCalls].value);

        if (snapshot[g_statInstancedDrawCalls].value > 0)
        {
            countersText += HYP_FORMAT(", Instanced: {}", snapshot[g_statInstancedDrawCalls].value);
        }

        if (snapshot[g_statDebugDraws].value > 0)
        {
            countersText += HYP_FORMAT(", DebugDraw: {}", snapshot[g_statDebugDraws].value);
        }

        countersText += HYP_FORMAT(", Tris: {}", snapshot[g_statTriangles].value);
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
        }

        m_countersTextElement->SetText(countersText);
    }
}

Color StatOverlay::GetFpsColor(int fps)
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

#pragma endregion StatOverlay

} // namespace hyperion
