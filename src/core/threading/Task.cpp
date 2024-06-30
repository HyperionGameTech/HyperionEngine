/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Task.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {
namespace threading {

bool TaskBase::Cancel()
{
    if (!m_id.IsValid() || !m_assigned_scheduler) {
        return false;
    }

    if (m_assigned_scheduler->Dequeue(m_id)) {
        m_id = { };
        m_assigned_scheduler = nullptr;

        return true;
    }

    return false;
}

void TaskBase::Await_Internal() const
{
    AssertThrow(IsValid());

    while (!IsCompleted()) {
        HYP_WAIT_IDLE();
    }

    // if (IsCompleted()) {
    //     return;
    // }

    // m_assigned_scheduler->Await(m_id);

#ifdef HYP_DEBUG_MODE
    AssertThrow(IsCompleted());
#endif
}

void TaskBase::LogWarning(ANSIStringView message) const
{
    HYP_LOG(Tasks, LogLevel::WARNING, "{}", message);
}

} // namespace threading
} // namespace hyperion