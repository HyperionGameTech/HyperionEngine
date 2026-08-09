/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Bitset.hpp>

#include <Core/Utilities/Optional.hpp>
#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Utilities/Span.hpp>

#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/SharedMutex.hpp>
#include <Core/Threading/Semaphore.hpp>

#include <Core/Logging/LoggerFwd.hpp>

#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Util.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

CORE_API HYP_DECLARE_LOG_CHANNEL(Threading);

enum class TaskEnqueueFlags : uint32
{
    NONE = 0x0,
    FIRE_AND_FORGET = 0x1
};

HYP_MAKE_ENUM_FLAGS(TaskEnqueueFlags);

namespace functional {

template <class FunctionSignature>
class Proc;

} // namespace functional

namespace threading {

class TaskThread;
class TaskBatch;
class SchedulerBase;
class TaskBase;

class TaskCompleteNotifier final : public Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>
{
public:
    TaskCompleteNotifier()
        : Semaphore(0)
    {
    }

    TaskCompleteNotifier(const TaskCompleteNotifier& other) = delete;
    TaskCompleteNotifier& operator=(const TaskCompleteNotifier& other) = delete;

    TaskCompleteNotifier(TaskCompleteNotifier&& other) noexcept = delete;
    TaskCompleteNotifier& operator=(TaskCompleteNotifier&& other) noexcept = delete;

    ~TaskCompleteNotifier() = default;

    /*! \brief Sets the number of tasks that need to be completed before the notifier is signalled.
     *  This is typically called when the task batch is created, and the number of tasks is known.
     */
    void SetTargetValue(uint32 numTasks)
    {
        Semaphore::SetValue(int32(numTasks));
    }

    /*! \brief Resets the notifier to its initial state (no tasks) */
    void Reset()
    {
        Semaphore::SetValue(0);
    }

    /*! \brief Waits for the task to be signalled as complete (when value is zero) */
    void Await()
    {
        Semaphore::Acquire();
    }
};

using OnTaskCompletedCallback = functional::Proc<void()>;

struct TaskID
{
    static TaskID Invalid()
    {
        return TaskID { 0 };
    }

    uint32 value { 0 };

    TaskID& operator=(uint32 id)
    {
        value = id;

        return *this;
    }

    TaskID& operator=(const TaskID& other) = default;

    HYP_FORCE_INLINE bool operator==(uint32 id) const
    {
        return value == id;
    }

    HYP_FORCE_INLINE bool operator!=(uint32 id) const
    {
        return value != id;
    }

    HYP_FORCE_INLINE bool operator==(const TaskID& other) const
    {
        return value == other.value;
    }

    HYP_FORCE_INLINE bool operator!=(const TaskID& other) const
    {
        return value != other.value;
    }

    HYP_FORCE_INLINE bool operator<(const TaskID& other) const
    {
        return value < other.value;
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return value != 0;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return value == 0;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return value != 0;
    }
};

/*! \brief Thread safe set of callbacks to be invoked when a task completes.
 *  Once the chain has been completed, any callback added to it is invoked immediately on the
 *  calling thread, so a callback can be registered without racing against the task finishing. */
class CORE_API TaskCallbackChain
{
public:
    using CallbackList = Array<functional::Proc<void()>, DynamicAllocator>;

    TaskCallbackChain() = default;

    TaskCallbackChain(const TaskCallbackChain& other) = delete;
    TaskCallbackChain& operator=(const TaskCallbackChain& other) = delete;

    TaskCallbackChain(TaskCallbackChain&& other) noexcept;
    TaskCallbackChain& operator=(TaskCallbackChain&& other) noexcept;

    ~TaskCallbackChain();

    HYP_FORCE_INLINE bool IsCompleted() const
    {
        return m_completed.Get(MemoryOrder::ACQUIRE);
    }

    /*! \brief Add \p callback to be invoked when the task completes.
     *  If the task has already completed, \p callback is invoked immediately on the calling thread. */
    void Add(functional::Proc<void()>&& callback);

