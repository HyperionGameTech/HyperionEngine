#ifndef HYPERION_SCRIPTING_SERVICE_HPP
#define HYPERION_SCRIPTING_SERVICE_HPP

#include <scripting/Script.hpp>

#include <core/containers/FlatMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/functional/Delegate.hpp>

#include <core/filesystem/FilePath.hpp>

namespace hyperion {

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
    ManagedScript   *script;
};

class HYP_API ScriptingService
{
public:
    ScriptingService(
        const FilePath &watch_directory,
        const FilePath &intermediate_directory,
        const FilePath &binary_output_directory
    );
    ScriptingService(const ScriptingService &other)                 = delete;
    ScriptingService &operator=(const ScriptingService &other)      = delete;
    ScriptingService(ScriptingService &&other) noexcept             = delete;
    ScriptingService &operator=(ScriptingService &&other) noexcept  = delete;
    ~ScriptingService();

    void Start();
    void Stop();

    /*! \brief Called from game thread */
    void Update();

    /*! \brief To be called from ScriptingService thread only */
    void PushScriptEvent(const ScriptEvent &event);

    Delegate<void, const ManagedScript &>   OnScriptStateChanged;

private:
    bool HasEvents() const;

    UniquePtr<ScriptingServiceThread>       m_thread;

    Queue<ScriptEvent>                      m_script_event_queue;
    Mutex                                   m_script_event_queue_mutex;
    AtomicVar<uint32>                       m_script_event_queue_count;
};

} // namespace hyperion

#endif