#pragma once
#include <scripting/Script.hpp>

#include <core/containers/FlatMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/functional/Delegate.hpp>

#include <core/filesystem/FilePath.hpp>

namespace Hyperion {

class ScriptTracker;

class ScriptingServiceThread;

enum class ScriptEventType : uint32
{
    NONE,
    STATE_CHANGED
};

struct ScriptEvent
{
    ScriptEventType type;
    ScriptData* script;
};

class HYP_API ScriptingService
{
public:
    ScriptingService(
        const FilePath& watchDirectory,
        const FilePath& intermediateDirectory,
        const FilePath& binaryOutputDirectory);
    ScriptingService(const ScriptingService& other) = delete;
    ScriptingService& operator=(const ScriptingService& other) = delete;
    ScriptingService(ScriptingService&& other) noexcept = delete;
    ScriptingService& operator=(ScriptingService&& other) noexcept = delete;
    ~ScriptingService();

    void Start();
    void Stop();

    /*! \brief Called from sim thread */
    void Update();

    /*! \brief To be called from ScriptingService thread only */
    void PushScriptEvent(const ScriptEvent& event);

    Delegate<void, const ScriptData&> OnScriptStateChanged;

private:
    bool HasEvents() const;

    UniquePtr<ScriptingServiceThread> m_thread;

    Queue<ScriptEvent> m_scriptEventQueue;
    Mutex m_scriptEventQueueMutex;
    AtomicVar<uint32> m_scriptEventQueueCount;
};

} // namespace Hyperion