    /*! \brief Mark the chain as completed and hand the pending callbacks to the caller to invoke.
     *  Callbacks are not invoked here so that the owner can signal waiting threads first; as soon as
     *  it does, the object owning this chain may be destroyed, so the callbacks have to live in the
     *  caller's own storage by then. */
    CallbackList Detach();

private:
    CallbackList m_callbacks;
    AtomicVar<bool> m_completed;
    Mutex m_mutex;
};

class ITaskExecutor
{
public:
    virtual ~ITaskExecutor() = default;

    virtual TaskID GetTaskID() const = 0;

    virtual bool IsCompleted() const = 0;

    virtual TaskCallbackChain& GetCallbackChain() = 0;
};

class CORE_API TaskExecutorBase : public ITaskExecutor
{
    template <class T>
    friend class Task;

public:
    /*! \brief Tracks who is left to delete an executor that supports deferred deletion.
     *  The owning Task and the thread that completes the task each mark their side; whichever gets
     *  there last performs the deletion. */
    enum class DeletionState : uint8
    {
        ALIVE = 0,
        OWNER_RELEASED,
        COMPLETED
    };

    TaskExecutorBase()
        : m_id(TaskID::Invalid()),
          m_initiatorThreadId(ThreadId::Invalid()),
          m_assignedScheduler(nullptr),
          m_deferredDeletionEnabled(false)
    {
        // set notifier to initial value of 1 (one task)
        m_notifier.Produce(1);
    }

    TaskExecutorBase(const TaskExecutorBase& other) = delete;
    TaskExecutorBase& operator=(const TaskExecutorBase& other) = delete;

    TaskExecutorBase(TaskExecutorBase&& other) noexcept
        : m_id(other.m_id),
          m_initiatorThreadId(other.m_initiatorThreadId),
          m_assignedScheduler(other.m_assignedScheduler),
          m_deferredDeletionEnabled(other.m_deferredDeletionEnabled)
    {
        m_callbackChain = std::move(other.m_callbackChain);

        other.m_id = TaskID::Invalid();
        other.m_initiatorThreadId = ThreadId::Invalid();
        other.m_assignedScheduler = nullptr;
    }

    TaskExecutorBase& operator=(TaskExecutorBase&& other) noexcept = delete;

    virtual ~TaskExecutorBase() override = default;

    virtual TaskID GetTaskID() const override final
    {
        return m_id;
    }

    /*! \internal This function is used by the Scheduler to set the task Id. */
    HYP_FORCE_INLINE void SetTaskID(TaskID id)
    {
        m_id = id;
    }

    HYP_FORCE_INLINE const ThreadId& GetInitiatorThreadId() const
    {
        return m_initiatorThreadId;
    }

    /*! \internal This function is used by the Scheduler to set the initiator thread Id. */
    HYP_FORCE_INLINE void SetInitiatorThreadId(const ThreadId& initiatorThreadId)
    {
        m_initiatorThreadId = initiatorThreadId;
    }

    HYP_FORCE_INLINE SchedulerBase* GetAssignedScheduler() const
    {
        return m_assignedScheduler;
    }

    /*! \internal This function is used by the Scheduler to set the assigned scheduler. */
    HYP_FORCE_INLINE void SetAssignedScheduler(SchedulerBase* assignedScheduler)
    {
        m_assignedScheduler = assignedScheduler;
    }

    HYP_FORCE_INLINE TaskCompleteNotifier& GetNotifier()
    {
        return m_notifier;
    }

    HYP_FORCE_INLINE const TaskCompleteNotifier& GetNotifier() const
    {
        return m_notifier;
    }

    virtual bool IsCompleted() const override final
    {
        return m_notifier.IsInSignalState();
    }

    virtual void Execute() = 0;

    virtual TaskCallbackChain& GetCallbackChain() override final
    {
        return m_callbackChain;
    }

    /*! \internal Signals that the task has finished running: detaches the completion callbacks,
     *  releases \p notifier (which may be a notifier shared by an entire TaskBatch) and then invokes
     *  the callbacks.
     *  \note This executor must not be touched by the caller afterwards - a thread waiting on the
     *  task is free to destroy it as soon as the notifier is released, and an executor with deferred
     *  deletion enabled may delete itself here. */
    void Complete(TaskCompleteNotifier* notifier, ProcRef<void()> onNotifierSignalled = ProcRef<void()>(nullptr));

