/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <UIPch.hpp>

#include <UI/UIStage.hpp>

#include <UI/Overlays/ConsoleOverlay.hpp>

#include <UI/Console/UIConsole.hpp>

#include <Input/Event.hpp>
#include <Input/Keyboard.hpp>

#include <ConsoleOverlay.generated.inl>

namespace Hyperion {

#pragma region ConsoleOverlay

ConsoleOverlay::ConsoleOverlay()
    : OverlayBase(),
      m_console(nullptr),
      m_isOpen(false),
      m_toggleKeyHeld(false)
{
}

ConsoleOverlay::~ConsoleOverlay() = default;

Handle<UIObject> ConsoleOverlay::CreateUIObject(UIObject* spawnParent)
{
    HYP_SCOPE;

    Handle<UIConsole> console = spawnParent->CreateUIObject<UIConsole>(
        NAME("ConsoleOverlay_Console"),
        Vec2i(2, 2),
        UIObjectSize({ 500, UIObjectSize::PIXEL }, { 200, UIObjectSize::PIXEL }));

    console->SetDepth(1000);

    m_console = console.Get();

    return console;
}

void ConsoleOverlay::Update(float delta)
{
    HYP_SCOPE;

}

void ConsoleOverlay::SetOpen(bool isOpen)
{
    HYP_SCOPE;

    if (m_isOpen == isOpen)
    {
        return;
    }

    m_isOpen = isOpen;

    if (!m_uiObject.IsValid() || m_console == nullptr)
    {
        return;
    }

    m_uiObject->SetIsVisible(isOpen);

    if (isOpen)
    {
        m_console->FocusInput();
    }
    else
    {
        m_console->Blur();
    }
}

bool ConsoleOverlay::OnInputEvent(const Event& event)
{
    HYP_SCOPE;

    switch (event.GetType())
    {
    case EventType::KEYDOWN:
        if (event.GetKeyCode() != KeyCode::KEY_TILDE)
        {
            return false;
        }

        if (!m_toggleKeyHeld)
        {
            m_toggleKeyHeld = true;

            Toggle();
        }

        return true;
    case EventType::KEYUP:
        if (event.GetKeyCode() == KeyCode::KEY_TILDE)
        {
            m_toggleKeyHeld = false;
        }

        return false;
    case EventType::TEXT_INPUT:
        return m_toggleKeyHeld && (event.GetTextInput() == "`" || event.GetTextInput() == "~");
    default:
        return false;
    }
}

#pragma endregion ConsoleOverlay

} // namespace Hyperion
