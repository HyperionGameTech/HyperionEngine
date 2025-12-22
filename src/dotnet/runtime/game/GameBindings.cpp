/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>

#include <engine/Game.hpp>

#include <engine/threads/SimThread.hpp>
#include <engine/EngineGlobals.hpp>

using namespace hyperion;

extern "C"
{
    HYP_EXPORT void Game_PostTask(Game* pGame, void (*pTaskFunc)())
    {
        Assert(pGame != nullptr);
        Assert(pTaskFunc != nullptr);

        if (IsOnThread(g_simThread))
        {
            // Execute immediately if already on the sim thread
            pTaskFunc();
            return;
        }

        g_simThreadInstance->GetScheduler().Enqueue(pTaskFunc, TaskEnqueueFlags::FIRE_AND_FORGET);
    }
} // extern "C"
