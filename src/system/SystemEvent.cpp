#include <HyperionPch.hpp>

#include <system/SystemEvent.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/threads/MainThread.hpp>

#include <input/InputManager.hpp>

#include <core/threading/Task.hpp>

namespace hyperion {

#ifdef HYP_MACOS
extern void DestroyCocoaEvent(CocoaEvent& cocoaEvent);
#endif

namespace sys {

#pragma region Helper methods

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

MouseEvent SystemEvent::ToMouseEvent() const
{
    Vec2i absolutePosition = m_eventData.Get<Vec2i>();

    MouseEvent me {
        .mouseButtons = GetMouseButtons(),
        .absolutePosition = absolutePosition,
        .previousPosition = Vec2f(g_inputManager->GetPreviousMousePosition()),
        .position = Vec2f(absolutePosition),
    };

    return me;
}

MouseEvent SystemEvent::ToMouseEvent(const Vec2f& surfaceSize) const
{
    Vec2i absolutePosition = m_eventData.Get<Vec2i>();

    Vec2f relativePosition = Vec2f(absolutePosition);

    if (!relativePosition.IsZero())
    {
        relativePosition = Vec2f(absolutePosition) / surfaceSize;
    }

    MouseEvent me {
        .mouseButtons = GetMouseButtons(),
        .absolutePosition = absolutePosition,
        .previousPosition = Vec2f(g_inputManager->GetPreviousMousePosition()),
        .position = relativePosition,
    };

    return me;
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
