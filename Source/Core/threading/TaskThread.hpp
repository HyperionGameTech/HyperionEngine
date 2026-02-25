/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/threading/Thread.hpp>
#include <Core/threading/Scheduler.hpp>
#include <Core/containers/Queue.hpp>
#include <Core/math/MathUtil.hpp>
#include <Core/Defines.hpp>

#include <Core/Types.hpp>

namespace Hyperion {
namespace threading {

class ThreadId;
class TaskThreadPool;

class HYP_API TaskThread : public Thread<Scheduler>
{
public:
    TaskThread(const ThreadId& threadId, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);
    TaskThread(Name name, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);

    virtual ~TaskThread() override = default;

    void SetPriority(ThreadPriorityValue priority);

    HYP_FORCE_INLINE void SetOwnerPool(TaskThreadPool* pool)
    {
        m_ownerPool = pool;
    }

    HYP_FORCE_INLINE TaskThreadPool* GetOwnerPool() const
    {
        return m_ownerPool;
    }

    HYP_FORCE_INLINE bool IsFree() const
    {
        return NumTasks() == 0;
    }

    HYP_FORCE_INLINE uint32 NumTasks() const
    {
        return m_numTasks.Get(MemoryOrder::ACQUIRE);
    }

protected:
    /*! \brief Method to be executed each tick of the task thread, before executing tasks.
     *  Used by derived classes to inject custom logic. */
    virtual void BeforeExecuteTasks()
    {
    }

    /*! \brief Method to be executed each tick of the task thread, after executing tasks.
     *  Used by derived classes to inject custom logic. */
    virtual void AfterExecuteTasks()
    {
    }

    virtual void operator()() override;

    AtomicVar<uint32> m_numTasks;

    TaskThreadPool* m_ownerPool = nullptr;
};

} // namespace threading

using threading::TaskThread;

} // namespace Hyperion
