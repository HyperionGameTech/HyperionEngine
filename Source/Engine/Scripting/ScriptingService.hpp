#pragma once
#include <Scripting/Script.hpp>

#include <Core/containers/Queue.hpp>

#include <Core/threading/Mutex.hpp>
#include <Core/threading/AtomicVar.hpp>

#include <Core/functional/Delegate.hpp>

namespace Hyperion {

enum class ScriptEventType : uint32
{
    NONE,
    STATE_CHANGED
};

struct ScriptEvent
{
    ScriptEventType type;
    ScriptDesc* script;
};

class ENGINE_API ScriptingService
{
public:
    ScriptingService() = default;
    ScriptingService(const ScriptingService& other) = delete;
    ScriptingService& operator=(const ScriptingService& other) = delete;
    ScriptingService(ScriptingService&& other) noexcept = delete;
    ScriptingService& operator=(ScriptingService&& other) noexcept = delete;
    ~ScriptingService() = default;

    /*! \brief Called from sim thread */
    void Update();

    /*! \brief Thread-safe event push (can be called from any thread) */
    void PushScriptEvent(const ScriptEvent& event);

    Delegate<void, const ScriptDesc&> OnScriptStateChanged;

private:
    bool HasEvents() const;

    Queue<ScriptEvent> m_scriptEventQueue;
    Mutex m_scriptEventQueueMutex;
    AtomicVar<uint32> m_scriptEventQueueCount;
};

} // namespace Hyperion
