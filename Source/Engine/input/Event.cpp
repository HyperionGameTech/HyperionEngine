#include <HyperionPch.hpp>

#include <input/Event.hpp>
#include <input/InputManager.hpp>

#include <system/AppContext.hpp>

#include <core/threading/Task.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/threads/MainThread.hpp>

#include <Event.generated.inl>

namespace Hyperion {

#ifdef HYP_MACOS
extern void DestroyCocoaEvent(CocoaEvent& cocoaEvent);
#endif

#pragma region Helper methods

MouseEvent Event::ToMouseEvent() const
{
    Vec2f offsetMousePos = Vec2f::Zero();
    Vec2f surfaceSize = Vec2f::One();

    if (m_window != nullptr)
    {
        offsetMousePos = Vec2f(m_window->GetInputManager()->GetMousePosition());
        surfaceSize = Vec2f(m_window->GetDimensions());
    }

    return ToMouseEvent(offsetMousePos, surfaceSize);
}

MouseEvent Event::ToMouseEvent(const Vec2f& offsetMousePos, const Vec2f& surfaceSize) const
{
    MouseEvent me {};
    me.baseEvent = this;
    me.mouseButtons = GetMouseButtons();

    me.absolutePos = IsAbsoluteMousePosition() ? Vec2f(GetMousePosition()) : (offsetMousePos + GetMousePositionDeltas());
    me.absolutePrevPos = offsetMousePos;

    me.relativePos = me.absolutePos;
    me.relativePrevPos = me.absolutePrevPos;

    if (!surfaceSize.IsZero())
    {
        me.relativePos /= surfaceSize;
        me.relativePrevPos /= surfaceSize;
    }

    return me;
}

KeyboardEvent Event::ToKeyboardEvent() const
{
    KeyboardEvent kbe {};
    kbe.baseEvent = this;
    kbe.inputManager = m_window ? m_window->GetInputManager() : nullptr;
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

#pragma region Event

Event::~Event()
{
#ifdef HYP_DEBUG_MODE
    // To allow easier debugging of stale events
    m_timestamp = Time(0);
#endif

#ifdef HYP_MACOS
    if (m_eventType == EventType::INVALID)
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

#pragma endregion Event

} // namespace Hyperion
