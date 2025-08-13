/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <console/ui/ConsoleUI.hpp>
#include <console/ConsoleCommandManager.hpp>

#include <ui/UIDataSource.hpp>
#include <ui/UITextbox.hpp>
#include <ui/UIListView.hpp>

#include <core/containers/Array.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);
HYP_DECLARE_LOG_CHANNEL(Console);

#pragma region ConsoleHistory

enum class ConsoleHistoryEntryType : uint32
{
    NONE,
    TEXT,
    COMMAND,
    ERR
};

struct ConsoleHistoryEntry
{
    UUID uuid;
    ConsoleHistoryEntryType type = ConsoleHistoryEntryType::NONE;
    String text;
};

class ConsoleHistory
{
public:
    ConsoleHistory(const Handle<UIDataSource>& dataSource, int maxHistorySize = 100)
        : m_dataSource(dataSource),
          m_maxHistorySize(maxHistorySize),
          m_numQueuedEntries(0)
    {
        m_entries.Reserve(m_maxHistorySize);
    }

    ~ConsoleHistory()
    {
    }

    HYP_FORCE_INLINE bool HasUpdates() const
    {
        return m_numQueuedEntries.Get(MemoryOrder::RELAXED) != 0;
    }

    HYP_FORCE_INLINE const Handle<UIDataSource>& GetDataSource() const
    {
        return m_dataSource;
    }

    // Must be called on parent UIObject's owner thread
    void SyncUpdates()
    {
        Array<ConsoleHistoryEntry> localQueuedEntries;

        {
            Mutex::Guard guard(m_queuedEntriesMutex);

            const uint32 numQueuedEntries = uint32(m_queuedEntries.Size());

            localQueuedEntries = std::move(m_queuedEntries);
            m_numQueuedEntries.Decrement(numQueuedEntries, MemoryOrder::RELEASE);
        }

        /// FIXME: This could be more efficient, if the entry list is too large we shouldn't push stuff just to remove it
        for (ConsoleHistoryEntry& entry : localQueuedEntries)
        {
            m_dataSource->Push(entry.uuid, HypData(entry), UUID::Invalid());
        }

        m_entries.Concat(std::move(localQueuedEntries));

        while (m_entries.Any() && int(m_entries.Size()) > m_maxHistorySize)
        {
            ConsoleHistoryEntry* entry = &m_entries.Front();

            if (m_dataSource)
            {
                m_dataSource->Remove(entry->uuid);
            }

            m_entries.PopFront();
        }
    }

    void AddEntry(const String& text, ConsoleHistoryEntryType entryType)
    {
        Mutex::Guard guard(m_queuedEntriesMutex);

        ConsoleHistoryEntry entry {};
        entry.type = entryType;
        entry.text = text;

        m_queuedEntries.PushBack(std::move(entry));
        m_numQueuedEntries.Increment(1, MemoryOrder::RELEASE);
    }

    // Must be called from owner thread of the UI Object
    void ClearHistory()
    {
        m_entries.Clear();

        if (m_dataSource)
        {
            m_dataSource->Clear();
        }
    }

private:
    Handle<UIDataSource> m_dataSource;
    int m_maxHistorySize;
    Array<ConsoleHistoryEntry> m_entries;

    Array<ConsoleHistoryEntry> m_queuedEntries;
    Mutex m_queuedEntriesMutex;
    AtomicVar<uint32> m_numQueuedEntries;
};

#pragma endregion ConsoleHistory

#pragma region ConsoleUI

ConsoleUI::ConsoleUI()
    : m_loggerRedirectId(-1),
      m_historyListView(nullptr),
      m_textbox(nullptr)
{
    SetBorderRadius(0);
    SetBorderFlags(UIObjectBorderFlags::ALL);
    SetPadding({ 5, 0 });
    SetBackgroundColor(Vec4f { 0.25f, 0.25f, 0.25f, 0.8f });
    SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    SetTextSize(12.0f);
    SetOriginAlignment(UIObjectAlignment::BOTTOM_LEFT);
    SetParentAlignment(UIObjectAlignment::BOTTOM_LEFT);

    m_loggerRedirectId = Logger::GetInstance().GetOutputStream()->AddRedirect(
        g_logChannel_Console.GetMaskBitset(),
        (void*)this,
        [](void* context, const LogChannel& channel, const LogMessage& message)
        {
            ConsoleUI* consoleUi = (ConsoleUI*)context;

            if (consoleUi->m_history)
            {
                String text;

                for (const auto& chunk : message.chunks)
                {
                    text.Append(chunk);
                }

                consoleUi->m_history->AddEntry(text, ConsoleHistoryEntryType::TEXT);
            }
        },
        [](void* context, const LogChannel& channel, const LogMessage& message)
        {
            ConsoleUI* consoleUi = (ConsoleUI*)context;

            if (consoleUi->m_history)
            {
                String text;

                for (const auto& chunk : message.chunks)
                {
                    text.Append(chunk);
                }

                consoleUi->m_history->AddEntry(text, ConsoleHistoryEntryType::ERR);
            }
        });
}

