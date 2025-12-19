#include <HyperionPch.hpp>

#include <scripting/ScriptingService.hpp>

#include <core/Core.hpp>

#include <core/threading/Threads.hpp>

#include <dotnet/Assembly.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/DotNETHost.hpp>

namespace hyperion {

#pragma region ScriptTracker

class ScriptTracker
{
public:
    ScriptTracker()
    {
        if (!DotNETHost::GetInstance().IsInitialized())
        {
            return;
        }

        const bool isEditor = CoreApi_GetCommandLineArguments()["Editor"].ToBool();

        RC<dotnet::Assembly> managedAssembly = DotNETHost::GetInstance().LoadAssembly("Hyperion.NET.Scripting.dll");
        Assert(managedAssembly != nullptr, "Failed to load Hyperion.NET.Scripting assembly");

        RC<dotnet::ManagedClass> managedClass = managedAssembly->FindClassByName("ScriptTracker");
        Assert(managedClass != nullptr, "Failed to load ScriptTracker class from Hyperion.NET.Scripting assembly");

        object = UniquePtr<dotnet::ManagedObject>(managedClass->NewObject());
        assembly = std::move(managedAssembly);
    }

    void InvokeUpdate()
    {
        if (!object || !object->IsValid())
        {
            return;
        }

        object->InvokeMethodByName<void>("Update");
    }

    RC<dotnet::Assembly> assembly;
    UniquePtr<dotnet::ManagedObject> object;
};

#pragma endregion ScriptTracker

#pragma region ScriptingServiceThread

using ScriptingServiceThreadCallback = void (*)(void*, ScriptEvent);

class ScriptingServiceThread : public TaskThread
{
public:
    ScriptingServiceThread(
        const FilePath& watchDirectory,
        const FilePath& intermediateDirectory,
        const FilePath& binaryOutputDirectory,
        ScriptingServiceThreadCallback callback,
        void* callbackSelfPtr)
        : TaskThread(ThreadId(NAME("ScriptingServiceThread")), ThreadPriorityValue::LOWEST),
          m_scriptTracker(new ScriptTracker),
          m_watchDirectory(watchDirectory),
          m_intermediateDirectory(intermediateDirectory),
          m_binaryOutputDirectory(binaryOutputDirectory),
          m_callback(callback),
          m_callbackSelfPtr(callbackSelfPtr)
    {
    }

    virtual ~ScriptingServiceThread() override
    {
        delete m_scriptTracker;
    }

    ScriptTracker* GetScriptTracker() const
    {
        return m_scriptTracker;
    }

    virtual void Stop() override
    {
        TaskThread::Stop();
    }

protected:
    virtual void operator()() override
    {
        if (!m_scriptTracker->object)
        {
            return;
        }

        {
            HYP_NAMED_SCOPE("Scripting service: Initialize");

            m_scriptTracker->object->InvokeMethodByName<void>(
                "Initialize",
                m_watchDirectory,
                m_intermediateDirectory,
                m_binaryOutputDirectory,
                (void*)m_callback,
                m_callbackSelfPtr);
        }

        Queue<Scheduler::ScheduledTask> tasks;

        while (!m_stopRequested.Get(MemoryOrder::RELAXED))
        {
            if (uint32 numEnqueued = m_scheduler.NumEnqueued())
            {
                HYP_NAMED_SCOPE("Scripting service: Execute enqueued tasks");

                m_scheduler.AcceptAll(tasks);

                while (tasks.Any())
                {
                    tasks.Pop().Execute();
                }
            }

            HYP_NAMED_SCOPE("Scripting service: invoke update on managed side");

            m_scriptTracker->InvokeUpdate();
            ThreadSleep(1000);
        }
    }

    ScriptTracker* m_scriptTracker;
    FilePath m_watchDirectory;
    FilePath m_intermediateDirectory;
    FilePath m_binaryOutputDirectory;
    ScriptingServiceThreadCallback m_callback;
    void* m_callbackSelfPtr;
};

#pragma endregion ScriptingServiceThread

#pragma region ScriptingService

ScriptingService::ScriptingService(const FilePath& watchDirectory, const FilePath& intermediateDirectory, const FilePath& binaryOutputDirectory)
    : m_thread(MakeUnique<ScriptingServiceThread>(
          watchDirectory,
          intermediateDirectory,
          binaryOutputDirectory,
          [](void* selfPtr, ScriptEvent event)
          {
              static_cast<ScriptingService*>(selfPtr)->PushScriptEvent(event);
          },
          this))
{
    HYP_NAMED_SCOPE("ScriptingService: Initialize directories");

    if (!watchDirectory.Exists())
    {
        watchDirectory.MkDir();
    }

    if (!intermediateDirectory.Exists())
    {
        intermediateDirectory.MkDir();
    }

    if (!binaryOutputDirectory.Exists())
    {
        binaryOutputDirectory.MkDir();
    }
}

ScriptingService::~ScriptingService()
{
    HYP_BREAKPOINT;
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
                HYP_LOG(ScriptingService, Error, "Unknown script event received: {}", uint32(event.type));

                break;
            }
        }
    }
}

// Called from any thread - most likely from C# thread pool
/// \todo : Use scheduler to push task to game thread instead
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

#pragma endregion ScriptingService

} // namespace hyperion
