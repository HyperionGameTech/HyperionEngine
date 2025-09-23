/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <ui/UIObject.hpp>

#include <core/memory/Pimpl.hpp>

#include <core/threading/Mutex.hpp>

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
    virtual void UpdateSize_Internal(bool updateChildren) override;
    
    virtual void Update_Internal(float delta) override;

    virtual bool NeedsUpdate() const override;

    virtual MaterialParameters GetMaterialParameters() const override;

    UIListView* m_historyListView;
    UITextbox* m_textbox;

    Pimpl<ConsoleHistory> m_history;

    String m_currentCommandText;

    int m_loggerRedirectId;
};

} // namespace hyperion

