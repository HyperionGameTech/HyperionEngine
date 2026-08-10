/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Threading/Task.hpp>

#include <Core/Functional/Delegate.hpp>

namespace Hyperion {

struct ContentSyncState
{
    enum State
    {
        Failed = -1,
        NotStarted = 0,
        InProgress,
        Finished
    } state = NotStarted;

    Result lastResult;
    Task<Result> currentTask;

    Delegate<void, State> OnStateChanged;

    void SetState(State state)
    {
        if (this->state == state)
        {
            return;
        }

        this->state = state;
        OnStateChanged(state);
    }

    bool IsSyncing() const
    {
        return currentTask.IsValid();
    }

    Result WaitForSync()
    {
        if (IsSyncing())
        {
            lastResult = currentTask.Await();
            currentTask = {};
        }

        return lastResult;
    }
};

} // namespace Hyperion
