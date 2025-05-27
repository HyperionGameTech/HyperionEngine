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
    ConsoleHistory(const RC<UIDataSource>& data_source, int max_history_size = 100)
        : m_data_source(data_source),
          m_max_history_size(max_history_size)
    {
        m_entries.Reserve(m_max_history_size);
    }

    ~ConsoleHistory()
    {
    }

    HYP_FORCE_INLINE const RC<UIDataSource>& GetDataSource() const
    {
        return m_data_source;
    }

    void AddEntry(const String& text, ConsoleHistoryEntryType entry_type)
    {
        if (m_entries.Any() && int(m_entries.Size() + 1) > m_max_history_size)
        {
            ConsoleHistoryEntry* entry = &m_entries.Front();

            if (m_data_source)
            {
                m_data_source->Remove(entry->uuid);
            }

            m_entries.PopFront();
        }

        ConsoleHistoryEntry entry;
        entry.type = entry_type;
        entry.text = text;

        if (m_data_source)
        {
            m_data_source->Push(entry.uuid, HypData(entry), UUID::Invalid());
        }

        m_entries.PushBack(std::move(entry));
    }

    void ClearHistory()
    {
        m_entries.Clear();

        if (m_data_source)
        {
            m_data_source->Clear();
        }
    }

private:
    RC<UIDataSource> m_data_source;
    int m_max_history_size;
    Array<ConsoleHistoryEntry> m_entries;
};

#pragma endregion ConsoleHistory

#pragma region ConsoleUI

ConsoleUI::ConsoleUI()
    : UIObject(UIObjectType::OBJECT),
      m_logger_redirect_id(-1)
{
    SetBorderRadius(0);
    SetBorderFlags(UIObjectBorderFlags::ALL);
    SetPadding({ 5, 0 });
    SetBackgroundColor(Vec4f { 0.25f, 0.25f, 0.25f, 0.8f });
    SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    SetTextSize(12.0f);
    SetOriginAlignment(UIObjectAlignment::BOTTOM_LEFT);
    SetParentAlignment(UIObjectAlignment::BOTTOM_LEFT);

    m_logger_redirect_id = Logger::GetInstance().GetOutputStream()->AddRedirect(
        Log_Console.GetMaskBitset(),
        (void*)this,
        [](void* context, const LogChannel& channel, const LogMessage& message)
        {
            ConsoleUI* console_ui = (ConsoleUI*)context;

            if (console_ui->m_history)
            {
                console_ui->m_history->AddEntry(message.message, ConsoleHistoryEntryType::TEXT);
            }
        },
        [](void* context, const LogChannel& channel, const LogMessage& message)
        {
            ConsoleUI* console_ui = (ConsoleUI*)context;

            if (console_ui->m_history)
            {
                console_ui->m_history->AddEntry(message.message, ConsoleHistoryEntryType::ERR);
            }
        });
}

ConsoleUI::~ConsoleUI()
{
    if (m_logger_redirect_id != -1)
    {
        Logger::GetInstance().GetOutputStream()->RemoveRedirect(m_logger_redirect_id);
    }
}

