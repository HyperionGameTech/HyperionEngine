/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <system/MessageBox.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

extern "C"
{
    extern int ShowMessageBox(int type, const char* title, const char* message, int buttons, const char* buttonTexts[3]);
}

namespace hyperion {
namespace sys {

SystemMessageBox::SystemMessageBox(MessageBoxType type)
    : m_type(type)
{
}

SystemMessageBox::SystemMessageBox(
    MessageBoxType type,
    const String& title,
    const String& message,
    Array<MessageBoxButton>&& buttons)
    : m_type(type),
      m_title(title),
      m_message(message),
      m_buttons(std::move(buttons))
{
    if (m_buttons.Size() > 3)
    {
        m_buttons.Resize(3);
    }
}

SystemMessageBox::SystemMessageBox(SystemMessageBox&& other) noexcept
    : m_type(other.m_type),
      m_title(std::move(other.m_title)),
      m_message(std::move(other.m_message)),
      m_buttons(std::move(other.m_buttons))
{
}

SystemMessageBox& SystemMessageBox::operator=(SystemMessageBox&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    m_type = other.m_type;
    m_title = std::move(other.m_title);
    m_message = std::move(other.m_message);
    m_buttons = std::move(other.m_buttons);

    return *this;
}

SystemMessageBox::~SystemMessageBox() = default;

SystemMessageBox& SystemMessageBox::Title(const String& title)
{
    m_title = title;

    return *this;
}

SystemMessageBox& SystemMessageBox::Text(const String& text)
{
    m_message = text;

    return *this;
}

SystemMessageBox& SystemMessageBox::Button(const String& text, Proc<void()>&& onClick)
{
    if (m_buttons.Size() >= 3)
    {
        return *this;
    }

    m_buttons.PushBack(MessageBoxButton { text, std::move(onClick) });

    return *this;
}

void SystemMessageBox::Show() const
{
    const char* buttonTexts[3] = {};

    for (int i = 0; i < int(m_buttons.Size()); i++)
    {
        buttonTexts[i] = m_buttons[i].text.Data();
    }
    
    AssertDebug(m_title.Data() != nullptr && m_message.Data() != nullptr);

    int buttonIndex = ShowMessageBox(int(m_type), m_title.Data(), m_message.Data(), int(m_buttons.Size()), buttonTexts);

    if (m_buttons.Any())
    {
        if (buttonIndex < 0 || buttonIndex >= m_buttons.Size())
        {
            HYP_LOG(Core, Warning, "ShowMessageBox() returned invalid index: {}, {} buttons", buttonIndex, m_buttons.Size());

            return;
        }

        if (m_buttons[buttonIndex].onClick.IsValid())
        {
            m_buttons[buttonIndex].onClick();
        }
    }
}

} // namespace sys
} // namespace hyperion
