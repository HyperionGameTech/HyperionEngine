/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <engine/threads/MainThread.hpp>
#include <engine/threads/RenderThread.hpp>
#include <engine/threads/SimThread.hpp>

#include <core/threading/Threads.hpp>

#include <core/cli/CommandLine.hpp>

#include <input/InputManager.hpp>
#include <input/Event.hpp>

#include <system/AppContext.hpp>

namespace Hyperion {

extern const CommandLineArguments& CoreApi_GetCommandLineArguments();

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

    SetCurrentThreadObject(this);

    m_isRunning.Set(true, MemoryOrder::RELAXED);

    (*this)();

    return true;
}

void MainThread::Stop()
{
    Thread::Stop();

    m_isRunning.Set(false, MemoryOrder::RELAXED);
}

void MainThread::Update()
{
    HYP_PROFILE_BEGIN;
    AssertOnThread(g_mainThread);

    static const bool s_renderOnMainThread = CoreApi_GetCommandLineArguments()["RenderOnMainThread"].ToBool();
    static const bool s_simulateOnMainThread = CoreApi_GetCommandLineArguments()["SimulateOnMainThread"].ToBool();

    Queue<Scheduler::ScheduledTask> tasks;
    if (uint32 numEnqueued = m_scheduler.NumEnqueued())
    {
        m_scheduler.AcceptAll(tasks);

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
        g_simThreadInstance->Update();

        return;
    }
}

void MainThread::operator()()
{
    static const bool s_isDetached = CoreApi_GetCommandLineArguments()["Detached"].ToBool();

    if (!s_isDetached)
    {
        while (m_isRunning.Get(MemoryOrder::RELAXED))
        {
            Update();
        }
    }
}

} // namespace Hyperion