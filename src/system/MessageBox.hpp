/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MESSAGE_BOX_HPP
#define HYPERION_MESSAGE_BOX_HPP

#include <core/Defines.hpp>

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>

#include <core/functional/Proc.hpp>

namespace hyperion {
namespace sys {

enum class MessageBoxType : int
{
    INFO        = 0,
    WARNING     = 1,
    CRITICAL    = 2
};

struct MessageBoxButton
{
    String      text;
    Proc<void()>  on_click;
};

class HYP_API SystemMessageBox
{
public:
    SystemMessageBox(MessageBoxType type);

    SystemMessageBox(
        MessageBoxType type,
        const String &title,
        const String &message,
        Array<MessageBoxButton> &&button = { }
    );

    SystemMessageBox(const SystemMessageBox &other)             = delete;
    SystemMessageBox &operator=(const SystemMessageBox &other)  = delete;

    SystemMessageBox(SystemMessageBox &&other) noexcept;
    SystemMessageBox &operator=(SystemMessageBox &&other) noexcept;

    ~SystemMessageBox();

    SystemMessageBox &Title(const String &title);
    SystemMessageBox &Text(const String &text);
    SystemMessageBox &Button(const String &text, Proc<void()> &&on_click);

    void Show() const;

private:
    MessageBoxType          m_type;
    String                  m_title;
    String                  m_message;
    Array<MessageBoxButton> m_buttons;
};

} // namespace sys

using sys::MessageBoxButton;
using sys::MessageBoxType;
using sys::SystemMessageBox;

} // namespace hyperion

#endif