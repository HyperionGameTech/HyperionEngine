/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Utilities/Result.hpp>

namespace Hyperion {

struct ServerConnectionState
{
    enum State
    {
        NotStarted = 0,
        Connecting,
        Connecting_StartingNextTask,
        Connected,
        Failed
    } state = NotStarted;

    Result lastResult;

    Delegate<void, State> OnStateChanged;

    void SetState(State newState)
    {
        if (state == newState)
        {
            return;
        }

        state = newState;
        OnStateChanged(newState);
    }
};


} // namespace Hyperion