/* Copyright (c) 2016 - 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <ui/UIObject.hpp>

#include <core/memory/Pimpl.hpp>

#include <core/threading/Mutex.hpp>

namespace Hyperion {

class UITextbox;
class UIListView;

class ConsoleHistory;

HYP_CLASS()
class HYP_API UIConsole : public UIObject
{
    HYP_OBJECT_BODY(UIConsole);

public:
    UIConsole();
    
    UIConsole(const UIConsole& other) = delete;
    UIConsole& operator=(const UIConsole& other) = delete;
    
    UIConsole(UIConsole&& other) noexcept = delete;
    UIConsole& operator=(UIConsole&& other) noexcept = delete;

    virtual ~UIConsole() override;

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

} // namespace Hyperion
