/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Threading/Task.hpp>
#include <Core/Threading/AtomicVar.hpp>

#include <Core/Functional/Delegate.hpp>

namespace Hyperion {

struct ContentSyncState
{
    enum State
    {
        Failed = -1,
        NotStarted = 0,
        InProgress,
        Downloaded_Preparing,
        Finished
    } state = NotStarted;

    Result lastResult;
    Task<Result> currentTask;

    // Scaled up to allow two decimal places (10000 * (0..1))
    AtomicVar<uint32> progress = 0;

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

    bool IsInProgress() const
    {
        return currentTask.IsValid();
    }

    Result Wait()
    {
        if (IsInProgress())
        {
            lastResult = currentTask.Await();
            currentTask = {};
        }

        return lastResult;
    }
};

} // namespace Hyperion
