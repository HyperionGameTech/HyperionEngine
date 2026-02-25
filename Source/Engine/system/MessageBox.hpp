/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/containers/String.hpp>
#include <Core/containers/Array.hpp>

#include <Core/functional/Proc.hpp>

namespace Hyperion {

enum class MessageBoxType : int
{
    INFO = 0,
    WARNING = 1,
    CRITICAL = 2
};

struct MessageBoxButton
{
    String text;
    Proc<void()> onClick;
};

class HYP_API SystemMessageBox
{
public:
    SystemMessageBox(MessageBoxType type);

    SystemMessageBox(
        MessageBoxType type,
        const String& title,
        const String& message,
        Array<MessageBoxButton>&& button = {});

    SystemMessageBox(const SystemMessageBox& other) = delete;
    SystemMessageBox& operator=(const SystemMessageBox& other) = delete;

    SystemMessageBox(SystemMessageBox&& other) noexcept;
    SystemMessageBox& operator=(SystemMessageBox&& other) noexcept;

    ~SystemMessageBox();

    SystemMessageBox& Title(const String& title);
    SystemMessageBox& Text(const String& text);
    SystemMessageBox& Button(const String& text, Proc<void()>&& onClick);

    void Show(bool showBlocking = true) const;

private:
    MessageBoxType m_type;
    String m_title;
    String m_message;
    Array<MessageBoxButton> m_buttons;
};

} // namespace Hyperion
