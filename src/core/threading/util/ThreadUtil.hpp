/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/threading/Threads.hpp>
#include <core/threading/Scheduler.hpp>
#include <core/threading/Task.hpp>

#include <core/utilities/Tuple.hpp>

namespace hyperion {
namespace threading {

#if 0
template <class ReturnType, class... ArgTypes>
static inline void CallOnMainThread(ReturnType (*func)(ArgTypes...), ArgTypes... args)
{
    Threads::GetThread(g_mainThread)->GetScheduler().Enqueue([func, tupleArgs = Tuple<ArgTypes...>(std::forward<ArgTypes>(args)...)]()
        {
            return Apply([func]<class... OtherArgs>(OtherArgs&&... args)
                {
                    return func(std::forward<OtherArgs>(args)...);
                },
                std::move(tupleArgs));
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
}
#endif

} // namespace threading

// using threading::CallOnMainThread;

} // namespace hyperion
