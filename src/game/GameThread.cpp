/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <game/GameThread.hpp>
#include <game/Game.hpp>
#include <util/GameCounter.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/math/MathUtil.hpp>

#include <core/Defines.hpp>

#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <scene/World.hpp>

#include <asset/Assets.hpp>

#include <rendering/debug/DebugDrawer.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

// #define HYP_GAME_THREAD_LOCKED 1

namespace hyperion {

HYP_DEFINE_LOG_CHANNEL(GameThread);

static constexpr float gameThreadTargetTicksPerSecond = 120.0f;

GameThread::GameThread()
    : Thread(g_gameThread, ThreadPriorityValue::HIGHEST)
{
}

void GameThread::SetGame(const Handle<Game>& game)
{
    if (IsRunning())
    {
        Task<void> future;

        GetScheduler().Enqueue([this, game = game, promise = future.Promise()]()
            {
                m_game = game;
                
                Assert(m_game != nullptr);

                InitObject(m_game);

                promise->Fulfill();
            });

        future.Await();

        return;
    }

    m_game = game;
}

void GameThread::operator()()
{
    GameCounter counter;

    Assert(m_game != nullptr);

    InitObject(m_game);

    Queue<Scheduler::ScheduledTask> tasks;
    Array<SystemEvent> events;

    while (!m_stopRequested.Get(MemoryOrder::RELAXED))
    {
#if HYP_GAME_THREAD_LOCKED
        if (counter.Waiting())
        {
            continue;
        }
#endif

        HYP_PROFILE_BEGIN;
        
        RenderApi_BeginFrame_GameThread();

        counter.NextTick();

        AssetManager::GetInstance()->Update(counter.delta);

        if (g_appContext->GetMainWindow()->GetInputEventSink().Poll(events))
        {
            for (SystemEvent& event : events)
            {
                g_appContext->GetInputManager()->CheckEvent(&event);
                
                m_game->HandleEvent(std::move(event));
            }

            events.Clear();
        }

        if (m_game.IsValid())
        {
            m_game->Update(counter.delta);
        }

        g_engineDriver->GetDebugDrawer()->Update(counter.delta);

        RenderApi_EndFrame_GameThread();
    }

    // flush scheduler
    m_scheduler.Flush([](auto& operation)
        {
            operation.Execute();
        });
}

} // namespace hyperion
