/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Task.hpp>
#include <core/threading/Scheduler.hpp>

namespace hyperion {
namespace threading {

#pragma region TaskCallbackChain

TaskCallbackChain::TaskCallbackChain(TaskCallbackChain &&other) noexcept
{
    Mutex::Guard guard(other.m_mutex);

    m_callbacks = std::move(other.m_callbacks);
    m_num_callbacks.Set(other.m_num_callbacks.Exchange(0, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);
}

TaskCallbackChain &TaskCallbackChain::operator=(TaskCallbackChain &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    Mutex::Guard this_guard(m_mutex);
    Mutex::Guard other_guard(other.m_mutex);

    m_callbacks = std::move(other.m_callbacks);
    m_num_callbacks.Set(other.m_num_callbacks.Exchange(0, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);

    return *this;
}

void TaskCallbackChain::Add(Proc<void()> &&callback)
{
    // @TODO: Smarter implementation possibly using semaphores that are set up with a value when task is first initialized,
    // need a way to tell if the added callback will never be executed because the task completed.
    Mutex::Guard guard(m_mutex);

    m_callbacks.PushBack(std::move(callback));

    m_num_callbacks.Increment(1, MemoryOrder::RELEASE);
}

void TaskCallbackChain::operator()()
{
    if (m_num_callbacks.Get(MemoryOrder::ACQUIRE)) {
        Mutex::Guard guard(m_mutex);

        for (Proc<void()> &proc : m_callbacks) {
            proc();
        }
    }
}

#pragma endregion TaskCallbackChain

#pragma region TaskBase

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

#pragma endregion TaskBase

} // namespace threading
} // namespace hyperion