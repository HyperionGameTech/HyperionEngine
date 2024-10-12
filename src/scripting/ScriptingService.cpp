#include <scripting/ScriptingService.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/threading/Threads.hpp>

#include <dotnet/Assembly.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/DotNetSystem.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <type_traits>

namespace hyperion {

#pragma region ScriptTracker

class ScriptTracker
{
public:
    ScriptTracker()
    {
        UniquePtr<dotnet::Assembly> managed_assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly("HyperionScripting.dll");
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
    UniquePtr<dotnet::Assembly> m_assembly;
    UniquePtr<dotnet::Object>   m_object;
};

#pragma endregion ScriptTracker

#pragma region ScriptingServiceThread

using ScriptingServiceThreadCallback = void(*)(void *, ScriptEvent);

class ScriptingServiceThread : public TaskThread
{
public:
    ScriptingServiceThread(
        const FilePath &watch_directory,
        const FilePath &intermediate_directory,
        const FilePath &binary_output_directory,
        ScriptingServiceThreadCallback callback,
        void *callback_self_ptr
    ) : TaskThread(ThreadID::CreateDynamicThreadID(NAME("ScriptingServiceThread")), ThreadPriorityValue::LOWEST),
        m_script_tracker(new ScriptTracker),
        m_watch_directory(watch_directory),
        m_intermediate_directory(intermediate_directory),
        m_binary_output_directory(binary_output_directory),
        m_callback(callback),
        m_callback_self_ptr(callback_self_ptr)
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

protected:
    virtual void operator()() override
    {
        m_is_running.Set(true, MemoryOrder::RELAXED);

        {
            HYP_NAMED_SCOPE("Scripting service: Initialize");

            m_script_tracker->GetObject()->InvokeMethodByName<void>(
                "Initialize",
                m_watch_directory.Data(),
                m_intermediate_directory.Data(),
                m_binary_output_directory.Data(),
                (void *)m_callback,
                m_callback_self_ptr
            );
        }

        Queue<Scheduler::ScheduledTask> tasks;

        while (!m_stop_requested.Get(MemoryOrder::RELAXED)) {
            if (uint32 num_enqueued = m_scheduler.NumEnqueued()) {
                HYP_NAMED_SCOPE("Scripting service: Execute enqueued tasks");

                m_scheduler.AcceptAll(tasks);

                while (tasks.Any()) {
                    tasks.Pop().Execute();
                }
            }

            HYP_NAMED_SCOPE("Scripting service: invoke update on managed side");

            m_script_tracker->InvokeUpdate();
            Threads::Sleep(1000);
        }
    }

    ScriptTracker                   *m_script_tracker;
    FilePath                        m_watch_directory;
    FilePath                        m_intermediate_directory;
    FilePath                        m_binary_output_directory;
    ScriptingServiceThreadCallback  m_callback;
    void                            *m_callback_self_ptr;
};

#pragma endregion ScriptingServiceThread

#pragma region ScriptingService

ScriptingService::ScriptingService(const FilePath &watch_directory, const FilePath &intermediate_directory, const FilePath &binary_output_directory)
    : m_thread(MakeUnique<ScriptingServiceThread>(
        watch_directory,
        intermediate_directory,
        binary_output_directory,
        [](void *self_ptr, ScriptEvent event)
        {
            static_cast<ScriptingService *>(self_ptr)->PushScriptEvent(event);
        },
        this
      ))
{
    HYP_NAMED_SCOPE("ScriptingService: Initialize directories");

    if (!watch_directory.Exists()) {
        watch_directory.MkDir();
    }

    if (!intermediate_directory.Exists()) {
        intermediate_directory.MkDir();
    }

    if (!binary_output_directory.Exists()) {
        binary_output_directory.MkDir();
    }
}

ScriptingService::~ScriptingService()
{
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

void ScriptingService::Update()
{
    if (!HasEvents()) {
        return;
    }

    HYP_NAMED_SCOPE("ScriptingService: Update");

    Queue<ScriptEvent> script_event_queue;

    { // pull events from queue
        HYP_NAMED_SCOPE("ScriptingService: Pull events from queue");

        Mutex::Guard guard(m_script_event_queue_mutex);

        script_event_queue = std::move(m_script_event_queue);

        m_script_event_queue_count.Decrement(1, MemoryOrder::RELEASE);
    }

    if (script_event_queue.Empty()) {
        return;
    }

    {
        HYP_NAMED_SCOPE("ScriptingService: Process events");

        for (ScriptEvent &event : script_event_queue) {
            switch (event.type) {
            case ScriptEventType::STATE_CHANGED:
                OnScriptStateChanged(*event.script);

                break;
            default:
                HYP_LOG(ScriptingService, LogLevel::ERR, "Unknown script event received: {}", uint32(event.type));

                break;
            }
        }
    }
}

// Called from any thread - most likely from C# thread pool
// @TODO: Use scheduler to push task to game thread instead
void ScriptingService::PushScriptEvent(const ScriptEvent &event)
{
    Mutex::Guard guard(m_script_event_queue_mutex);

    m_script_event_queue.Push(event);

    m_script_event_queue_count.Increment(1, MemoryOrder::RELEASE);
}

bool ScriptingService::HasEvents() const
{
    return m_script_event_queue_count.Get(MemoryOrder::ACQUIRE);
}

#pragma endregion ScriptingService

} // namespace hyperion