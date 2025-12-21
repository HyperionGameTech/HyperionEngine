/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <input/InputManager.hpp>

#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Spinlock.hpp>

#include <engine/EngineGlobals.hpp>

#include <InputManager.generated.inl>

namespace hyperion {

static SlabAllocator s_inputMouseLockStateAllocator(sizeof(InputMouseLockState), alignof(InputMouseLockState), 32, AF_THREAD_SAFE);

#pragma region InputEventQueue

/// <summary>
/// SPSC queue for events
/// </summary>
class InputEventQueue
{
public:
    static constexpr uint32 MaxSize = 1024;

    InputEventQueue()
        : m_head(0),
          m_tail(0)
    {
        m_buffer.Resize(MaxSize);
    }

    InputEventQueue(const InputEventQueue& other) = delete;
    InputEventQueue& operator=(const InputEventQueue& other) = delete;

    InputEventQueue(InputEventQueue&& other) noexcept = delete;
    InputEventQueue& operator=(InputEventQueue&& other) noexcept = delete;

    ~InputEventQueue();

    bool Push(SystemEvent&& evt);
    bool Pop(SystemEvent& outEvent);

private:
    HYP_FORCE_INLINE static bool Full(uint32 head, uint32 tail)
    {
        return ((head + 1) & (MaxSize - 1)) == tail;
    }

    HYP_FORCE_INLINE static bool Empty(uint32 head, uint32 tail)
    {
        return head == tail;
    }

    Array<ValueStorage<SystemEvent>, DynamicAllocator> m_buffer;

    mutable volatile int32 m_head;
    mutable volatile int32 m_tail;
};

InputEventQueue::~InputEventQueue()
{
    const uint32 head = std::bit_cast<uint32>(AtomicAdd(&m_head, 0));
    uint32 tail = std::bit_cast<uint32>(AtomicAdd(&m_tail, 0));

    while (tail != head)
    {
        m_buffer[tail].Destruct();

        tail = (tail + 1) & (MaxSize - 1);
    }
}

bool InputEventQueue::Push(SystemEvent&& evt)
{
    const uint32 head = std::bit_cast<uint32>(AtomicAdd(&m_head, 0));
    const uint32 tail = std::bit_cast<uint32>(AtomicAdd(&m_tail, 0));

    if (Full(head, tail))
    {
        return false;
    }

    new (&m_buffer[head]) SystemEvent(std::move(evt));

    AtomicExchange(&m_head, (head + 1) & (MaxSize - 1));

    return true;
}

bool InputEventQueue::Pop(SystemEvent& outEvent)
{
    const uint32 head = std::bit_cast<uint32>(AtomicAdd(&m_head, 0));
    const uint32 tail = std::bit_cast<uint32>(AtomicAdd(&m_tail, 0));

    if (Empty(head, tail))
    {
        return false;
    }

    outEvent = std::move(m_buffer[tail].Get());
    m_buffer[tail].Destruct();

    AtomicExchange(&m_tail, ((tail + 1) & (MaxSize - 1)));

    return true;
}

#pragma endregion InputEventQueue

#pragma region InputMouseLockScope

InputMouseLockScope& InputMouseLockScope::operator=(InputMouseLockScope&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (mouseLockState && inputMgr)
    {
        inputMgr->RemoveMouseLockState(mouseLockState);
    }

    mouseLockState = other.mouseLockState;
    inputMgr = other.inputMgr;

    other.mouseLockState = nullptr;
    other.inputMgr = nullptr;

    return *this;
}

InputMouseLockScope::~InputMouseLockScope()
{
    if (mouseLockState && inputMgr)
    {
        inputMgr->RemoveMouseLockState(mouseLockState);
    }
}

void InputMouseLockScope::Reset()
{
    if (mouseLockState && inputMgr)
    {
        inputMgr->RemoveMouseLockState(mouseLockState);
    }

    mouseLockState = nullptr;
    inputMgr = nullptr;
}

#pragma endregion InputMouseLockScope

#pragma region InputManager

InputManager::InputManager(ApplicationWindow* ownerWindow)
    : m_eventQueue(new InputEventQueue),
      m_ownerWindow(ownerWindow)
{
    AssertDebug(ownerWindow != nullptr);
}

InputManager::~InputManager()
{
    SetIsMouseLocked(false);

    for (int i = 0; i < int(m_mouseLockStates.Size()); i++)
    {
        InputMouseLockState* state = m_mouseLockStates[i];
        auto it = m_mouseLockStates.Begin() + i;

        if (m_mouseLockStates.IndexOf(it) == i)
        {
            state->~InputMouseLockState();
            s_inputMouseLockStateAllocator.Free(state);
        }
    }

    delete m_eventQueue;
}

bool InputManager::IsMouseLocked() const
{
    return m_isMouseLocked;
}

void InputManager::PushMouseLockState(bool mouseLocked)
{
    InputMouseLockState* mouseLockState = (InputMouseLockState*)s_inputMouseLockStateAllocator.Allocate();

    new (mouseLockState) InputMouseLockState;
    mouseLockState->locked = mouseLocked;

    ApplyMouseLockState(mouseLockState);
}

void InputManager::PopMouseLockState()
{
    Mutex::Guard guard(m_mouseLockStatesMutex);

    if (m_mouseLockStates.Empty())
    {
        if (IsOnThread(g_mainThread))
        {
            // Default state if none active
            SetIsMouseLocked(false);
        }

        return;
    }

    InputMouseLockState* lastState = m_mouseLockStates.PopBack();

    if (IsOnThread(g_mainThread))
    {
        SetIsMouseLocked(lastState->locked);
    }

    if (!m_mouseLockStates.Contains(lastState))
    {
        lastState->~InputMouseLockState();
        s_inputMouseLockStateAllocator.Free(lastState);
    }
}

InputMouseLockScope InputManager::AcquireMouseLock()
{
    InputMouseLockState* mouseLockState = (InputMouseLockState*)s_inputMouseLockStateAllocator.Allocate();

    new (mouseLockState) InputMouseLockState;
    mouseLockState->locked = true;

    ApplyMouseLockState(mouseLockState);

    return InputMouseLockScope { this, mouseLockState };
}

void InputManager::SetIsMouseLocked(bool isMouseLocked)
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    if (m_isMouseLocked == isMouseLocked)
    {
        return; // already set
    }