    /*! \internal Called by the owning Task when it goes away, for executors that support deferred
     *  deletion. Deletion is left to whichever of the owner and the completing thread comes last.
     *  \returns True if the caller is responsible for deleting this executor. */
    inline bool RelinquishOwnership()
    {
        HYP_CORE_ASSERT(m_deferredDeletionEnabled, "Executor does not support deferred deletion");

        return m_deletionState.Exchange(DeletionState::OWNER_RELEASED, MemoryOrder::ACQUIRE_RELEASE) == DeletionState::COMPLETED;
    }

protected:
    TaskID m_id;
    ThreadId m_initiatorThreadId;
    SchedulerBase* m_assignedScheduler;
    TaskCompleteNotifier m_notifier;
    TaskCallbackChain m_callbackChain;
    AtomicVar<DeletionState> m_deletionState;
    bool m_deferredDeletionEnabled;
};

CORE_API extern void Task_DeleteAllDeferredTasks();
CORE_API extern void Task_DeferTaskDeletion(TaskExecutorBase* taskExecutor);

template <class ReturnType>
class TaskExecutorInstance : public TaskExecutorBase
{
public:
    using Function = functional::Proc<ReturnType()>;
    using Base = TaskExecutorBase;

    template <class Lambda>
    TaskExecutorInstance(Lambda&& fn)
        : m_fn(std::forward<Lambda>(fn))
    {
    }

    TaskExecutorInstance(const TaskExecutorInstance& other) = delete;
    TaskExecutorInstance& operator=(const TaskExecutorInstance& other) = delete;

    TaskExecutorInstance(TaskExecutorInstance&& other) noexcept
        : Base(static_cast<Base&&>(other)),
          m_fn(std::move(other.m_fn)),
          m_resultValue(std::move(other.m_resultValue))
    {
    }

    TaskExecutorInstance& operator=(TaskExecutorInstance&& other) noexcept = delete;

    virtual ~TaskExecutorInstance() override = default;

    HYP_FORCE_INLINE ReturnType& Result() &
    {
        return m_resultValue.Get();
    }

    HYP_FORCE_INLINE const ReturnType& Result() const&
    {
        return m_resultValue.Get();
    }

    HYP_FORCE_INLINE ReturnType Result() &&
    {
        return std::move(m_resultValue.Get());
    }

    HYP_FORCE_INLINE ReturnType Result() const&&
    {
        return m_resultValue.Get();
    }

    virtual void Execute() override
    {
        HYP_CORE_ASSERT(m_fn.IsValid());

        m_resultValue.Emplace(m_fn());
    }

protected:
    Function m_fn;
    Optional<ReturnType> m_resultValue;
};

/*! \brief Specialization for void return type. */
template <>
class TaskExecutorInstance<void> : public TaskExecutorBase
{
    using Function = functional::Proc<void()>;

    struct Functor
    {

    };

public:
    using Base = TaskExecutorBase;

    template <class Lambda>
    TaskExecutorInstance(Lambda&& fn)
        : m_fn(std::forward<Lambda>(fn))
    {
    }

    TaskExecutorInstance(const TaskExecutorInstance& other) = delete;
    TaskExecutorInstance& operator=(const TaskExecutorInstance& other) = delete;

    TaskExecutorInstance(TaskExecutorInstance&& other) noexcept
        : Base(static_cast<Base&&>(other)),
          m_fn(std::move(other.m_fn))
    {
    }

    TaskExecutorInstance& operator=(TaskExecutorInstance&& other) noexcept = delete;

    virtual ~TaskExecutorInstance() override = default;

    virtual void Execute() override
    {
        HYP_CORE_ASSERT(m_fn.IsValid());

        m_fn();
    }

protected:
    Function m_fn;
};

template <class ReturnType>
class TaskFuture;

template <class ReturnType>
class TaskPromise final : public TaskExecutorInstance<ReturnType>
{
public:
    using Base = TaskExecutorInstance<ReturnType>;

    TaskPromise(TaskBase* task)
        : Base(static_cast<ReturnType (*)(void)>(nullptr)),
          m_task(task)
    {
        Base::m_deferredDeletionEnabled = true;
    }

