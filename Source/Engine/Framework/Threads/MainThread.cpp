/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineDriver.hpp>

#include <Framework/Threads/MainThread.hpp>
#include <Framework/Threads/RenderThread.hpp>
#include <Framework/Threads/SimThread.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/ThreadSignal.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#ifdef HYP_STEAM_SDK
#include <Steam/SteamInput.hpp>
#endif // HYP_STEAM_SDK

#include <System/AppContext.hpp>

#include <semaphore>

namespace Hyperion {

namespace CoreApi {
CORE_API extern const CommandLineArguments& GetCommandLineArguments();
} // namespace CoreApi

extern ThreadSignal g_renderInitSignal;

MainThread::MainThread()
    : Thread(g_mainThread, ThreadPriorityValue::HIGHEST)
{
    // Needs to initialize before Start() is called!
    InitThreadAllocator();
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

    HYP_DEFER({ m_threadAllocator->Reset(); });

    Array<Scheduler::ScheduledTask, ThreadAllocator> tasks;
    if (uint32 numEnqueued = m_scheduler->NumEnqueued())
    {
        m_scheduler->AcceptAll(tasks);

        for (auto it = tasks.Begin(); it != tasks.End(); ++it)
        {
            it->Execute();
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

#ifdef HYP_STEAM_SDK
    Steam::SteamInputManager::GetInstance().Update();
#endif // HYP_STEAM_SDK

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
