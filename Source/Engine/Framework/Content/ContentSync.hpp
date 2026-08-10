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
    Result lastResult;
    Task<Result> currentTask;

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