    TaskPromise(const TaskPromise& other) = delete;
    TaskPromise& operator=(const TaskPromise& other) = delete;

    TaskPromise(TaskPromise&& other) noexcept
        : Base(static_cast<Base&&>(other)),
          m_task(other.m_task)
    {
        other.m_task = nullptr;
    }

    TaskPromise& operator=(TaskPromise&& other) noexcept = delete;

    virtual ~TaskPromise() override = default;

    HYP_FORCE_INLINE TaskBase* GetTask() const
    {
        return m_task;
    }

    /*! \brief Resolve the task with \p value, waking any waiting threads and running the task's
     *  completion callbacks.
     *  \note This promise must not be touched afterwards - the owning Task may have already gone
     *  away, in which case this call deletes it. */
    void Fulfill(ReturnType&& value)
    {
        HYP_CORE_ASSERT(!Base::IsCompleted());

        Base::m_resultValue.Set(std::move(value));

        Base::Complete(&Base::GetNotifier());
    }

    /*! \brief Resolve the task with \p value, waking any waiting threads and running the task's
     *  completion callbacks.
     *  \note This promise must not be touched afterwards - the owning Task may have already gone
     *  away, in which case this call deletes it. */
    void Fulfill(const ReturnType& value)
    {
        HYP_CORE_ASSERT(!Base::IsCompleted());

        Base::m_resultValue.Set(value);

        Base::Complete(&Base::GetNotifier());
    }

protected:
    virtual void Execute() override final
    {
    }

    TaskBase* m_task;
};

template <>
class TaskPromise<void> final : public TaskExecutorInstance<void>
{
public:
    using Base = TaskExecutorInstance<void>;

    TaskPromise(TaskBase* task)
        : Base(static_cast<void (*)(void)>(nullptr)),
          m_task(task)
    {
        Base::m_deferredDeletionEnabled = true;
    }

    TaskPromise(const TaskPromise& other) = delete;
    TaskPromise& operator=(const TaskPromise& other) = delete;

    TaskPromise(TaskPromise&& other) noexcept
        : Base(static_cast<Base&&>(other)),
          m_task(other.m_task)
    {
        other.m_task = nullptr;
    }

    TaskPromise& operator=(TaskPromise&& other) noexcept = delete;

    virtual ~TaskPromise() override = default;

    HYP_FORCE_INLINE TaskBase* GetTask() const
    {
        return m_task;
    }

    /*! \brief Resolve the task, waking any waiting threads and running the task's completion
     *  callbacks.
     *  \note This promise must not be touched afterwards - the owning Task may have already gone
     *  away, in which case this call deletes it. */
    void Fulfill()
    {
        HYP_CORE_ASSERT(!Base::IsCompleted());

        Base::Complete(&Base::GetNotifier());
    }

protected:
    virtual void Execute() override final
    {
    }

    TaskBase* m_task;
};

template <class ReturnType>
class Task;

struct TaskRef
{
    TaskID id = {};
    SchedulerBase* assignedScheduler = nullptr;

    TaskRef() = default;

    TaskRef(TaskID id, SchedulerBase* assignedScheduler)
        : id(id),
          assignedScheduler(assignedScheduler)
    {
    }

    template <class ReturnType>
    TaskRef(const Task<ReturnType>& task)
        : id(task.GetTaskID()),
          assignedScheduler(task.GetAssignedScheduler())
    {
    }

    TaskRef(const TaskRef& other) = delete;
    TaskRef& operator=(const TaskRef& other) = delete;

    TaskRef(TaskRef&& other) noexcept
        : id(other.id),
          assignedScheduler(other.assignedScheduler)
    {
        other.id = {};
        other.assignedScheduler = nullptr;
    }

    TaskRef& operator=(TaskRef&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        id = other.id;
        assignedScheduler = other.assignedScheduler;

        other.id = {};
        other.assignedScheduler = nullptr;

        return *this;
    }

    ~TaskRef() = default;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return id.IsValid() && assignedScheduler != nullptr;
    }
};

class CORE_API TaskBase
{
public:
    TaskBase(TaskID id, SchedulerBase* assignedScheduler)
        : m_id(id),
          m_assignedScheduler(assignedScheduler)
    {
    }

