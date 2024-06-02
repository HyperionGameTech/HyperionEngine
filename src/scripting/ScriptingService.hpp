#ifndef HYPERION_SCRIPTING_SERVICE_HPP
#define HYPERION_SCRIPTING_SERVICE_HPP

#include <scripting/Script.hpp>

#include <core/containers/FlatMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/TaskThread.hpp>

namespace hyperion {

class ScriptTracker;

class ScriptingServiceThread;

class HYP_API ScriptingService
{
public:
    ScriptingService();
    ScriptingService(const ScriptingService &other)                 = delete;
    ScriptingService &operator=(const ScriptingService &other)      = delete;
    ScriptingService(ScriptingService &&other) noexcept             = delete;
    ScriptingService &operator=(ScriptingService &&other) noexcept  = delete;
    ~ScriptingService();

    void StartTrackingScript(Name name, const RC<Script> &script);
    void StopTrackingScript(Name name);

    void Start();
    void Stop();

private:
    UniquePtr<ScriptingServiceThread>   m_thread;
};

} // namespace hyperion

#endif