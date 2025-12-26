#pragma once

#include <core/math/Vector2.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/reflection/ObjectFwd.hpp>

namespace Hyperion {

class InputManager;
class Event;

HYP_ENUM()
enum MouseButtonKey : uint32
{
    MBK_INVALID = ~0u,
    MBK_LEFT = 0,
    MBK_MIDDLE,
    MBK_RIGHT,
    MBK_MAX
};

HYP_ENUM()
enum class MouseButtonState : uint32
{
    NONE = 0x0,
    LEFT = 0x1,
    MIDDLE = 0x2,
    RIGHT = 0x4
};

HYP_MAKE_ENUM_FLAGS(MouseButtonState)

HYP_STRUCT()
struct MouseEvent
{
    HYP_STRUCT_BODY(MouseEvent);

    const Event* baseEvent = nullptr;

    HYP_FIELD()
    Vec2f relativePos;

    HYP_FIELD()
    Vec2f relativePrevPos;

    HYP_FIELD()
    Vec2f absolutePos;

    HYP_FIELD()
    Vec2f absolutePrevPos;

    HYP_FIELD()
    EnumFlags<MouseButtonState> mouseButtons = MouseButtonState::NONE;

    HYP_FIELD()
    Vec2i wheel;

    HYP_FIELD(Deprecated)
    bool isDown = false;
};

struct InputMouseLockState
{
    bool locked = false;
    bool syncToVirtualPosition = false;
};

struct InputMouseLockScope
{
    InputManager* inputMgr;
    InputMouseLockState* mouseLockState;

    InputMouseLockScope()
        : inputMgr(nullptr),
          mouseLockState(nullptr)
    {
    }

    InputMouseLockScope(InputManager* inputMgr, InputMouseLockState* mouseLockState)
        : inputMgr(inputMgr),
          mouseLockState(mouseLockState)
    {
    }

    InputMouseLockScope(const InputMouseLockScope& other) = delete;
    InputMouseLockScope& operator=(const InputMouseLockScope& other) = delete;

    InputMouseLockScope(InputMouseLockScope&& other) noexcept
        : inputMgr(other.inputMgr),
          mouseLockState(other.mouseLockState)
    {
        other.inputMgr = nullptr;
        other.mouseLockState = nullptr;
    }

    InputMouseLockScope& operator=(InputMouseLockScope&& other) noexcept;

    ~InputMouseLockScope();

    void Reset();

    bool operator!() const
    {
        return !inputMgr || !mouseLockState;
    }

    explicit operator bool() const
    {
        return inputMgr && mouseLockState;
    }
};

} // namespace Hyperion