    TaskBase(const TaskBase& other) = delete;
    TaskBase& operator=(const TaskBase& other) = delete;

    TaskBase(TaskBase&& other) noexcept
        : m_id(other.m_id),
          m_assignedScheduler(other.m_assignedScheduler)
    {
        other.m_id = {};
        other.m_assignedScheduler = nullptr;
    }

    TaskBase& operator=(TaskBase&& other) noexcept
    {
        m_id = other.m_id;
        m_assignedScheduler = other.m_assignedScheduler;

        other.m_id = {};
        other.m_assignedScheduler = nullptr;

        return *this;
    }

    virtual ~TaskBase() = default;

    HYP_FORCE_INLINE TaskID GetTaskID() const
    {
        return m_id;
    }

    HYP_FORCE_INLINE SchedulerBase* GetAssignedScheduler() const
    {
        return m_assignedScheduler;
    }

    virtual TaskExecutorBase* GetTaskExecutor() const = 0;

    virtual bool IsValid() const
    {
        const TaskExecutorBase* executor = GetTaskExecutor();

        return executor != nullptr; // && executor->GetTaskID().IsValid();
    }

    virtual bool IsCompleted() const final
    {
        return GetTaskExecutor()->IsCompleted();
    }

    /*! \brief Register \p callback to be invoked once the task has completed.
     *  If the task has already completed, \p callback is invoked immediately on the calling thread,
     *  so there is no window in which a callback can be dropped.
     *  May be called from any thread.
     *  \note The callback runs on whichever thread completes the task, which may be a task thread. */
    void OnComplete(functional::Proc<void()>&& callback);

    /*! \brief Remove the task from the scheduler.
     *  \returns True if the task was successfully cancelled, false otherwise. */
    bool Cancel();

protected:
    virtual void Await_Internal() const;

    virtual void Reset()
    {
        m_id = TaskID::Invalid();
        m_assignedScheduler = nullptr;
    }

    TaskID m_id;
    SchedulerBase* m_assignedScheduler;
};

/// \todo : Refactor so we can use a custom deleter for the task executor.
// use pre-allocated memory for manually fulfilled instances, so that Fulfill() and Await()
// on a task from the same thread doesn't cost much more than a typical function call.

template <class ReturnType>
class Task final : public TaskBase
{
public:
    using Base = TaskBase;
    using TaskExecutorType = TaskExecutorInstance<ReturnType>;

    // Default constructor, sets task as invalid
    Task()
        : TaskBase({}, nullptr),
          m_executor(nullptr),
          m_ownsExecutor(false),
          m_allowDeferredDeletion(false)
    {
    }

    Task(TaskID id, SchedulerBase* assignedScheduler, TaskExecutorType* executor, bool ownsExecutor)
        : TaskBase(id, assignedScheduler),
          m_executor(executor),
          m_ownsExecutor(ownsExecutor),
          m_allowDeferredDeletion(false)
    {
    }

    Task(const Task& other) = delete;
    Task& operator=(const Task& other) = delete;

    Task(Task&& other) noexcept
        : TaskBase(static_cast<TaskBase&&>(other)),
          m_executor(other.m_executor),
          m_ownsExecutor(other.m_ownsExecutor),
          m_allowDeferredDeletion(other.m_allowDeferredDeletion)
    {
        other.m_executor = nullptr;
        other.m_ownsExecutor = false;
        other.m_allowDeferredDeletion = false;
    }

    Task& operator=(Task&& other) noexcept
    {
        Reset();

        TaskBase::operator=(static_cast<TaskBase&&>(other));

        m_executor = other.m_executor;
        m_ownsExecutor = other.m_ownsExecutor;
        m_allowDeferredDeletion = other.m_allowDeferredDeletion;

        other.m_executor = nullptr;
        other.m_ownsExecutor = false;
        other.m_allowDeferredDeletion = false;

        return *this;
    }

    virtual ~Task() override
    {
        Reset();

        // otherwise, the executor will be freed when the task is completed
    }

    virtual TaskExecutorBase* GetTaskExecutor() const override
    {
        return m_executor;
    }

