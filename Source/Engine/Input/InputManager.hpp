/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector2.hpp>

#include <Core/Threading/Semaphore.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Input/Keyboard.hpp>
#include <Input/Mouse.hpp>
#include <Input/Controller.hpp>
#include <Input/InputConstants.hpp>

namespace Hyperion {

class ApplicationWindow;
class Event;
class InputHandlerBase;

extern Pool* g_inputPool;
using InputAllocator = AllocatorInstance<Pool, &g_inputPool>;

struct InputState
{
    mutable volatile int32 keyStates[NumKeyboardKeys / 32 + 1];
    mutable volatile int32 mouseButtonStates;

    InputState()
        : keyStates {},
          mouseButtonStates {}
    {
    }
};

class InputEventQueue;

HYP_CLASS()
class InputManager : public ObjectBase
{
    HYP_OBJECT_BODY(InputManager);

    friend struct InputMouseLockScope;

public:
    explicit InputManager(ApplicationWindow* ownerWindow);

    InputManager(const InputManager& other) = delete;
    InputManager& operator=(const InputManager& other) = delete;

    InputManager(InputManager&& other) noexcept = delete;
    InputManager& operator=(InputManager&& other) noexcept = delete;

    ~InputManager();

    HYP_METHOD()
    bool IsMouseLocked() const;

    HYP_METHOD()
    void PushMouseLockState(bool mouseLocked, bool syncToVirtualPosition = false);

    HYP_METHOD()
    void PopMouseLockState();

    /*! \brief Acquire a new mouse lock scope. The input device will be locked until the object goes out of scope or another state takes precedence over this one.
     *   \param [syncToVirtualPosition] If true, when the state is removed, the mouse state will be synchronized with the virtual mouse position (that is, the internal position that
     *   continues to be updated regardless of whether or not the actual mouse is locked) */
    InputMouseLockScope AcquireMouseLock(bool syncToVirtualPosition = false);

    HYP_METHOD()
    Vec2i GetMousePosition() const
    {
        return m_mousePosition;
    }

    Vec2i GetPreviousMousePosition() const
    {
        return m_previousMousePosition;
    }

    HYP_METHOD()
    Vec2i GetVirtualMousePosition() const
    {
        return m_virtualMousePosition;
    }

    HYP_METHOD()
    Vec2i GetPreviousVirtualMousePosition() const
    {
        return m_previousVirtualMousePosition;
    }

    HYP_METHOD()
    Vec2f GetVirtualMousePositionNormalized() const;

    HYP_METHOD()
    HYP_FORCE_INLINE Vec2i GetWindowSize() const
    {
        return m_windowSize;
    }

    HYP_METHOD()
    bool IsKeyDown(KeyCode key) const;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsKeyUp(KeyCode key) const
    {
        return !IsKeyDown(key);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsShiftDown() const
    {
        return IsKeyDown(KeyCode::KEY_LSHIFT) || IsKeyDown(KeyCode::KEY_RSHIFT);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsAltDown() const
    {
        return IsKeyDown(KeyCode::KEY_LALT) || IsKeyDown(KeyCode::KEY_RALT);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsCtrlDown() const
    {
        return IsKeyDown(KeyCode::KEY_LCTRL) || IsKeyDown(KeyCode::KEY_RCTRL);
    }

    HYP_METHOD()
    bool IsButtonDown(MouseButtonKey btn) const;

    HYP_METHOD()
    EnumFlags<MouseButtonState> GetButtonStates() const;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsButtonUp(MouseButtonKey btn) const
    {
        return !IsButtonDown(btn);
    }

    void AddController(ControllerHandle controller);
    void RemoveController(ControllerHandle controller);

    HYP_FORCE_INLINE ControllerHandle GetController(uint8 controllerIndex) const
    {
        return m_controllers[controllerIndex];
    }

    HYP_FORCE_INLINE bool HasAttachedController(uint8& outControllerIndex) const
    {
        if (m_validControllersMask == 0)
        {
            return false;
        }

        outControllerIndex = uint8(ByteUtil::LowestSetBitIndex(m_validControllersMask));

        return true;
    }

    HYP_FORCE_INLINE uint8 NumAttachedControllers() const
    {
        return ByteUtil::BitCount(m_validControllersMask);
    }
    
    HYP_METHOD()
    HYP_FORCE_INLINE ApplicationWindow* GetWindow() const
    {
        return m_ownerWindow;
    }

    void ProcessEvent(Event&& event);

    void MainThreadUpdate();
    void BufferSwap();

    bool PollEvent(Event& outEvent);

private:
    void SetIsMouseLocked(bool locked);

    void UpdateMousePosition(Event& event);
    void UpdateWindowSize(Vec2i newSize);

    void SetKey(KeyCode key, bool pressed);
    void SetMouseButton(MouseButtonKey btn, bool pressed);

    void ApplyMouseLockState(InputMouseLockState* mouseLockState);
    void RemoveMouseLockState(InputMouseLockState* mouseLockState);

    struct AtomicVec2i
    {
        volatile int32 x;
        volatile int32 y;

        AtomicVec2i()
            : x(0),
              y(0)
        {
        }

        AtomicVec2i(const Vec2i& vec)
            : x(vec.x),
              y(vec.y)
        {
        }

        AtomicVec2i(const AtomicVec2i& other)
            : x(AtomicAdd(const_cast<volatile int32*>(&other.x), 0)),
              y(AtomicAdd(const_cast<volatile int32*>(&other.y), 0))
        {
        }

        AtomicVec2i& operator=(const AtomicVec2i& other)
        {
            AtomicExchange(&x, AtomicAdd(const_cast<volatile int32*>(&other.x), 0));
            AtomicExchange(&y, AtomicAdd(const_cast<volatile int32*>(&other.y), 0));

            return *this;
        }

        HYP_FORCE_INLINE operator Vec2i() const
        {
            return Vec2i(
                AtomicAdd(const_cast<volatile int32*>(&x), 0),
                AtomicAdd(const_cast<volatile int32*>(&y), 0));
        }

        HYP_FORCE_INLINE explicit operator Vec2f() const
        {
            return Vec2f(
                AtomicAdd(const_cast<volatile int32*>(&x), 0),
                AtomicAdd(const_cast<volatile int32*>(&y), 0));
        }

        HYP_FORCE_INLINE AtomicVec2i& operator=(const Vec2i& vec)
        {
            AtomicExchange(&x, vec.x);
            AtomicExchange(&y, vec.y);

            return *this;
        }
    };

    InputEventQueue* m_eventQueue;
    InputState m_inputState;

    AtomicVec2i m_mousePosition;
    AtomicVec2i m_previousMousePosition;

    AtomicVec2i m_virtualMousePosition;
    AtomicVec2i m_previousVirtualMousePosition;

    Vec2f m_virtualMousePositionAccum;

    AtomicVec2i m_windowSize;

    Array<InputMouseLockState*, InputAllocator> m_mouseLockStates;
    Mutex m_mouseLockStatesMutex;

    ApplicationWindow* m_ownerWindow;

    ControllerHandle m_controllers[MaxAttachedControllers];
    uint8 m_validControllersMask;

    bool m_isMouseLocked;
    bool m_syncToVirtualPosition;
};

} // namespace Hyperion
