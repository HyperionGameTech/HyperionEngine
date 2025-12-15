/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>

#include <game/Game.hpp>

#include <engine/threads/GameThread.hpp>
#include <engine/EngineGlobals.hpp>

using namespace hyperion;

extern "C"
{
    HYP_EXPORT void Game_PostTask(Game* pGame, void (*pTaskFunc)())
    {
        Assert(pGame != nullptr);
        Assert(pTaskFunc != nullptr);

        if (IsOnThread(g_gameThread))
        {
            // Execute immediately if already on the game thread
            pTaskFunc();
            return;
        }

        g_gameThreadInstance->GetScheduler().Enqueue(pTaskFunc, TaskEnqueueFlags::FIRE_AND_FORGET);
    }
} // extern "C"