    if (m_ownerWindow)
    {
        m_ownerWindow->SetIsMouseLocked(isMouseLocked);
    }
    else
    {
        // set to false if no window
        isMouseLocked = false;
    }

    m_isMouseLocked = isMouseLocked;
}

void InputManager::SetMousePosition(Vec2i position)
{
    HYP_SCOPE;
    //AssertOnThread(g_mainThread);

    if (!m_ownerWindow)
    {
        return;
    }

    m_mousePosition = position;

    m_ownerWindow->SetMousePosition(position);
}

void InputManager::UpdateMousePosition(const SystemEvent& event)
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    if (!m_ownerWindow)
    {
        return;
    }

    m_previousMousePosition = m_mousePosition;
    m_mousePosition = event.IsAbsoluteMousePosition()
        ? event.GetMousePosition()
        : Vec2i(m_mousePosition) + Vec2i(event.GetMousePositionDeltas());
}

void InputManager::UpdateWindowSize(Vec2i newSize)
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    m_windowSize = newSize;
}

void InputManager::SetKey(KeyCode key, bool pressed)
{
    AssertOnThread(g_mainThread);

    if (uint32(key) < NUM_KEYBOARD_KEYS)
    {
        const uint32 bitIdx = uint32(key) / 32;
        const uint32 bitMask = 1u << (uint32(key) % 32);

        if (pressed)
        {
            AtomicBitOr(&m_inputState.keyStates[bitIdx], bitMask);
        }
        else
        {
            AtomicBitAnd(&m_inputState.keyStates[bitIdx], ~bitMask);
        }
    }
}

void InputManager::SetMouseButton(MouseButtonKey btn, bool pressed)
{
    AssertOnThread(g_mainThread);

    if (uint32(btn) < NUM_MOUSE_BUTTONS)
    {
        const uint32 bitIdx = uint32(btn) / 32;
        const uint32 bitMask = 1u << (uint32(btn) % 32);

        if (pressed)
        {
            AtomicBitOr(&m_inputState.mouseButtonStates, bitMask);
        }
        else
        {
            AtomicBitAnd(&m_inputState.mouseButtonStates, ~bitMask);
        }
    }
}