void ConsoleUI::Init()
{
    UIObject::Init();

    OnMouseDown.Bind([this](const MouseEvent& event_data) -> UIEventHandlerResult
                   {
                       return UIEventHandlerResult::STOP_BUBBLING;
                   })
        .Detach();

    OnKeyDown.Bind([this](const KeyboardEvent& event_data) -> UIEventHandlerResult
                 {
                     return UIEventHandlerResult::STOP_BUBBLING;
                 })
        .Detach();

    OnKeyUp.Bind([this](const KeyboardEvent& event_data) -> UIEventHandlerResult
               {
                   return UIEventHandlerResult::STOP_BUBBLING;
               })
        .Detach();

    RC<UIDataSource> data_source = MakeRefCountedPtr<UIDataSource>(
        TypeWrapper<ConsoleHistoryEntry> {},
        [this](UIObject* parent, ConsoleHistoryEntry& value, const HypData& context) -> RC<UIObject>
        {
            RC<UIText> text = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));
            text->SetText(value.text);
            text->SetTextSize(12.0f);

            switch (value.type)
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
        [](UIObject* ui_object, ConsoleHistoryEntry& value, const HypData& context) -> void
        {
        });

    m_history = MakePimpl<ConsoleHistory>(data_source, 100);

    RC<UIListView> history_list_view = CreateUIObject<UIListView>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { GetActualSize().y - 30, UIObjectSize::PIXEL }));
    history_list_view->SetParentAlignment(UIObjectAlignment::TOP_LEFT);
    history_list_view->SetOriginAlignment(UIObjectAlignment::TOP_LEFT);
    history_list_view->SetBackgroundColor(Vec4f { 0.0f, 0.0f, 0.0f, 0.0f });
    history_list_view->SetInnerSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    history_list_view->SetDataSource(data_source);

    history_list_view->OnChildAttached.Bind([this](UIObject* child) -> UIEventHandlerResult
                                          {
                                              // m_history_list_view->SetScrollOffset(Vec2i {
                                              //     m_history_list_view->GetScrollOffset().x,
                                              //     m_history_list_view->GetActualInnerSize().y - m_history_list_view->GetActualSize().y
                                              // }, /* smooth */ false);

                                              return UIEventHandlerResult::STOP_BUBBLING;
                                          })
        .Detach();

    AddChildUIObject(history_list_view);

    m_history_list_view = history_list_view;

    m_history_list_view->OnSelectedItemChange.Bind([this](UIListViewItem* item) -> UIEventHandlerResult
                                                 {
                                                     if (item)
                                                     {
                                                         const ConsoleHistoryEntry& entry = m_history->GetDataSource()->Get(item->GetDataSourceElementUUID())->GetValue().Get<ConsoleHistoryEntry>();

                                                         m_textbox->SetText(entry.text);
                                                     }

                                                     return UIEventHandlerResult::STOP_BUBBLING;
                                                 })
        .Detach();

    RC<UITextbox> textbox = CreateUIObject<UITextbox>(Vec2i { 0, history_list_view->GetActualSize().y }, UIObjectSize({ 100, UIObjectSize::FILL }, { 25, UIObjectSize::PIXEL }));
    textbox->SetPlaceholder("Enter command");
    textbox->SetBackgroundColor(Vec4f { 0.0f, 0.0f, 0.0f, 0.5f });
    textbox->SetTextColor(Vec4f { 1.0f, 1.0f, 1.0f, 1.0f });
    textbox->SetTextSize(16.0f);
    AddChildUIObject(textbox);

    m_textbox = textbox;

    m_textbox->OnKeyDown.Bind([this](const KeyboardEvent& event_data) -> UIEventHandlerResult
                            {
                                if (event_data.key_code == KeyCode::RETURN)
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

                                        m_current_command_text.Clear();
                                        m_textbox->SetText("");
                                    }
                                }
                                else if (event_data.key_code == KeyCode::ARROW_UP)
                                {
                                    // @TODO Only cycle through COMMAND items..
                                    if (m_history_list_view && m_history_list_view->GetListViewItems().Any())
                                    {
                                        const int selected_item_index = m_history_list_view->GetSelectedItemIndex() - 1;

                                        if (selected_item_index >= 0 && selected_item_index < int(m_history_list_view->GetListViewItems().Size()))
                                        {
                                            m_history_list_view->SetSelectedItemIndex(selected_item_index);
                                        }
                                    }
                                }
                                else if (event_data.key_code == KeyCode::ARROW_DOWN)
                                {
                                    if (m_history_list_view && m_history_list_view->GetListViewItems().Any())
                                    {
                                        const int selected_item_index = m_history_list_view->GetSelectedItemIndex() + 1;

                                        if (selected_item_index < 0)
                                        {
                                            m_history_list_view->SetSelectedItemIndex(0);
                                        }
                                        else if (selected_item_index < int(m_history_list_view->GetListViewItems().Size()))
                                        {
                                            m_history_list_view->SetSelectedItemIndex(selected_item_index);
                                        }
                                        else
                                        {
                                            m_textbox->SetText(m_current_command_text);
                                        }
                                    }
                                }
                                else if (event_data.key_code == KeyCode::ESC)
                                {
                                    m_textbox->SetText("");
                                    m_current_command_text.Clear();

                                    // Lose focus of the console.
                                    Blur();
                                }
                                else
                                {
                                    m_current_command_text = m_textbox->GetText();
                                }

                                return UIEventHandlerResult::STOP_BUBBLING;
                            })
        .Detach();

    for (int i = 0; i < 10; i++)
    {
        HYP_LOG(Console, Info, "Console initialized {}", i);
    }
}

void ConsoleUI::UpdateSize_Internal(bool update_children)
{
    UIObject::UpdateSize_Internal(update_children);

    if (m_history_list_view)
    {
        m_history_list_view->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { GetActualSize().y - 30, UIObjectSize::PIXEL }));
    }

    if (m_textbox)
    {
        m_textbox->SetPosition(Vec2i { 0, m_history_list_view->GetActualSize().y });
        m_textbox->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 25, UIObjectSize::PIXEL }));
    }
}

Material::ParameterTable ConsoleUI::GetMaterialParameters() const
{
    return UIObject::GetMaterialParameters();
}

#pragma endregion ConsoleUI

} // namespace hyperion
