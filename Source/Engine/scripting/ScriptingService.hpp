#pragma once
#include <scripting/Script.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/FlatMap.hpp>

#include <Core/threading/Mutex.hpp>
#include <Core/threading/AtomicVar.hpp>
#include <Core/threading/TaskThread.hpp>

#include <Core/functional/Delegate.hpp>

#include <Core/filesystem/FilePath.hpp>

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
    ScriptDesc* script;
};

class HYP_API ScriptingService
{
public:
    ScriptingService(
        const Array<FilePath>& watchDirectories,
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

    Delegate<void, const ScriptDesc&> OnScriptStateChanged;

private:
    bool HasEvents() const;

    UniquePtr<ScriptingServiceThread> m_thread;

    Queue<ScriptEvent> m_scriptEventQueue;
    Mutex m_scriptEventQueueMutex;
    AtomicVar<uint32> m_scriptEventQueueCount;
};

} // namespace Hyperion