bool InputManager::IsKeyDown(KeyCode key) const
{
    if (uint32(key) < NUM_KEYBOARD_KEYS)
    {
        const uint32 bitIdx = uint32(key) / 32;
        const uint32 bitMask = 1u << (uint32(key) % 32);

        return AtomicAdd(&m_inputState.keyStates[bitIdx], 0) & bitMask;
    }

    return false;
}

bool InputManager::IsButtonDown(MouseButtonKey btn) const
{
    if (uint32(btn) < NUM_MOUSE_BUTTONS)
    {
        const uint32 bitMask = 1u << (uint32(btn) % 32);

        return AtomicAdd(&m_inputState.mouseButtonStates, 0) & bitMask;
    }

    return false;
}

EnumFlags<MouseButtonState> InputManager::GetButtonStates() const
{
    EnumFlags<MouseButtonState> state = MouseButtonState::NONE;

    const uint32 states = AtomicAdd(&m_inputState.mouseButtonStates, 0);

    for (uint32 i = 0; i < NUM_MOUSE_BUTTONS; i++)
    {
        if (states & (1u << i))
        {
            state |= MouseButtonState(1u << i);
        }
    }

    return state;
}

void InputManager::ApplyMouseLockState(InputMouseLockState* mouseLockState)
{
    HYP_SCOPE;

    Mutex::Guard guard(m_mouseLockStatesMutex);

    if (!mouseLockState)
    {
        if (m_mouseLockStates.Empty() && IsOnThread(g_mainThread))
        {
            // apply default state
            SetIsMouseLocked(false);
        }

        return;
    }

    m_mouseLockStates.PushBack(mouseLockState);

    if (IsOnThread(g_mainThread))
    {
        SetIsMouseLocked(mouseLockState->locked);
    }
}

void InputManager::RemoveMouseLockState(InputMouseLockState* mouseLockState)
{
    HYP_SCOPE;

    if (!mouseLockState)
    {
        return;
    }

    Mutex::Guard guard(m_mouseLockStatesMutex);

    auto it = m_mouseLockStates.Find(mouseLockState);
    Assert(it != m_mouseLockStates.End());

    auto eraseIt = m_mouseLockStates.Erase(it);

    if (!m_mouseLockStates.Contains(mouseLockState))
    {
        mouseLockState->~InputMouseLockState();
        s_inputMouseLockStateAllocator.Free(mouseLockState);
    }

    if (eraseIt == m_mouseLockStates.End() && IsOnThread(g_mainThread)) // was it at the end?
    {
        SetIsMouseLocked(m_mouseLockStates.Any() ? m_mouseLockStates.Back()->locked : false);
    }
}

void InputManager::ProcessEvent(SystemEvent&& event)
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    switch (event.GetType())
    {
    case SystemEvent::KEYDOWN:
        SetKey(event.GetKeyCode(), true);

        break;
    case SystemEvent::KEYUP:
        SetKey(event.GetKeyCode(), false);

        break;
    case SystemEvent::MOUSEBUTTON_DOWN:
        for (Bitset::BitIndex index : Bitset(event.GetMouseButtons()))
        {
            SetMouseButton(MouseButtonKey(index), true);
        }

        break;
    case SystemEvent::MOUSEBUTTON_UP:
        for (Bitset::BitIndex index : Bitset(event.GetMouseButtons()))
        {
            SetMouseButton(MouseButtonKey(index), false);
        }

        break;
    case SystemEvent::MOUSEMOTION:
        UpdateMousePosition(event);

        break;
    case SystemEvent::WINDOW_RESIZED:
        UpdateWindowSize(event.GetWindowResizeDimensions());

        break;
    default:
        break;
    }

    const SystemEvent::EventType eventType = event.GetType();

    if (!m_eventQueue->Push(std::move(event)))
    {
        HYP_LOG(Input, Warning, "Input event queue full! Skipped event of type {}", eventType);
    }
}

void InputManager::BufferSwap()
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);
}

bool InputManager::PollEvent(SystemEvent& outEvent)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    return m_eventQueue->Pop(outEvent);
}

void InputManager::MainThreadUpdate()
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    Mutex::Guard guard(m_mouseLockStatesMutex);
    SetIsMouseLocked(m_mouseLockStates.Any() ? m_mouseLockStates.Back()->locked : false);
}

#pragma endregion InputManager

} // namespace hyperion
