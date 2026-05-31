/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Scheduler.hpp>

#include <Framework/Game.hpp>

#include <Framework/Threads/SimThread.hpp>
#include <Framework/EngineGlobals.hpp>

using namespace Hyperion;

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
