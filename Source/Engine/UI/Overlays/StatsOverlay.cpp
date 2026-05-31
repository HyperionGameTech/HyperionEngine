/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <UIPch.hpp>

#include <UI/Overlays/StatsOverlay.hpp>

#include <UI/UIButton.hpp>
#include <UI/UIListView.hpp>
#include <UI/UIText.hpp>
#include <UI/UIDataSource.hpp>

#include <Scene/World.hpp>

#include <Framework/EngineStats.hpp>

#include <Core/Profiling/ProfileScope.hpp>
#include <Core/Core.hpp>

#include <StatsOverlay.generated.inl>

namespace Hyperion {

#pragma region StatsOverlay

StatsOverlay::StatsOverlay()
    : OverlayBase()
{
    m_timer = ClockTimer { 0.0333f }; // update max. 30hz/s
    m_hotFunctionsUpdateTimer = ClockTimer { 1.0f };
}

StatsOverlay::~StatsOverlay() = default;

Handle<UIObject> StatsOverlay::CreateUIObject_Impl(UIObject* spawnParent)
{
    HYP_SCOPE;

    Handle<UIPanel> panelBackdrop = spawnParent->CreateUIObject<UIPanel>(
        NAME_FMT("StatsOverlay_PanelBackdrop"),
        Vec2i(2, 2),
        UIObjectSize({ 250, UIObjectSize::PIXEL }, { 300, UIObjectSize::PIXEL }));

    panelBackdrop->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.8f));
    panelBackdrop->SetPadding(Vec2i(10, 10));
    panelBackdrop->SetBorderRadius(10);

    m_panel = spawnParent->CreateUIObject<UIListView>(
        NAME_FMT("StatsOverlay_Panel"),
        Vec2i::Zero(),
        UIObjectSize(100, UIObjectSize::PERCENT));

    m_panel->SetTextSize(14.0f);
    m_panel->SetTextColor(Color(0.7f, 0.7f, 0.7f, 1.0f));
    m_panel->SetBackgroundColor(Color::Transparent());
    m_panel->SetPadding(Vec2i(2, 2));

    m_dataSource = MakeHandle<UIDataSource>(
        Array<Handle<UIElementFactoryBase>> {},
        [](UIObject* parent, const BoxedValue& value, const BoxedValue& context) -> Handle<UIObject>
        {
            if (value.Is<Name>())
            {
                Handle<UIPanel> textPanel = parent->CreateUIObject<UIPanel>(
                    Vec2i::Zero(),
                    UIObjectSize({ 100, UIObjectSize::PERCENT }, { 25, UIObjectSize::PIXEL }));

                textPanel->SetBackgroundColor(Color::Transparent());

                // heading
                Handle<UIText> text = parent->CreateUIObject<UIText>(
                    Vec2i::Zero(),
                    UIObjectSize(Vec2i::Zero(), UIObjectSize::AUTO));

                text->SetTextColor(Color(0.9f, 0.9f, 0.9f, 1.0f));
                text->SetText(*value.Get<Name>());
                text->SetOriginAlignment(UIObjectAlignment::CENTER);
                text->SetParentAlignment(UIObjectAlignment::CENTER);

                textPanel->AddChildUIObject(text);

                return textPanel;
            }
            else
            {
                Handle<UIText> text = parent->CreateUIObject<UIText>(
                    Vec2i::Zero(),
                    UIObjectSize(Vec2i::Zero(), UIObjectSize::AUTO));

                text->SetPadding(Vec2i(1, 1));
                text->SetText(value.Get<String>());

                return text;
            }
        },
        [](UIObject* uiObject, const BoxedValue& value, const BoxedValue& context)
        {
            uiObject->SetText(value.Get<String>());
        });

    m_panel->SetDataSource(m_dataSource);

    panelBackdrop->AddChildUIObject(m_panel);

#if HYP_ENABLE_PROFILE
    if (CoreApi::IsProfilingEnabled())
    {
        // if profiling is enabled, also set up a panel for hot functions.
        // make the main panel smaller to make room for the hot functions panel.
        m_panel->SetSize(UIObjectSize({ 50, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));
        panelBackdrop->SetSize(UIObjectSize({ 500, UIObjectSize::PIXEL }, { 300, UIObjectSize::PIXEL }));

        // set up panel showing hot functions.
        m_hotFunctionsPanel = spawnParent->CreateUIObject<UIListView>(
            NAME("StatsOverlay_HotFunctionsPanel"),
            Vec2i(250, 0),
            UIObjectSize({ 50, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));

        m_hotFunctionsPanel->SetTextSize(14.0f);
        m_hotFunctionsPanel->SetTextColor(Color(0.7f, 0.7f, 0.7f, 1.0f));
        m_hotFunctionsPanel->SetBackgroundColor(Color::Transparent());
        m_hotFunctionsPanel->SetPadding(Vec2i(2, 2));

        m_hotFunctionsText = m_hotFunctionsPanel->CreateUIObject<UIText>();
        m_hotFunctionsPanel->AddChildUIObject(m_hotFunctionsText);

        panelBackdrop->AddChildUIObject(m_hotFunctionsPanel);
    }
#endif

    return panelBackdrop;
}

void StatsOverlay::Update_Impl(float delta)
{
    HYP_SCOPE;

#if HYP_ENABLE_PROFILE
    if (CoreApi::IsProfilingEnabled())
    {
        if (!m_hotFunctionsUpdateTimer.Waiting())
        {
            m_hotFunctionsUpdateTimer.NextTick();

            Array<Pair<ANSIString, double>> hotFunctions;
            CollectAllHotFunctions(hotFunctions);

            String allHotFunctionsText;

            for (const Pair<ANSIString, double>& pair : hotFunctions)
            {
                allHotFunctionsText += HYP_FORMAT("{} : {}ms", pair.first, pair.second) + "\n";
            }

            m_hotFunctionsText->SetText(allHotFunctionsText);
        }
    }
#endif

    const Handle<EngineStats>& engineStats = EngineStats::GetInstance();
    const EngineStatsSnapshot& snapshot = engineStats->GetCurrentSnapshot();

    using ProcessGroupFuncRef = ProcRef<void(const EngineStatGroup&)>;
    ProcessGroupFuncRef ProcessGroup = nullptr;

    auto ProcessGroupImpl = [this, &ProcessGroup, &snapshot](const EngineStatGroup& group) -> void
    {
        for (EngineStatBase* stat : group.stats)
        {
            auto it = m_statUuids.Find(stat);

            if (stat->type == EST_GROUP)
            {
                if (it == m_statUuids.End())
                {
                    UUID groupUuid = UUID();
                    m_statUuids.Set(stat, groupUuid);
                    m_dataSource->Push(groupUuid, BoxedValue(stat->name), UUID::Invalid());
                }

                ProcessGroup(static_cast<const EngineStatGroup&>(*stat));
            }
            else
            {
                String statText = HYP_FORMAT("{}: {}", stat->name, snapshot[*stat].value);

                if (it == m_statUuids.End())
                {
                    UUID statUuid = UUID();
                    m_statUuids.Set(stat, statUuid);
                    m_dataSource->Push(statUuid, BoxedValue(std::move(statText)), UUID::Invalid());
                }
                else
                {
                    m_dataSource->Set(it->second, BoxedValue(std::move(statText)));
                }
            }
        }
    };

    ProcessGroup = ProcessGroupImpl;

    ProcessGroup(*engineStats->root);
}

#pragma endregion StatsOverlay

} // namespace Hyperion
