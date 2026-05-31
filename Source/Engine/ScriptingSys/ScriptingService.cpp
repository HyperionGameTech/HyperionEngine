#include <HyperionPch.hpp>

#include <scripting/ScriptingService.hpp>

#include <Core/Core.hpp>

namespace Hyperion {

void ScriptingService::Update()
{
    if (!HasEvents())
    {
        return;
    }

    HYP_NAMED_SCOPE("ScriptingService: Update");

    Queue<ScriptEvent> scriptEventQueue;

    { // pull events from queue
        HYP_NAMED_SCOPE("ScriptingService: Pull events from queue");

        Mutex::Guard guard(m_scriptEventQueueMutex);

        scriptEventQueue = std::move(m_scriptEventQueue);

        m_scriptEventQueueCount.Decrement(scriptEventQueue.Size(), MemoryOrder::RELEASE);
    }

    if (scriptEventQueue.Empty())
    {
        return;
    }

    {
        HYP_NAMED_SCOPE("ScriptingService: Process events");

        for (ScriptEvent& event : scriptEventQueue)
        {
            switch (event.type)
            {
            case ScriptEventType::STATE_CHANGED:
                OnScriptStateChanged(*event.script);

                break;
            default:
                HYP_LOG(Engine, Error, "Unknown script event received: {}", uint32(event.type));

                break;
            }
        }
    }
}

void ScriptingService::PushScriptEvent(const ScriptEvent& event)
{
    Mutex::Guard guard(m_scriptEventQueueMutex);

    m_scriptEventQueue.Push(event);

    m_scriptEventQueueCount.Increment(1, MemoryOrder::RELEASE);
}

bool ScriptingService::HasEvents() const
{
    return m_scriptEventQueueCount.Get(MemoryOrder::ACQUIRE);
}

} // namespace Hyperion
