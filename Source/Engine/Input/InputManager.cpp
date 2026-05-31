/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <System/AppContext.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Threading/Threads.hpp>

#include <Framework/EngineGlobals.hpp>

#include <InputManager.generated.inl>

namespace Hyperion {

static Pool s_inputPool(1 * 1024 * 1024);
Pool* g_inputPool = &s_inputPool;

static TSlabAllocator<Pool>& GetMouseLockStateAllocator()
{
    static TSlabAllocator<Pool> s_mouseLockStateAllocator(
        g_inputPool,
        sizeof(InputMouseLockState),
        alignof(InputMouseLockState),
        32,
        AF_THREAD_SAFE);

    return s_mouseLockStateAllocator;
}

#pragma region InputEventQueue

/// <summary>
/// SPSC queue for events
/// </summary>
class InputEventQueue
{
public:
    static constexpr uint32 MaxSize = 1024;

    InputEventQueue()
        : m_buffer(),
          m_head(0),
          m_tail(0)
    {
        m_buffer.Resize(MaxSize);
    }

    InputEventQueue(const InputEventQueue& other) = delete;
    InputEventQueue& operator=(const InputEventQueue& other) = delete;

    InputEventQueue(InputEventQueue&& other) noexcept = delete;
    InputEventQueue& operator=(InputEventQueue&& other) noexcept = delete;

    ~InputEventQueue();

    bool Push(Event&& evt);
    bool Pop(Event& outEvent);

private:
    HYP_FORCE_INLINE static bool Full(uint32 head, uint32 tail)
    {
        return ((head + 1) & (MaxSize - 1)) == tail;
    }

    HYP_FORCE_INLINE static bool Empty(uint32 head, uint32 tail)
    {
        return head == tail;
    }

    Array<ValueStorage<Event>, InputAllocator> m_buffer;

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

bool InputEventQueue::Push(Event&& evt)
{
    const uint32 head = std::bit_cast<uint32>(AtomicAdd(&m_head, 0));
    const uint32 tail = std::bit_cast<uint32>(AtomicAdd(&m_tail, 0));

    if (Full(head, tail))
    {
        return false;
    }

    new (&m_buffer[head]) Event(std::move(evt));

    AtomicExchange(&m_head, (head + 1) & (MaxSize - 1));

    return true;
}

bool InputEventQueue::Pop(Event& outEvent)
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
      m_mouseLockStates(),
      m_ownerWindow(ownerWindow),
      m_isMouseLocked(false),
      m_syncToVirtualPosition(false)
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
            GetMouseLockStateAllocator().Free(state);
        }
    }

    delete m_eventQueue;
}

bool InputManager::IsMouseLocked() const
{
    return m_isMouseLocked;
}

void InputManager::PushMouseLockState(bool mouseLocked, bool syncToVirtualPosition)
{
    InputMouseLockState* mouseLockState = (InputMouseLockState*)GetMouseLockStateAllocator().Allocate();

    new (mouseLockState) InputMouseLockState;
    mouseLockState->locked = mouseLocked;
    mouseLockState->syncToVirtualPosition = syncToVirtualPosition;

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
        GetMouseLockStateAllocator().Free(lastState);
    }
}

InputMouseLockScope InputManager::AcquireMouseLock(bool syncToVirtualPosition)
{
    InputMouseLockState* mouseLockState = (InputMouseLockState*)GetMouseLockStateAllocator().Allocate();

    new (mouseLockState) InputMouseLockState;
    mouseLockState->locked = true;
    mouseLockState->syncToVirtualPosition = syncToVirtualPosition;

    ApplyMouseLockState(mouseLockState);

    return InputMouseLockScope { this, mouseLockState };
}

void InputManager::SetIsMouseLocked(bool locked)
{
    AssertOnThread(g_mainThread);

    if (!m_ownerWindow)
    {
        return;
    }

    if (m_isMouseLocked == locked)
    {
        return; // already set
    }

    m_ownerWindow->SetIsMouseLocked(locked);

    if (!locked)
    {
        if (m_syncToVirtualPosition)
        {
            // Set the new mouse position to the virtual mouse position we had before,
            // so things line up as expected
            m_ownerWindow->SetMousePosition(m_virtualMousePosition);
        }
    }

    // sync the virtual positions to the physical ones
    m_previousMousePosition = m_mousePosition;
    m_mousePosition = m_ownerWindow->GetMousePosition();

    m_previousVirtualMousePosition = m_previousMousePosition;
    m_virtualMousePosition = m_mousePosition;

    m_isMouseLocked = locked;
}

