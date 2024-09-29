/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/system/MessageBox.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

extern "C" {
extern int ShowMessageBox(int type, const char *title, const char *message, int buttons, const char *buttonTexts[3]);
}

namespace hyperion {
namespace sys {

SystemMessageBox::SystemMessageBox(MessageBoxType type)
    : m_type(type)
{
}

SystemMessageBox::SystemMessageBox(
    MessageBoxType type,
    const String &title,
    const String &message,
    Array<MessageBoxButton> &&buttons
) : m_type(type),
    m_title(title),
    m_message(message),
    m_buttons(std::move(buttons))
{
    if (m_buttons.Size() > 3) {
        HYP_LOG(Core, LogLevel::WARNING, "MessageBox does not support > 3 buttons");

        m_buttons.Resize(3);
    }
}

SystemMessageBox::SystemMessageBox(SystemMessageBox &&other) noexcept
    : m_type(other.m_type),
      m_title(std::move(other.m_title)),
      m_message(std::move(other.m_message)),
      m_buttons(std::move(other.m_buttons))
{
}

SystemMessageBox &SystemMessageBox::operator=(SystemMessageBox &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    m_type = other.m_type;
    m_title = std::move(other.m_title);
    m_message = std::move(other.m_message);
    m_buttons = std::move(other.m_buttons);

    return *this;
}

SystemMessageBox::~SystemMessageBox() = default;

SystemMessageBox &SystemMessageBox::Title(const String &title)
{
    m_title = title;

    return *this;
}

SystemMessageBox &SystemMessageBox::Text(const String &text)
{
    m_message = text;

    return *this;
}

SystemMessageBox &SystemMessageBox::Button(const String &text, Proc<void> &&on_click)
{
    if (m_buttons.Size() >= 3) {
        HYP_LOG(Core, LogLevel::WARNING, "MessageBox does not support > 3 buttons");

        return *this;
    }

    m_buttons.PushBack(MessageBoxButton { text, std::move(on_click) });

    return *this;
}

void SystemMessageBox::Show() const
{
    const char *button_texts[3];

    for (int i = 0; i < int(m_buttons.Size()); i++) {
        button_texts[i] = m_buttons[i].text.Data();
    }

    int button_index = ShowMessageBox(int(m_type), m_title.Data(), m_message.Data(), int(m_buttons.Size()), button_texts);

    if (m_buttons.Any()) {
        if (button_index < 0 || button_index >= m_buttons.Size()) {
            HYP_LOG(Core, LogLevel::WARNING, "MessageBox Show() returned invalid index: {}, {} buttons", button_index, m_buttons.Size());

            return;
        }

        if (m_buttons[button_index].on_click.IsValid()) {
            m_buttons[button_index].on_click();
        }
    }
}

} // namespace sys
} // namespace hyperion