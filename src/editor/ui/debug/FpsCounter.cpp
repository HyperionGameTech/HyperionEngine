/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <editor/ui/debug/FpsCounter.hpp>

#include <ui/UIListView.hpp>
#include <ui/UIText.hpp>

#include <scene/World.hpp>

#include <rendering/RenderStats.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

const Array<Pair<int, Color>> FpsCounter::s_fpsColors = {
    { 30, Color(1.0f, 0.0f, 0.0f, 1.0f) },
    { 60, Color(1.0f, 1.0f, 0.0f, 1.0f) },
    { INT32_MAX, Color(0.0f, 1.0f, 0.0f, 1.0f) }
};

FpsCounter::FpsCounter()
    : m_world(nullptr),
      m_deltaAccumGame(0.0f),
      m_numTicksGame(0)
{
}

FpsCounter::FpsCounter(World* world)
    : m_world(world),
      m_deltaAccumGame(0.0f),
      m_numTicksGame(0)
{
}

FpsCounter::~FpsCounter() = default;

Handle<UIObject> FpsCounter::CreateUIObject_Impl(UIObject* spawnParent)
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

void FpsCounter::Update_Impl(float delta)
{
    HYP_SCOPE;

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

    if (m_world == nullptr)
    {
        return;
    }

    const RenderStats* renderStats = m_world->GetRenderStats();
    if (renderStats == nullptr)
    {
        return;
    }

    if (m_fpsTextElement.IsValid())
    {
        // @TODO: Round to two decimal places when added to HYP_FORMAT
        m_fpsTextElement->SetText(HYP_FORMAT(
            "{} fps, {} ms/frame (avg: {}, min: {}, max: {})",
            int(renderStats->framesPerSecond),
            renderStats->millisecondsPerFrame,
            renderStats->millisecondsPerFrameAvg,
            renderStats->millisecondsPerFrameMin,
            renderStats->millisecondsPerFrameMax));

        m_fpsTextElement->SetTextColor(GetFpsColor(int(renderStats->framesPerSecond)));
    }

    if (m_countersTextElement.IsValid())
    {
        String countersText;
        countersText += HYP_FORMAT("DrawCalls: {}", renderStats->counts[ERS_DRAW_CALLS]);

        if (renderStats->counts[ERS_INSTANCED_DRAW_CALLS] > 0)
        {
            countersText += HYP_FORMAT(", Instanced: {}", renderStats->counts[ERS_INSTANCED_DRAW_CALLS]);
        }

        if (renderStats->counts[ERS_DEBUG_DRAWS] > 0)
        {
            countersText += HYP_FORMAT(", DebugDraw: {}", renderStats->counts[ERS_DEBUG_DRAWS]);
        }

        countersText += HYP_FORMAT(", Tris: {}", renderStats->counts[ERS_TRIANGLES]);
        countersText += HYP_FORMAT(", Groups: {}", renderStats->counts[ERS_RENDER_GROUPS]);
        countersText += HYP_FORMAT(", Views: {}", renderStats->counts[ERS_VIEWS]);
        countersText += HYP_FORMAT(", Textures: {}", renderStats->counts[ERS_TEXTURES]);
        countersText += HYP_FORMAT(", Materials: {}", renderStats->counts[ERS_MATERIALS]);

        if (renderStats->counts[ERS_LIGHTS] > 0)
        {
            countersText += HYP_FORMAT(", Lights: {}", renderStats->counts[ERS_LIGHTS]);
        }

        if (renderStats->counts[ERS_LIGHTMAP_VOLUMES] > 0)
        {
            countersText += HYP_FORMAT(", LightmapVolumes: {}", renderStats->counts[ERS_LIGHTMAP_VOLUMES]);
        }

        if (renderStats->counts[ERS_ENV_PROBES] > 0)
        {
            countersText += HYP_FORMAT(", EnvProbes: {}", renderStats->counts[ERS_ENV_PROBES]);
        }

        m_countersTextElement->SetText(countersText);
    }
}

Color FpsCounter::GetFpsColor(int fps)
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

// StatOverlay implementation

StatOverlay::StatOverlay() = default;

StatOverlay::~StatOverlay() = default;

Handle<UIObject> StatOverlay::CreateUIObject_Impl(UIObject* spawnParent)
{
    HYP_SCOPE;

    Handle<UIListView> panel = spawnParent->CreateUIObject<UIListView>(
        NAME("StatOverlay_Panel"),
        Vec2i(0, 0),
        UIObjectSize(UIObjectSize::AUTO));

    panel->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.5f));
    panel->SetTextSize(9);
    panel->SetPadding(Vec2i(10, 10));

    return panel;
}

void StatOverlay::Update_Impl(float delta)
{
    HYP_SCOPE;

    // @TODO - update stats items for each stat group
}

} // namespace hyperion