void InputManager::UpdateMousePosition(Event& event)
{
    AssertOnThread(g_mainThread);

    if (!m_ownerWindow)
    {
        return;
    }

    if (m_isMouseLocked)
    {
        const Vec2f deltas = event.IsAbsoluteMousePosition()
            ? event.GetMousePosition() - Vec2f(m_previousMousePosition)
            : event.GetMousePositionDeltas();

        // if locked, only update the virtual position
        Vec2i newVirtualMousePosition = Vec2i(Vec2f(m_virtualMousePosition) + deltas);
        newVirtualMousePosition = MathUtil::Clamp(newVirtualMousePosition, Vec2i::Zero(), m_ownerWindow->GetDimensions() - 1);

        m_virtualMousePosition = newVirtualMousePosition;

        return;
    }

    m_mousePosition = event.IsAbsoluteMousePosition()
        ? Vec2i(event.GetMousePosition())
        : Vec2i(Vec2f(m_previousMousePosition) + event.GetMousePositionDeltas());

    // not locked, keep virtual position synced with the physical one.
    m_virtualMousePosition = m_mousePosition;
}

void InputManager::UpdateWindowSize(Vec2i newSize)
{
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
    Mutex::Guard guard(m_mouseLockStatesMutex);

    if (!mouseLockState)
    {
        if (m_mouseLockStates.Empty() && IsOnThread(g_mainThread))
        {
            // apply default state
            SetIsMouseLocked(false);

            m_syncToVirtualPosition = false;
        }

        return;
    }

    m_mouseLockStates.PushBack(mouseLockState);

    if (IsOnThread(g_mainThread))
    {
        SetIsMouseLocked(mouseLockState->locked);

        m_syncToVirtualPosition = mouseLockState->syncToVirtualPosition;
    }
}

void InputManager::RemoveMouseLockState(InputMouseLockState* mouseLockState)
{
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
        GetMouseLockStateAllocator().Free(mouseLockState);
    }

    if (eraseIt == m_mouseLockStates.End() && IsOnThread(g_mainThread)) // was it at the end?
    {
        if (m_mouseLockStates.Any())
        {
            InputMouseLockState* nextMouseLockState = m_mouseLockStates.Back();
            AssertDebug(nextMouseLockState != nullptr);

            SetIsMouseLocked(nextMouseLockState->locked);

            m_syncToVirtualPosition = nextMouseLockState->syncToVirtualPosition;
        }
        else
        {
            SetIsMouseLocked(false); // default state
        }
    }
}

void InputManager::ProcessEvent(Event&& event)
{
    AssertOnThread(g_mainThread);

    const EventType eventType = event.GetType();

    switch (eventType)
    {
    case EventType::KEYDOWN:
        SetKey(event.GetKeyCode(), true);

        break;
    case EventType::KEYUP:
        SetKey(event.GetKeyCode(), false);

        break;
    case EventType::MOUSEBUTTON_DOWN:
        for (Bitset::BitIndex index : Bitset(event.GetMouseButtons()))
        {
            SetMouseButton(MouseButtonKey(index), true);
        }

        break;
    case EventType::MOUSEBUTTON_UP:
        for (Bitset::BitIndex index : Bitset(event.GetMouseButtons()))
        {
            SetMouseButton(MouseButtonKey(index), false);
        }

        break;
    case EventType::MOUSEMOTION:
        UpdateMousePosition(event);

        break;
    case EventType::WINDOW_RESIZED:
        UpdateWindowSize(event.GetWindowResizeDimensions());

        break;
    case EventType::WINDOW_FOCUS_LOST:
        if (m_isMouseLocked && m_ownerWindow && m_ownerWindow->IsMouseLocked())
        {
            m_ownerWindow->SetIsMouseLocked(false);
        }

        break;
    case EventType::WINDOW_FOCUS_GAINED:
        if (m_isMouseLocked && m_ownerWindow && !m_ownerWindow->IsMouseLocked())
        {
            m_ownerWindow->SetIsMouseLocked(true);
        }

        break;
    case EventType::WINDOW_CLOSE:
        break;
    default:
        break;
    }

    if (!m_eventQueue->Push(std::move(event)))
    {
        HYP_LOG(Input, Warning, "Input event queue full! Skipped event of type {}", eventType);
    }
}

void InputManager::BufferSwap()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);
}

bool InputManager::PollEvent(Event& outEvent)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    return m_eventQueue->Pop(outEvent);
}

void InputManager::MainThreadUpdate()
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    Mutex::Guard guard(m_mouseLockStatesMutex);

    if (m_mouseLockStates.Any())
    {
        InputMouseLockState* mouseLockState = m_mouseLockStates.Back();
        AssertDebug(mouseLockState != nullptr);

        SetIsMouseLocked(mouseLockState->locked);

        m_syncToVirtualPosition = mouseLockState->syncToVirtualPosition;
    }
    else
    {
        SetIsMouseLocked(false); // default state

        m_syncToVirtualPosition = false;
    }

    if (m_ownerWindow->IsMouseLocked())
    {
        m_ownerWindow->SetMousePosition(m_previousMousePosition);
        m_mousePosition = m_previousMousePosition;
    }
    else
    {
        m_previousMousePosition = m_mousePosition;
        m_mousePosition = m_ownerWindow->GetMousePosition();
    }

    m_previousVirtualMousePosition = m_virtualMousePosition;
}

#pragma endregion InputManager

} // namespace Hyperion
