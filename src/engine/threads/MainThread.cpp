/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <engine/threads/MainThread.hpp>
#include <engine/threads/RenderThread.hpp>

#include <core/threading/Threads.hpp>

#include <core/cli/CommandLine.hpp>

#include <input/InputManager.hpp>

#include <system/SystemEvent.hpp>
#include <system/AppContext.hpp>

namespace hyperion {

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

    Queue<Scheduler::ScheduledTask> tasks;
    if (uint32 numEnqueued = m_scheduler.NumEnqueued())
    {
        m_scheduler.AcceptAll(tasks);

        while (tasks.Any())
        {
            tasks.Pop().Execute();
        }
    }

    SystemEvent event;
    while (g_appContext->PollEvents(event))
    {
        g_inputManager->CheckEvent(&event);
    }

    g_inputManager->MainThreadUpdate();

#ifdef HYP_LIBUI
    uiMainSteps();
#endif

    if (s_renderOnMainThread
        && g_renderThreadInstance
        && g_renderThreadInstance->IsRunning())
    {
        g_renderThreadInstance->Update();

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

} // namespace hyperion