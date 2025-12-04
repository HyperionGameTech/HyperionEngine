/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/threads/MainThread.hpp>
#include <engine/threads/RenderThread.hpp>

#include <core/threading/Threads.hpp>

#include <core/cli/CommandLine.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineGlobals.hpp>

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

    const bool renderOnMainThread = CoreApi_GetCommandLineArguments()["RenderOnMainThread"].ToBool();

    SystemEvent event;
    while (g_appContext->PollEvents(event))
        ;

#ifdef HYP_LIBUI
    uiMainSteps();
#endif

    if (renderOnMainThread
        && g_renderThreadInstance
        && g_renderThreadInstance->IsRunning())
    {
        g_renderThreadInstance->Update();

        return;
    }
}

void MainThread::operator()()
{
    const bool isDetached = CoreApi_GetCommandLineArguments()["Detached"].ToBool();

    if (!isDetached)
    {
        while (m_isRunning.Get(MemoryOrder::RELAXED))
        {
            Update();
        }
    }
}

} // namespace hyperion