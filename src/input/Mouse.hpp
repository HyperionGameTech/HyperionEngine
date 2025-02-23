#ifndef HYPERION_MOUSE_HPP
#define HYPERION_MOUSE_HPP

#include <core/math/Vector2.hpp>
#include <core/utilities/EnumFlags.hpp>

namespace hyperion {

class InputManager;

enum class MouseButton : uint32
{
    INVALID = ~0u,
    LEFT    = 0,
    MIDDLE,
    RIGHT,
    
    MAX
};

enum class MouseButtonState : uint32
{
    NONE    = 0x0,
    LEFT    = 1u << uint32(MouseButton::LEFT),
    MIDDLE  = 1u << uint32(MouseButton::MIDDLE),
    RIGHT   = 1u << uint32(MouseButton::RIGHT)
};

HYP_MAKE_ENUM_FLAGS(MouseButtonState)

HYP_STRUCT(Size=56)
struct MouseEvent
{
    HYP_FIELD()
    InputManager                *input_manager = nullptr;

    HYP_FIELD()
    Vec2f                       position;

    HYP_FIELD()
    Vec2f                       previous_position;

    HYP_FIELD()
    Vec2i                       absolute_position;

    HYP_FIELD()
    EnumFlags<MouseButtonState> mouse_buttons = MouseButtonState::NONE;

    HYP_FIELD()
    Vec2i                       wheel;

    HYP_FIELD(Deprecated)
    bool                        is_down = false;
};

struct InputMouseLockState
{
    InputManager    *input_manager = nullptr;
    bool            locked = false;

    HYP_FORCE_INLINE bool operator==(const InputMouseLockState &other) const
    {
        return input_manager == other.input_manager
            && locked == other.locked;
    }

    HYP_FORCE_INLINE bool operator!=(const InputMouseLockState &other) const
    {
        return input_manager != other.input_manager
            || locked != other.locked;
    }
};

struct InputMouseLockScope
{
    InputMouseLockState *mouse_lock_state;

    InputMouseLockScope()
        : mouse_lock_state(nullptr)
    {
    }

    InputMouseLockScope(InputMouseLockState &mouse_lock_state)
        : mouse_lock_state(&mouse_lock_state)
    {
    }

    InputMouseLockScope(const InputMouseLockScope &other)                   = delete;
    InputMouseLockScope &operator=(const InputMouseLockScope &other)        = delete;

    InputMouseLockScope(InputMouseLockScope &&other) noexcept
        : mouse_lock_state(other.mouse_lock_state)
    {
    }

    HYP_API InputMouseLockScope &operator=(InputMouseLockScope &&other) noexcept;

    HYP_API ~InputMouseLockScope();

    HYP_API void Reset();

    HYP_FORCE_INLINE explicit operator bool() const
        { return mouse_lock_state != nullptr && mouse_lock_state->locked; }

    HYP_FORCE_INLINE bool operator!() const
        { return mouse_lock_state == nullptr || !mouse_lock_state->locked; }
};

} // namespace hyperion

#endif