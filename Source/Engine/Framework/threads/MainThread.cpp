/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineDriver.hpp>

#include <Framework/threads/MainThread.hpp>
#include <Framework/threads/RenderThread.hpp>
#include <Framework/threads/SimThread.hpp>

#include <Core/threading/Threads.hpp>
#include <Core/threading/ThreadSignal.hpp>

#include <Core/cli/CommandLine.hpp>

#include <input/InputManager.hpp>
#include <input/Event.hpp>

#include <system/AppContext.hpp>

#include <semaphore>

namespace Hyperion {

namespace CoreApi {
CORE_API extern const CommandLineArguments& GetCommandLineArguments();
} // namespace CoreApi

extern ThreadSignal g_renderInitSignal;

MainThread::MainThread()
    : Thread(g_mainThread, ThreadPriorityValue::HIGHEST)
{
}

MainThread::~MainThread()
{
}

bool MainThread::Start()
{
    AssertOnThread(g_mainThread);

    AssertDebug(!IsRunning());

    // Should already be set in InitThreads()
    AssertDebug(CurrentThreadObject() == this);

    m_isRunning.Store(true);

    (*this)();

    return true;
}

void MainThread::Stop()
{
    Thread::Stop();

    m_isRunning.Store(false);
}

void MainThread::Update()
{
    HYP_PROFILE_BEGIN;
    AssertOnThread(g_mainThread);

    static const bool s_renderOnMainThread = CoreApi::GetCommandLineArguments()["RenderOnMainThread"].ToBool();
    static const bool s_simulateOnMainThread = CoreApi::GetCommandLineArguments()["SimulateOnMainThread"].ToBool();

    Queue<Scheduler::ScheduledTask> tasks;
    if (uint32 numEnqueued = m_scheduler->NumEnqueued())
    {
        m_scheduler->AcceptAll(tasks);

        while (tasks.Any())
        {
            tasks.Pop().Execute();
        }
    }

    Event event;
    while (g_appContext->PollEvents(event))
    {
        if (event.GetWindow() != nullptr)
        {
            event.GetWindow()->GetInputManager()->ProcessEvent(std::move(event));
        }
    }

    for (ApplicationWindow* window : g_appContext->GetWindows())
    {
        window->GetInputManager()->MainThreadUpdate();
    }

    if (s_renderOnMainThread
        && g_renderThreadInstance
        && g_renderThreadInstance->IsRunning())
    {
        g_renderThreadInstance->Update();

        return;
    }

    if (s_simulateOnMainThread
        && g_simThreadInstance
        && g_simThreadInstance->IsRunning())
    {

        // wait until render thread is finished initializing
        // until we call Update() - otherwise, sim(main) thread will
        // try to go into lockstep with RT, and that may be waiting on
        // the main thread to do stuff for Cocoa (dispatch_sync)
        static bool s_isRenderThreadInit = false;

        if (!s_isRenderThreadInit)
        {
            if (!g_renderInitSignal.IsSignalled())
            {
                return;
            }

            s_isRenderThreadInit = true;
        }

        g_simThreadInstance->Update();

        return;
    }
}

void MainThread::operator()()
{
    static const bool s_isDetached = CoreApi::GetCommandLineArguments()["Detached"].ToBool();

    if (!s_isDetached)
    {
        while (m_isRunning.Load())
        {
            Update();
        }
    }
}

} // namespace Hyperion
