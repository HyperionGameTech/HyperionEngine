#include <HyperionPch.hpp>

#include <Input/Event.hpp>
#include <Input/InputManager.hpp>

#include <System/AppContext.hpp>

#include <Core/threading/Task.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/threads/MainThread.hpp>

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
        surfaceSize = Vec2f(m_window->GetSize());
    }

    return ToMouseEvent(offsetMousePos, surfaceSize);
}

MouseEvent Event::ToMouseEvent(const Vec2f& offsetMousePos, const Vec2f& surfaceSize) const
{
    MouseEvent me {};
    me.baseEvent = this;

    // To allow conversion from touch events to mouse events
    switch (m_eventType)
    {
    case EventType::TOUCH_DOWN:
    case EventType::TOUCH_UP:
    case EventType::TOUCH_MOVE:
    {
        const TouchEventData* touchData = GetTouchEventData();
        AssertDebug(touchData != nullptr);

        if (touchData != nullptr)
        {
            const int32 pointerId = touchData->pointerId;

            me.mouseButtons = pointerId == 0 ? MouseButtonState::LEFT : MouseButtonState::NONE;

            me.absolutePos = touchData->motionData.position;
            me.absolutePrevPos = me.absolutePos - touchData->motionData.delta;
        }

        break;
    }
    default:
        me.mouseButtons = GetMouseButtons();

        me.absolutePos = IsAbsoluteMousePosition() ? GetMousePosition() : (offsetMousePos + GetMousePositionDeltas());
        me.absolutePrevPos = offsetMousePos;

        break;
    }

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

TouchEvent Event::ToTouchEvent() const
{
    TouchEvent te {};
    te.baseEvent = this;
    te.pointerId = GetTouchPointerId();
    te.position = GetTouchPosition();
    te.delta = GetTouchDelta();

    // Calculate normalized values (0..1 range)
    Vec2f screenSize = Vec2f::Zero();
    if (m_window != nullptr)
    {
        screenSize = Vec2f(m_window->GetDimensions());
    }

    if (!screenSize.IsZero())
    {
        te.relativePosition = te.position / screenSize;
        te.relativeDelta = te.delta / screenSize;
    }

    return te;
}

#pragma endregion Helper methods

#pragma region Event

Event::~Event()
{
#if HYP_DEBUG_MODE
    // To allow easier debugging of stale events
    m_timestamp = Time(0);
#endif

#ifdef HYP_MACOS
    if (m_eventType == EventType::INVALID)
    {
        return;
    }

    CocoaEvent& cocoaEvent = m_platformEvent.cocoaEvent;

    if (cocoaEvent.nsEvent != nullptr)
    {
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
    }

    cocoaEvent = {};
#endif
}

#pragma endregion Event

} // namespace Hyperion