    /*! \brief Initialize the task without scheduling it.
     *  The task must be resolved with the \ref Fulfill method. */
    TaskPromise<ReturnType>* Promise()
    {
        Reset();

        m_id = TaskID { ~0u };

        m_executor = new TaskPromise<ReturnType>(this);
        m_ownsExecutor = true;
        m_allowDeferredDeletion = true;

        return static_cast<TaskPromise<ReturnType>*>(m_executor);
    }

    template <class... ArgTypes>
    void Fulfill(ArgTypes&&... args)
    {
        HYP_CORE_ASSERT(m_assignedScheduler == nullptr, "Cannot call Fulfill() on a task that has already been initialized");

        TaskPromise<ReturnType>* executor = Promise();

        executor->Fulfill(ReturnType(std::forward<ArgTypes>(args)...));
    }

    /*! \brief Wait for the task to complete.
     *  \note This function will block the current thread until the task is completed.
     *  \returns The result of the task. */
    HYP_FORCE_INLINE ReturnType& Await() &
    {
        Await_Internal();

        return m_executor->Result();
    }

    /*! \brief Wait for the task to complete.
     *  \note This function will block the current thread until the task is completed.
     *  \returns The result of the task. */
    HYP_FORCE_INLINE const ReturnType& Await() const&
    {
        Await_Internal();

        return m_executor->Result();
    }

    /*! \brief Wait for the task to complete.
     *  \note This function will block the current thread until the task is completed.
     *  \returns The result of the task. */
    HYP_FORCE_INLINE ReturnType Await() &&
    {
        Await_Internal();

        return std::move(m_executor->Result());
    }

    /*! \brief Register \p callback to be invoked once the task has completed. It may take the task's
     *  result as its only argument, or no arguments at all.
     *  If the task has already completed, \p callback is invoked immediately on the calling thread,
     *  so there is no window in which a callback can be dropped.
     *  May be called from any thread.
     *  \note The callback runs on whichever thread completes the task, which may be a task thread. */
    template <class Callback>
    void OnComplete(Callback&& callback)
    {
        if constexpr (std::is_invocable_v<Callback, ReturnType&>)
        {
            HYP_CORE_ASSERT(IsValid(), "Cannot add a completion callback to an invalid Task");

            TaskBase::OnComplete([executor = m_executor, callback = std::forward<Callback>(callback)]() mutable
                {
                    callback(executor->Result());
                });
        }
        else
        {
            static_assert(std::is_invocable_v<Callback>, "Callback must be callable with either no arguments or the task's result");

            TaskBase::OnComplete(std::forward<Callback>(callback));
        }
    }

protected:
    virtual void Await_Internal() const override
    {
        /// \todo : Move semaphore to this - executor may be deleted for FIRE_AND_FORGET tasks as we don't own it.

        m_executor->GetNotifier().Await();

#if HYP_DEBUG_MODE
        // Sanity Check
        HYP_CORE_ASSERT(IsCompleted());
#endif
    }

    virtual void Reset() override
    {
        if (m_executor != nullptr && m_ownsExecutor)
        {
            if (m_allowDeferredDeletion)
            {
                // The task may still be fulfilled from another thread; whoever gets there last
                // deletes the executor.
                if (m_executor->RelinquishOwnership())
                {
                    delete m_executor;
                }
            }
            else if (!IsCompleted())
            {
                HYP_FAIL("Task was destroyed before it was completed. Waiting on task to complete. Create a fire-and-forget task to prevent this.");
            }
            else
            {
                delete m_executor;
            }
        }

        m_executor = nullptr;
        m_ownsExecutor = false;
        m_allowDeferredDeletion = false;

        TaskBase::Reset();
    }

private:
    TaskExecutorType* m_executor;
    bool m_ownsExecutor : 1;
    bool m_allowDeferredDeletion : 1;
};

template <>
class Task<void> final : public TaskBase
{
public:
    using Base = TaskBase;
    using TaskExecutorType = TaskExecutorInstance<void>;

    // Default constructor, sets task as invalid
    Task()
        : TaskBase({}, nullptr),
          m_executor(nullptr),
          m_ownsExecutor(false),
          m_allowDeferredDeletion(false)
    {
    }

