#include <SystemPch.hpp>

#include <system/SystemEvent.hpp>
#include <system/AppContext.hpp>

#include <engine/threads/MainThread.hpp>

#include <input/InputManager.hpp>

#include <core/threading/Task.hpp>

namespace hyperion {

#ifdef HYP_MACOS
extern void DestroyCocoaEvent(CocoaEvent& cocoaEvent);
#endif

namespace sys {

#pragma region Helper methods

MouseEvent SystemEvent::ToMouseEvent() const
{
    return ToMouseEvent(m_window ? Vec2f(m_window->GetDimensions()) : Vec2f::One());
}

MouseEvent SystemEvent::ToMouseEvent(const Vec2f& surfaceSize) const
{
    MouseEvent me {};
    me.mouseButtons = GetMouseButtons();

    me.absolutePos = m_eventData.Is<Vec2i>() ? m_eventData.GetUnchecked<Vec2i>() : Vec2i::Zero();
    me.absolutePrevPos = g_inputManager->GetPreviousMousePosition();

    me.relativePos = Vec2f(me.absolutePos);
    me.relativePrevPos = Vec2f(me.absolutePrevPos);

    if (!surfaceSize.IsZero())
    {
        me.relativePos /= surfaceSize;
        me.relativePrevPos /= surfaceSize;
    }

    return me;
}

KeyboardEvent SystemEvent::ToKeyboardEvent() const
{
    KeyboardEvent kbe {};
    kbe.inputManager = g_inputManager;
    kbe.keyCode = m_eventData.Is<KeyCode>() ? m_eventData.GetUnchecked<KeyCode>() : KeyCode::KEY_UNKNOWN;

    return kbe;
}

#ifdef HYP_SDL

static EnumFlags<MouseButtonState> GetMouseButtonState(int sdlButton)
{
    EnumFlags<MouseButtonState> mouseButtonState = MouseButtonState::NONE;

    switch (sdlButton)
    {
    case SDL_BUTTON_LEFT:
        mouseButtonState |= MouseButtonState::LEFT;
        break;
    case SDL_BUTTON_MIDDLE:
        mouseButtonState |= MouseButtonState::MIDDLE;
        break;
    case SDL_BUTTON_RIGHT:
        mouseButtonState |= MouseButtonState::RIGHT;
        break;
    default:
        break;
    }

    // Bitset bitset { uint32(sdlButton) };

    // Bitset::BitIndex firstSetBitIndex = -1;

    // while ((firstSetBitIndex = bitset.FirstSetBitIndex()) != -1) {
    //     switch (firstSetBitIndex + 1) {
    //     case SDL_BUTTON_LEFT:
    //         mouseButtonState |= MouseButtonState::LEFT;
    //         break;
    //     case SDL_BUTTON_MIDDLE:
    //         mouseButtonState |= MouseButtonState::MIDDLE;
    //         break;
    //     case SDL_BUTTON_RIGHT:
    //         mouseButtonState |= MouseButtonState::RIGHT;
    //         break;
    //     default:
    //         break;
    //     }

    //     bitset.Set(firstSetBitIndex, false);
    // }

    return mouseButtonState;
}

#endif

#pragma endregion Helper methods

#pragma region SystemEvent

SystemEvent::~SystemEvent()
{
#ifdef HYP_MACOS
    if (m_eventType == INVALID)
    {
        return;
    }

    CocoaEvent& cocoaEvent = m_platformEvent.cocoaEvent;

    if (IsOnThread(g_mainThread))
    {
        DestroyCocoaEvent(cocoaEvent);
    }
    else
    {
        g_mainThreadInstance->GetScheduler().Enqueue([cocoaEvent = std::move(cocoaEvent)]() mutable
            {
                DestroyCocoaEvent(cocoaEvent);
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }

    cocoaEvent = {};
#endif
}

#pragma endregion SystemEvent

} // namespace sys
} // namespace hyperion
