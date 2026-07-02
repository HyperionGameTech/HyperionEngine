/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <SystemPch.hpp>

#include <System/MessageBox.hpp>

#include <Core/Threading/Task.hpp>
#include <Core/Threading/Threads.hpp>

extern "C"
{
#ifdef HYP_MACOS
    extern int ShowMessageBox(
        int type,
        const char* title,
        const char* message,
        int buttons,
        const char* buttonTexts[3],
        const void* buttonFuncs[3],
        Hyperion::TaskPromise<void>* promise);
#else
    extern int ShowMessageBox(
        int type,
        const char* title,
        const char* message,
        int buttons,
        const char* buttonTexts[3]);
#endif
}

namespace Hyperion {

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
      m_message(message)
{
    if (buttons.Size() > 3)
    {
        m_buttons.Resize(3);
    }
    else
    {
        m_buttons.Resize(buttons.Size());
    }

    for (int i = 0; i < 3; i++)
    {
        if (i == m_buttons.Size())
        {
            break;
        }

        m_buttons[i] = MakeShared<MessageBoxButton>(std::move(buttons[i]));
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

    m_buttons.PushBack(MakeShared<MessageBoxButton>(MessageBoxButton { text, std::move(onClick) }));

    return *this;
}

void SystemMessageBox::Show(bool showBlocking) const
{
    const char* buttonTexts[3] = {};

    for (int i = 0; i < int(m_buttons.Size()); i++)
    {
        buttonTexts[i] = m_buttons[i]->text.Data();
    }

#ifdef HYP_MACOS
    const bool isOnMainThread = IsOnThread(g_mainThread);

    Task<void> futureValue;

    const void* buttonFuncs[3] = {};
    for (int i = 0; i < int(m_buttons.Size()); i++)
    {
        if (!m_buttons[i]->onClick.IsValid())
        {
            buttonFuncs[i] = nullptr;

            continue;
        }

        // macos needs these allocated on the heap for async usage
        // wrap button as a ref counted ptr to keep it alive as long as we need it.
        buttonFuncs[i] = new Proc<void()>([button = m_buttons[i]]
            {
                button->onClick();
            });
    }

    int buttonIndex = ShowMessageBox(
        int(m_type),
        m_title.Data(),
        m_message.Data(),
        int(m_buttons.Size()),
        buttonTexts,
        buttonFuncs,
        futureValue.Promise());

#else
    int buttonIndex = ShowMessageBox(
        int(m_type),
        m_title.Data(),
        m_message.Data(),
        int(m_buttons.Size()),
        buttonTexts);
#endif

    if (buttonIndex != -1) // handle on this side
    {
        if (buttonIndex < 0 || buttonIndex >= m_buttons.Size())
        {
            // on macOS, we pass function pointers to Obj-C side,
            // to invoke the function pointer when not on the main thread and using async dispatch.

#ifndef HYP_APPLE
            HYP_LOG(Core, Warning, "ShowMessageBox() returned invalid index: {}, {} buttons", buttonIndex, m_buttons.Size());
#endif

            return;
        }

        if (m_buttons[buttonIndex]->onClick.IsValid())
        {
            m_buttons[buttonIndex]->onClick();
        }
    }

#ifdef HYP_MACOS
    else if (showBlocking) // async call and showBlocking true, await
    {
        futureValue.Await();
    }
#endif
}

} // namespace Hyperion
