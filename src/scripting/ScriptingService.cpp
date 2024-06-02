#include <scripting/ScriptingService.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/threading/Threads.hpp>

#include <dotnet/Assembly.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/DotNetSystem.hpp>

#include <type_traits>

namespace hyperion {

#pragma region Interop structs
#pragma endregion Interop structs

#pragma region ScriptTracker

class ScriptTracker
{
public:
    ScriptTracker()
    {
        RC<dotnet::Assembly> managed_assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly("HyperionScripting.dll");
        AssertThrowMsg(managed_assembly != nullptr, "Failed to load HyperionScripting assembly");

        dotnet::Class *class_ptr = managed_assembly->GetClassObjectHolder().FindClassByName("ScriptTracker");
        AssertThrowMsg(class_ptr != nullptr, "Failed to load ScriptTracker class from HyperionScripting assembly");

        m_object = class_ptr->NewObject();
        m_assembly = std::move(managed_assembly);
    }

    dotnet::Object *GetObject() const
        { return m_object.Get(); } 

    void InvokeUpdate()
    {
        AssertThrowMsg(m_object != nullptr, "Cannot call InvokeUpdate(), ScriptTracker is not properly initialized");

        m_object->InvokeMethodByName<void>("Update");
    }

private:
    RC<dotnet::Assembly>        m_assembly;
    UniquePtr<dotnet::Object>   m_object;
};

#pragma endregion ScriptTracker

#pragma region ScriptingServiceThread

class ScriptingServiceThread : public TaskThread
{
public:
    ScriptingServiceThread()
        : TaskThread(ThreadID::CreateDynamicThreadID(HYP_NAME(ScriptingServiceThread)), ThreadPriorityValue::LOWEST),
          m_script_tracker(new ScriptTracker)
    {
    }

    virtual ~ScriptingServiceThread() override
    {
        delete m_script_tracker;
    }

    ScriptTracker *GetScriptTracker() const
        { return m_script_tracker; }

    virtual void Stop() override
    {
        TaskThread::Stop();
    }

    void StartTracking(Name name, const RC<Script> &script)
    {
        Threads::AssertOnThread(GetID());

        AssertThrow(script != nullptr);
        AssertThrowMsg(script->GetDescriptor().path.Size() + 1 <= script_max_path_length, "Invalid script path: must be <= %llu characters", script_max_path_length - 1);

        HYP_LOG(ScriptingService, LogLevel::DEBUG, "Calling C# start tracking with managedscript with state {}", script->GetManagedScript().state);

        m_script_tracker->GetObject()->InvokeMethodByName<void, Name, ManagedScript *>("StartTracking", Name(name), &script->GetManagedScript());

        m_tracked_scripts.Set(name, script);
    }

    void StopTracking(Name name)
    {
        Threads::AssertOnThread(GetID());

        auto it = m_tracked_scripts.Find(name);

        if (it == m_tracked_scripts.End()) {
            return;
        }

        m_script_tracker->GetObject()->InvokeMethodByName<void, Name>("StopTracking", Name(name));

        m_tracked_scripts.Erase(it);
    }

protected:
    virtual void operator()() override
    {
        m_is_running.Set(true, MemoryOrder::RELAXED);

        Queue<Scheduler::ScheduledTask> tasks;

        while (!m_stop_requested.Get(MemoryOrder::RELAXED)) {
            if (uint32 num_enqueued = m_scheduler.NumEnqueued()) {
                m_scheduler.AcceptAll(tasks);

                while (tasks.Any()) {
                    tasks.Pop().Execute();
                }
            }

            m_script_tracker->InvokeUpdate();
            Threads::Sleep(1000);
        }
    }

    ScriptTracker               *m_script_tracker;
    FlatMap<Name, RC<Script>>   m_tracked_scripts;
};

#pragma endregion ScriptingServiceThread

#pragma region ScriptingService

ScriptingService::ScriptingService()
    : m_thread(new ScriptingServiceThread)
{
}

ScriptingService::~ScriptingService()
{
}

void ScriptingService::StartTrackingScript(Name name, const RC<Script> &script)
{
    AssertThrowMsg(m_thread && m_thread->IsRunning(), "Cannot start tracking script: Service is not running");

    m_thread->GetScheduler().Enqueue([this, name, script]
    {
        m_thread->StartTracking(name, script);
    });
}

void ScriptingService::StopTrackingScript(Name name)
{
    if (!m_thread || !m_thread->IsRunning()) {
        return;
    }

    m_thread->GetScheduler().Enqueue([this, name]
    {
        m_thread->StopTracking(name);
    });
}

void ScriptingService::Start()
{
    m_thread->Start();
}

void ScriptingService::Stop()
{
    m_thread->Stop();
    m_thread->Join();
}

#pragma endregion ScriptingService

} // namespace hyperion