    Task(TaskID id, SchedulerBase* assignedScheduler, TaskExecutorType* executor, bool ownsExecutor)
        : TaskBase(id, assignedScheduler),
          m_executor(executor),
          m_ownsExecutor(ownsExecutor),
          m_allowDeferredDeletion(false)
    {
    }

    Task(const Task& other) = delete;
    Task& operator=(const Task& other) = delete;

    Task(Task&& other) noexcept
        : TaskBase(static_cast<TaskBase&&>(other)),
          m_executor(other.m_executor),
          m_ownsExecutor(other.m_ownsExecutor),
          m_allowDeferredDeletion(other.m_allowDeferredDeletion)
    {
        other.m_executor = nullptr;
        other.m_ownsExecutor = false;
        other.m_allowDeferredDeletion = false;
    }

    Task& operator=(Task&& other) noexcept
    {
        TaskBase::operator=(static_cast<TaskBase&&>(other));

        m_executor = other.m_executor;
        m_ownsExecutor = other.m_ownsExecutor;
        m_allowDeferredDeletion = other.m_allowDeferredDeletion;

        other.m_executor = nullptr;
        other.m_ownsExecutor = false;
        other.m_allowDeferredDeletion = false;

        return *this;
    }

    virtual ~Task() override
    {
        Reset();
        // otherwise, the executor will be freed when the task is completed
    }

    virtual TaskExecutorBase* GetTaskExecutor() const override
    {
        return m_executor;
    }

    /*! \brief Initialize the task without scheduling it.
     *  The task must be resolved with the \ref Fulfill method. */
    TaskPromise<void>* Promise()
    {
        Reset();

        m_id = TaskID { ~0u };

        m_executor = new TaskPromise<void>(this);
        m_ownsExecutor = true;
        m_allowDeferredDeletion = true;

        return static_cast<TaskPromise<void>*>(m_executor);
    }

    void Fulfill()
    {
        HYP_CORE_ASSERT(m_assignedScheduler == nullptr, "Cannot call Fulfill() on a task that has already been initialized");

        TaskPromise<void>* executor = Promise();

        executor->Fulfill();
    }

    HYP_FORCE_INLINE void Await()
    {
        Await_Internal();
    }

protected:
    virtual void Await_Internal() const override
    {
        m_executor->GetNotifier().Await();

#if HYP_DEBUG_MODE
        // Sanity Check
        HYP_CORE_ASSERT(IsCompleted());
#endif
    }

    virtual void Reset() override
    {
        if (m_executor != nullptr && m_ownsExecutor)
        {
            if (m_allowDeferredDeletion)
            {
                // The task may still be fulfilled from another thread; whoever gets there last
                // deletes the executor.
                if (m_executor->RelinquishOwnership())
                {
                    delete m_executor;
                }
            }
            else if (!IsCompleted())
            {
                HYP_FAIL("Task was destroyed before it was completed. Waiting on task to complete. Create a fire-and-forget task to prevent this.");
            }
            else
            {
                delete m_executor;
            }
        }

        m_executor = nullptr;
        m_ownsExecutor = false;
        m_allowDeferredDeletion = false;

        TaskBase::Reset();
    }

private:
    TaskExecutorType* m_executor;
    bool m_ownsExecutor : 1;
    bool m_allowDeferredDeletion : 1;
};

#pragma region AwaitAll

template <class TaskType>
struct TaskAwaitAll_Impl;

