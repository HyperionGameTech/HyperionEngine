/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CONSOLE_UI_HPP
#define HYPERION_CONSOLE_UI_HPP

#include <ui/UIObject.hpp>

#include <core/memory/Pimpl.hpp>

namespace hyperion {

class UITextbox;
class UIListView;

class ConsoleHistory;

HYP_CLASS()
class HYP_API ConsoleUI : public UIObject
{
    HYP_OBJECT_BODY(ConsoleUI);

public:
    ConsoleUI();
    ConsoleUI(const ConsoleUI& other) = delete;
    ConsoleUI& operator=(const ConsoleUI& other) = delete;
    ConsoleUI(ConsoleUI&& other) noexcept = delete;
    ConsoleUI& operator=(ConsoleUI&& other) noexcept = delete;
    virtual ~ConsoleUI() override;

protected:
    virtual void Init() override;
    virtual void UpdateSize_Internal(bool update_children) override;

    virtual Material::ParameterTable GetMaterialParameters() const override;

    Handle<UIListView> m_history_list_view;
    Handle<UITextbox> m_textbox;

    Pimpl<ConsoleHistory> m_history;

    String m_current_command_text;

    int m_logger_redirect_id;
};

} // namespace hyperion

#endif