ConsoleUI::~ConsoleUI()
{
    if (m_loggerRedirectId != -1)
    {
        Logger::GetInstance().GetOutputStream()->RemoveRedirect(m_loggerRedirectId);
    }
}

void ConsoleUI::Init()
{
    UIObject::Init();

    OnMouseDown
        .Bind([this](const MouseEvent& eventData) -> UIEventHandlerResult
        {
            return UIEventHandlerResult::STOP_BUBBLING;
         })
        .Detach();

    OnKeyDown
        .Bind([this](const KeyboardEvent& eventData) -> UIEventHandlerResult
        {
            return UIEventHandlerResult::STOP_BUBBLING;
        })
        .Detach();

    OnKeyUp
        .Bind([this](const KeyboardEvent& eventData) -> UIEventHandlerResult
        {
            return UIEventHandlerResult::STOP_BUBBLING;
        })
        .Detach();

    Handle<UIDataSource> dataSource = CreateObject<UIDataSource>(
        UIElementFactoryRegistry::GetInstance().GetFactories<ConsoleHistoryEntry>(),
        [this](UIObject* parent, const HypData& value, const HypData& context) -> Handle<UIObject>
        {
            if (!value.Is<ConsoleHistoryEntry>())
            {
                return nullptr;
            }

            const ConsoleHistoryEntry& entry = value.Get<ConsoleHistoryEntry>();

            Handle<UIText> text = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));
            text->SetText(entry.text);
            text->SetTextSize(12.0f);

            switch (entry.type)
            {
            case ConsoleHistoryEntryType::COMMAND:
                text->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
                break;
            case ConsoleHistoryEntryType::TEXT:
                text->SetTextColor(Vec4f { 0.9f, 0.9f, 0.9f, 1.0f });
                break;
            case ConsoleHistoryEntryType::ERR:
                text->SetTextColor(Vec4f { 1.0f, 0.0f, 0.0f, 1.0f });
                break;
            default:
                text->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
                break;
            }

            return text;
        },
        [](UIObject* uiObject, const HypData& value, const HypData& context) -> void
        {
        });

    m_history = MakePimpl<ConsoleHistory>(dataSource, 100);

    Handle<UIListView> historyListView = CreateUIObject<UIListView>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { GetActualSize().y - 30, UIObjectSize::PIXEL }));
    historyListView->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    historyListView->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    historyListView->SetBackgroundColor(Vec4f { 0.0f, 0.0f, 0.0f, 0.0f });
    historyListView->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    historyListView->SetDataSource(dataSource);

    historyListView->OnChildAttached
        .Bind([this](UIObject* child) -> UIEventHandlerResult
            {
                m_historyListView->ScrollToChild(child);

                return UIEventHandlerResult::STOP_BUBBLING;
            })
        .Detach();

    AddChildUIObject(historyListView);

    m_historyListView = historyListView;

    m_historyListView->OnSelectedItemChange
        .Bind([this](UIListViewItem* item) -> UIEventHandlerResult
            {
                if (item)
                {
                    const ConsoleHistoryEntry& entry = m_history->GetDataSource()->Get(item->GetDataSourceElementUUID())->GetValue().Get<ConsoleHistoryEntry>();

                    m_textbox->SetText(entry.text);
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            })
        .Detach();

    Handle<UITextbox> textbox = CreateUIObject<UITextbox>(Vec2i { 0, historyListView->GetActualSize().y }, UIObjectSize({ 100, UIObjectSize::FILL }, { 25, UIObjectSize::PIXEL }));
    textbox->SetPlaceholder("Enter command");
    textbox->SetBackgroundColor(Vec4f { 0.0f, 0.0f, 0.0f, 0.5f });
    textbox->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    textbox->SetTextSize(16.0f);
    AddChildUIObject(textbox);

    m_textbox = textbox;

    m_textbox->OnKeyDown
        .Bind([this](const KeyboardEvent& eventData) -> UIEventHandlerResult
            {
                if (eventData.keyCode == KeyCode::RETURN)
                {
                    const String& text = m_textbox->GetText();

                    if (text.Any())
                    {
                        if (text.ToLower() == "clear")
                        {
                            m_history->ClearHistory();
                        }
                        else
                        {
                            m_history->AddEntry(text, ConsoleHistoryEntryType::COMMAND);
                        }

                        if (Result result = ConsoleCommandManager::GetInstance().ExecuteCommand(text); result.HasError())
                        {
                            HYP_LOG(Console, Error, "Error executing command: {}", result.GetError().GetMessage());
                        }
                        else
                        {
                            HYP_LOG(Console, Info, "Executed command: {}", text);
                        }

                        m_currentCommandText.Clear();
                        m_textbox->SetText("");
                    }
                }
                else if (eventData.keyCode == KeyCode::ARROW_UP)
                {
                    // @TODO Only cycle through COMMAND items..
                    if (m_historyListView && m_historyListView->GetListViewItems().Any())
                    {
                        const int selectedItemIndex = m_historyListView->GetSelectedItemIndex() - 1;

                        if (selectedItemIndex >= 0 && selectedItemIndex < int(m_historyListView->GetListViewItems().Size()))
                        {
                            m_historyListView->SetSelectedItemIndex(selectedItemIndex);
                        }
                    }
                }
                else if (eventData.keyCode == KeyCode::ARROW_DOWN)
                {
                    if (m_historyListView && m_historyListView->GetListViewItems().Any())
                    {
                        const int selectedItemIndex = m_historyListView->GetSelectedItemIndex() + 1;

                        if (selectedItemIndex < 0)
                        {
                            m_historyListView->SetSelectedItemIndex(0);
                        }
                        else if (selectedItemIndex < int(m_historyListView->GetListViewItems().Size()))
                        {
                            m_historyListView->SetSelectedItemIndex(selectedItemIndex);
                        }
                        else
                        {
                            m_textbox->SetText(m_currentCommandText);
                        }
                    }
                }
                else if (eventData.keyCode == KeyCode::ESC)
                {
                    m_textbox->SetText("");
                    m_currentCommandText.Clear();

                    // Lose focus of the console.
                    Blur();
                }
                else
                {
                    m_currentCommandText = m_textbox->GetText();
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            })
        .Detach();
}

void ConsoleUI::UpdateSize_Internal(bool updateChildren)
{
    UIObject::UpdateSize_Internal(updateChildren);

    if (m_historyListView)
    {
        m_historyListView->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { GetActualSize().y - 30, UIObjectSize::PIXEL }));
    }

    if (m_textbox)
    {
        m_textbox->SetPosition(Vec2i { 0, m_historyListView->GetActualSize().y });
        m_textbox->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 25, UIObjectSize::PIXEL }));
    }
}

void ConsoleUI::Update_Internal(float delta)
{
    UIObject::Update_Internal(delta);

    m_history->SyncUpdates();
}

bool ConsoleUI::NeedsUpdate() const
{
    if (UIObject::NeedsUpdate())
    {
        return true;
    }

    if (m_history != nullptr)
    {
        return m_history->HasUpdates();
    }

    return false;
}

Material::ParameterTable ConsoleUI::GetMaterialParameters() const
{
    return UIObject::GetMaterialParameters();
}

#pragma endregion ConsoleUI

class UIElementFactory_ConsoleHistoryEntry : public UIElementFactory<ConsoleHistoryEntry>
{
public:
    Handle<UIObject> Create(UIObject* parent, const ConsoleHistoryEntry& value) const
    {
        Handle<UIText> text = parent->CreateUIObject<UIText>();
        text->SetText(value.text);

        parent->Node::AddTag(NodeTag(NAME("ConsoleHistoryEntry"), value.uuid));

        return text;
    }

    void Update(UIObject* uiObject, const ConsoleHistoryEntry& value) const
    {
        uiObject->SetText(value.text);
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(ConsoleHistoryEntry, UIElementFactory_ConsoleHistoryEntry);

} // namespace hyperion