template <class ReturnType>
struct TaskAwaitAll_Impl<Task<ReturnType>>
{
    auto operator()(Span<Task<ReturnType>> tasks) const -> std::conditional_t<std::is_void_v<ReturnType>, void, Array<ReturnType>>
    {
        Array<ReturnType> results;
        results.ResizeUninitialized(tasks.Size());

        for (size_t i = 0; i < tasks.Size(); ++i)
        {
            Task<ReturnType>& task = tasks[i];

            if (!task.IsValid())
            {
                Memory::Construct<ReturnType>(&results[i]);
                continue;
            }

            Memory::Construct<ReturnType>(&results[i], std::move(task.Await()));
        }

        return results;

#if 0
        for (size_t i = 0; i < tasks.Size(); ++i) {
            Task<ReturnType> &task = tasks[i];

            HYP_CORE_ASSERT(task.IsValid());
        }

        Bitset completionStates;
        Bitset boundStates;

        //debug
        Array<int> calledStates;
        calledStates.Resize(tasks.Size());

        Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE> semaphore(tasks.Size());

        while ((completionStates | boundStates).Count() != tasks.Size()) {
            for (size_t i = 0; i < tasks.Size(); ++i) {
                if (completionStates.Test(i) || boundStates.Test(i)) {
                    continue;
                }

                Task<ReturnType> &task = tasks[i];

                if (task.IsCompleted()) {
                    completionStates.Set(i, true);

                    semaphore.Release(1);

                    continue;
                }

                /// \todo : What if task finished right before callback was set?

                task.GetTaskExecutor()->GetCallbackChain().Add([&semaphore, &completionStates, &calledStates, &tasks, taskIndex = i]()
                {
                    HYP_CORE_ASSERT(calledStates[taskIndex] == 0);

                    calledStates[taskIndex] = 1;
                    semaphore.Release(1);
                });

                boundStates.Set(i, true);
            }
        }

        semaphore.Acquire();

        if constexpr (std::is_void_v<ReturnType>) {
            for (size_t i = 0; i < tasks.Size(); ++i) {
                Task<ReturnType> &task = tasks[i];
                HYP_CORE_ASSERT(task.IsCompleted());

                task.Await();
            }
        } else {
            Array<ReturnType> results;
            results.ResizeUninitialized(tasks.Size());

            for (size_t i = 0; i < tasks.Size(); ++i) {
                Task<ReturnType> &task = tasks[i];
                HYP_CORE_ASSERT(task.IsCompleted());

                Memory::Construct<ReturnType>(&results[i], std::move(task.Await()));
            }

            return results;
        }
#endif
    }
};

template <>
struct TaskAwaitAll_Impl<Task<void>>
{
    void operator()(Span<Task<void>> tasks) const
    {
        for (size_t i = 0; i < tasks.Size(); ++i)
        {
            Task<void>& task = tasks[i];

            if (!task.IsValid())
            {
                continue;
            }

            task.Await();
        }

#if 0
        for (size_t i = 0; i < tasks.Size(); ++i) {
            Task<void> &task = tasks[i];

            HYP_CORE_ASSERT(task.IsValid());
        }

        Bitset completionStates;
        Bitset boundStates;

        //debug
        Array<int> calledStates;
        calledStates.Resize(tasks.Size());

        Semaphore<int, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE> semaphore(int(tasks.Size()));

        while ((completionStates | boundStates).Count() != tasks.Size()) {
            for (size_t i = 0; i < tasks.Size(); ++i) {
                if (completionStates.Test(i) || boundStates.Test(i)) {
                    continue;
                }

                Task<void> &task = tasks[i];

                if (task.IsCompleted()) {
                    completionStates.Set(i, true);

                    semaphore.Release(1);

                    continue;
                }

                /// \todo : What if task finished right before callback was set?

                task.GetTaskExecutor()->GetCallbackChain().Add([&semaphore, &completionStates, &calledStates, &tasks, taskIndex = i]()
                {
                    HYP_CORE_ASSERT(calledStates[taskIndex] == 0);

                    calledStates[taskIndex] = 1;
                    semaphore.Release(1);
                });

                boundStates.Set(i, true);
            }
        }

        semaphore.Acquire();

        for (size_t i = 0; i < tasks.Size(); ++i) {
            Task<void> &task = tasks[i];
            HYP_CORE_ASSERT(task.IsCompleted());

            task.Await();
        }
#endif
    }
};

template <class TaskType>
decltype(auto) AwaitAll(Span<TaskType> tasks)
{
    return TaskAwaitAll_Impl<TaskType> {}(tasks);
}

#pragma endregion AwaitAll

} // namespace threading

using threading::AwaitAll;
using threading::ITaskExecutor;
using threading::Task;
using threading::TaskBase;
using threading::TaskCallbackChain;
using threading::TaskCompleteNotifier;
using threading::TaskExecutorBase;
using threading::TaskExecutorInstance;
using threading::TaskID;
using threading::TaskPromise;

} // namespace Hyperion
