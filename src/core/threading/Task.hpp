/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_TASK_HPP
#define HYPERION_TASK_HPP

#include <core/containers/Bitset.hpp>

#include <core/utilities/Optional.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Span.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Thread.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/functional/Proc.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/Util.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Tasks);

enum class TaskEnqueueFlags : uint32
{
    NONE = 0x0,
    FIRE_AND_FORGET = 0x1
};

HYP_MAKE_ENUM_FLAGS(TaskEnqueueFlags)

namespace threading {

class TaskThread;
struct TaskBatch;
class SchedulerBase;
class TaskBase;

class TaskCompleteNotifier final : public Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>
{
public:
    /*! \brief Sets the number of tasks that need to be completed before the notifier is signalled.
     *  This is typically called when the task batch is created, and the number of tasks is known.
     */
    void SetTargetValue(uint32 num_tasks)
    {
        Semaphore::SetValue(int32(num_tasks));
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

using OnTaskCompletedCallback = Proc<void()>;

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

class HYP_API TaskCallbackChain
{
public:
    TaskCallbackChain() = default;

    TaskCallbackChain(const TaskCallbackChain& other) = delete;
    TaskCallbackChain& operator=(const TaskCallbackChain& other) = delete;

    TaskCallbackChain(TaskCallbackChain&& other) noexcept;
    TaskCallbackChain& operator=(TaskCallbackChain&& other) noexcept;

    ~TaskCallbackChain() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_num_callbacks.Get(MemoryOrder::ACQUIRE);
    }

    void Add(Proc<void()>&& callback);

    void operator()();

private:
    Array<Proc<void()>> m_callbacks;
    AtomicVar<uint32> m_num_callbacks;
    Mutex m_mutex;
};

class ITaskExecutor
{
public:
    virtual ~ITaskExecutor() = default;

    virtual TaskID GetTaskID() const = 0;

    virtual bool IsCompleted() const = 0;

    /*! \brief Not called if task is part of a TaskBatch. */
    virtual TaskCallbackChain& GetCallbackChain() = 0;
};

class HYP_API TaskExecutorBase : public ITaskExecutor
{
public:
    TaskExecutorBase()
        : m_id(TaskID::Invalid()),
          m_initiator_thread_id(ThreadID::Invalid()),
          m_assigned_scheduler(nullptr)
    {
        // set notifier to initial value of 1 (one task)
        m_notifier.Produce(1);
    }

    TaskExecutorBase(const TaskExecutorBase& other) = delete;
    TaskExecutorBase& operator=(const TaskExecutorBase& other) = delete;

    TaskExecutorBase(TaskExecutorBase&& other) noexcept
        : m_id(other.m_id),
          m_initiator_thread_id(other.m_initiator_thread_id),
          m_assigned_scheduler(other.m_assigned_scheduler),
          m_notifier(std::move(other.m_notifier)),
          m_callback_chain(std::move(other.m_callback_chain))
    {
        other.m_id = {};
        other.m_initiator_thread_id = {};
        other.m_assigned_scheduler = nullptr;
    }

    TaskExecutorBase& operator=(TaskExecutorBase&& other) noexcept = delete;

    virtual ~TaskExecutorBase() override = default;

    virtual TaskID GetTaskID() const override final
    {
        return m_id;
    }

    /*! \internal This function is used by the Scheduler to set the task ID. */
    HYP_FORCE_INLINE void SetTaskID(TaskID id)
    {
        m_id = id;
    }

    HYP_FORCE_INLINE const ThreadID& GetInitiatorThreadID() const
    {
        return m_initiator_thread_id;
    }

    /*! \internal This function is used by the Scheduler to set the initiator thread ID. */
    HYP_FORCE_INLINE void SetInitiatorThreadID(const ThreadID& initiator_thread_id)
    {
        m_initiator_thread_id = initiator_thread_id;
    }

    HYP_FORCE_INLINE SchedulerBase* GetAssignedScheduler() const
    {
        return m_assigned_scheduler;
    }

    /*! \internal This function is used by the Scheduler to set the assigned scheduler. */
    HYP_FORCE_INLINE void SetAssignedScheduler(SchedulerBase* assigned_scheduler)
    {
        m_assigned_scheduler = assigned_scheduler;
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
        return m_callback_chain;
    }

protected:
    TaskID m_id;
    ThreadID m_initiator_thread_id;
    SchedulerBase* m_assigned_scheduler;
    TaskCompleteNotifier m_notifier;

    TaskCallbackChain m_callback_chain;
};

template <class ReturnType>
class TaskExecutorInstance : public TaskExecutorBase
{
public:
    using Function = Proc<ReturnType()>;
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
          m_result_value(std::move(other.m_result_value))
    {
    }

    TaskExecutorInstance& operator=(TaskExecutorInstance&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        Base::operator=(static_cast<Base&&>(other));

        m_fn = std::move(other.m_fn);
        m_result_value = std::move(other.m_result_value);

        return *this;
    }

    virtual ~TaskExecutorInstance() override = default;

    HYP_FORCE_INLINE ReturnType& Result() &
    {
        return m_result_value.Get();
    }

    HYP_FORCE_INLINE const ReturnType& Result() const&
    {
        return m_result_value.Get();
    }

    HYP_FORCE_INLINE ReturnType Result() &&
    {
        return std::move(m_result_value.Get());
    }

    HYP_FORCE_INLINE ReturnType Result() const&&
    {
        return m_result_value.Get();
    }

    virtual void Execute() override
    {
        AssertThrow(m_fn.IsValid());

        m_result_value.Emplace(m_fn());
    }

protected:
    Function m_fn;
    Optional<ReturnType> m_result_value;
};

/*! \brief Specialization for void return type. */
template <>
class TaskExecutorInstance<void> : public TaskExecutorBase
{
    using Function = Proc<void()>;

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

    // TaskExecutorInstance &operator=(TaskExecutorInstance &&other) noexcept
    // {
    //     Base::operator=(static_cast<Base &&>(other));

    //     m_fn = std::move(other.m_fn);

    //     return *this;
    // }

    virtual ~TaskExecutorInstance() override = default;

    virtual void Execute() override
    {
        AssertThrow(m_fn.IsValid());

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

    void Fulfill(ReturnType&& value)
    {
        AssertThrow(!Base::IsCompleted());

        Base::m_result_value.Set(std::move(value));

        TaskCallbackChain& callback_chain = Base::GetCallbackChain();

        Base::GetNotifier().Release(1);

        if (callback_chain)
        {
            callback_chain();
        }
    }

    void Fulfill(const ReturnType& value)
    {
        AssertThrow(!Base::IsCompleted());

        Base::m_result_value.Set(value);

        TaskCallbackChain& callback_chain = Base::GetCallbackChain();

        Base::GetNotifier().Release(1);

        if (callback_chain)
        {
            callback_chain();
        }
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

    void Fulfill()
    {
        AssertThrow(!Base::IsCompleted());

        TaskCallbackChain& callback_chain = Base::GetCallbackChain();

        Base::GetNotifier().Release(1);

        if (callback_chain)
        {
            callback_chain();
        }
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
    SchedulerBase* assigned_scheduler = nullptr;

    TaskRef() = default;

    TaskRef(TaskID id, SchedulerBase* assigned_scheduler)
        : id(id),
          assigned_scheduler(assigned_scheduler)
    {
    }

    template <class ReturnType>
    TaskRef(const Task<ReturnType>& task)
        : id(task.GetTaskID()),
          assigned_scheduler(task.GetAssignedScheduler())
    {
    }

    TaskRef(const TaskRef& other) = delete;
    TaskRef& operator=(const TaskRef& other) = delete;

    TaskRef(TaskRef&& other) noexcept
        : id(other.id),
          assigned_scheduler(other.assigned_scheduler)
    {
        other.id = {};
        other.assigned_scheduler = nullptr;
    }

    TaskRef& operator=(TaskRef&& other) noexcept
    {
        id = other.id;
        assigned_scheduler = other.assigned_scheduler;

        other.id = {};
        other.assigned_scheduler = nullptr;

        return *this;
    }

    ~TaskRef() = default;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return id.IsValid() && assigned_scheduler != nullptr;
    }
};

class TaskBase
{
public:
    TaskBase(TaskID id, SchedulerBase* assigned_scheduler)
        : m_id(id),
          m_assigned_scheduler(assigned_scheduler)
    {
    }

    TaskBase(const TaskBase& other) = delete;
    TaskBase& operator=(const TaskBase& other) = delete;

    TaskBase(TaskBase&& other) noexcept
        : m_id(other.m_id),
          m_assigned_scheduler(other.m_assigned_scheduler)
    {
        other.m_id = {};
        other.m_assigned_scheduler = nullptr;
    }

    TaskBase& operator=(TaskBase&& other) noexcept
    {
        m_id = other.m_id;
        m_assigned_scheduler = other.m_assigned_scheduler;

        other.m_id = {};
        other.m_assigned_scheduler = nullptr;

        return *this;
    }

    virtual ~TaskBase() = default;

    HYP_FORCE_INLINE TaskID GetTaskID() const
    {
        return m_id;
    }

    HYP_FORCE_INLINE SchedulerBase* GetAssignedScheduler() const
    {
        return m_assigned_scheduler;
    }

    virtual ITaskExecutor* GetTaskExecutor() const = 0;

    virtual bool IsValid() const
    {
        const ITaskExecutor* executor = GetTaskExecutor();

        return executor != nullptr; // && executor->GetTaskID().IsValid();
    }

    virtual bool IsCompleted() const final
    {
        return GetTaskExecutor()->IsCompleted();
    }

    /*! \brief Remove the task from the scheduler.
     *  \returns True if the task was successfully cancelled, false otherwise. */
    bool Cancel();

protected:
    virtual void Await_Internal() const;

    virtual void Reset()
    {
        m_id = TaskID::Invalid();
        m_assigned_scheduler = nullptr;
    }

    TaskID m_id;
    SchedulerBase* m_assigned_scheduler;
};

// @TODO: Refactor so we can use a custom deleter for the task executor.
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
          m_owns_executor(false)
    {
    }

    Task(TaskID id, SchedulerBase* assigned_scheduler, TaskExecutorType* executor, bool owns_executor)
        : TaskBase(id, assigned_scheduler),
          m_executor(executor),
          m_owns_executor(owns_executor)
    {
    }

    Task(const Task& other) = delete;
    Task& operator=(const Task& other) = delete;

    Task(Task&& other) noexcept
        : TaskBase(static_cast<TaskBase&&>(other)),
          m_executor(other.m_executor),
          m_owns_executor(other.m_owns_executor)
    {
        other.m_executor = nullptr;
        other.m_owns_executor = false;
    }

    Task& operator=(Task&& other) noexcept
    {
        Reset();

        TaskBase::operator=(static_cast<TaskBase&&>(other));

        m_executor = other.m_executor;
        m_owns_executor = other.m_owns_executor;

        other.m_executor = nullptr;
        other.m_owns_executor = false;

        return *this;
    }

    virtual ~Task() override
    {
        Reset();

        // otherwise, the executor will be freed when the task is completed
    }

    virtual ITaskExecutor* GetTaskExecutor() const override
    {
        return m_executor;
    }

    /*! \brief Initialize the task without scheduling it.
     *  The task must be resolved with the \ref{Fulfill} method. */
    TaskPromise<ReturnType>* Promise()
    {
        Reset();

        m_id = TaskID { ~0u };

        m_executor = new TaskPromise<ReturnType>(this);
        m_owns_executor = true;

        return static_cast<TaskPromise<ReturnType>*>(m_executor);
    }

    template <class... ArgTypes>
    void Fulfill(ArgTypes&&... args)
    {
        AssertThrowMsg(m_assigned_scheduler == nullptr, "Cannot call Fulfill() on a task that has already been initialized");

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

protected:
    virtual void Await_Internal() const override
    {
        // @TODO: Move semaphore to this - executor may be deleted for FIRE_AND_FORGET tasks as we don't own it.

        m_executor->GetNotifier().Await();

#ifdef HYP_DEBUG_MODE
        // Sanity Check
        AssertThrow(IsCompleted());
#endif
    }

    virtual void Reset() override
    {
        if (m_owns_executor)
        {
            // Wait for the task to complete when not in debug mode
            if (IsValid() && !IsCompleted())
            {
                HYP_FAIL("Task was destroyed before it was completed. Waiting on task to complete. Create a fire-and-forget task to prevent this.");
            }

            delete m_executor;
        }

        m_executor = nullptr;
        m_owns_executor = false;

        TaskBase::Reset();
    }

private:
    TaskExecutorType* m_executor;
    bool m_owns_executor;
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
          m_owns_executor(false)
    {
    }

    Task(TaskID id, SchedulerBase* assigned_scheduler, TaskExecutorType* executor, bool owns_executor)
        : TaskBase(id, assigned_scheduler),
          m_executor(executor),
          m_owns_executor(owns_executor)
    {
    }

    Task(const Task& other) = delete;
    Task& operator=(const Task& other) = delete;

    Task(Task&& other) noexcept
        : TaskBase(static_cast<TaskBase&&>(other)),
          m_executor(other.m_executor),
          m_owns_executor(other.m_owns_executor)
    {
        other.m_executor = nullptr;
        other.m_owns_executor = false;
    }

    Task& operator=(Task&& other) noexcept
    {
        TaskBase::operator=(static_cast<TaskBase&&>(other));

        m_executor = other.m_executor;
        m_owns_executor = other.m_owns_executor;

        other.m_executor = nullptr;
        other.m_owns_executor = false;

        return *this;
    }

    virtual ~Task() override
    {
        if (m_owns_executor)
        {
            // Wait for the task to complete when not in debug mode
            if (IsValid() && !IsCompleted())
            {
#ifdef HYP_DEBUG_MODE
                HYP_FAIL("Task was destroyed before it was completed. Waiting on task to complete. Create a fire-and-forget task to prevent this.");
#else
                Base::Await_Internal();
#endif
            }

            delete m_executor;
        }

        // otherwise, the executor will be freed when the task is completed
    }

    virtual ITaskExecutor* GetTaskExecutor() const override
    {
        return m_executor;
    }

    /*! \brief Initialize the task without scheduling it.
     *  The task must be resolved with the \ref{Fulfill} method. */
    TaskPromise<void>* Promise()
    {
        Reset();

        m_id = TaskID { ~0u };

        m_executor = new TaskPromise<void>(this);
        m_owns_executor = true;

        return static_cast<TaskPromise<void>*>(m_executor);
    }

    void Fulfill()
    {
        AssertThrowMsg(m_assigned_scheduler == nullptr, "Cannot call Fulfill() on a task that has already been initialized");

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

#ifdef HYP_DEBUG_MODE
        // Sanity Check
        AssertThrow(IsCompleted());
#endif
    }

    virtual void Reset() override
    {
        if (m_owns_executor)
        {
            delete m_executor;
        }
        else
        {
            // Wait for the task to complete when not in debug mode
            if (IsValid() && !IsCompleted())
            {
#ifdef HYP_DEBUG_MODE
                HYP_FAIL("Task was destroyed before it was completed. Waiting on task to complete. Create a fire-and-forget task to prevent this.");
#else
                Base::Await_Internal();
#endif
            }
        }

        m_executor = nullptr;
        m_owns_executor = false;

        TaskBase::Reset();
    }

private:
    TaskExecutorType* m_executor;
    bool m_owns_executor;
};

#if 0
template <class ReturnType>
class TaskFuture final
{
public:
    friend class TaskPromise<ReturnType>;
    friend class Task<ReturnType>;

    ~TaskFuture()
    {
        if (m_task != nullptr)
        {
            delete m_task;
        }
    }

    TaskFuture(const TaskFuture& other) = delete;
    TaskFuture& operator=(const TaskFuture& other) = delete;

    TaskFuture(TaskFuture&& other) noexcept
        : m_task(other.m_task)
    {
        other.m_task = nullptr;
    }

    TaskFuture& operator=(TaskFuture&& other) noexcept
    {
        if (this == &other || m_task == other.m_task)
        {
            return *this;
        }

        AssertDebug(m_task != nullptr);

        delete m_task;

        m_task = other.m_task;
        other.m_task = nullptr;

        return *this;
    }

    ReturnType& Result() const&
    {
        AssertThrow(m_task != nullptr);

        return m_task->Await();
    }

    ReturnType Result() &&
    {
        AssertThrow(m_task != nullptr);

        ResultType result = std::move(*m_task).Await();

        delete m_task;
        m_task = nullptr;

        return result;
    }

protected:
    TaskFuture(Task<ReturnType>* task)
        : m_task(task)
    {
    }

    Task<ReturnType>* m_task;
};

template <>
class TaskFuture<void> final
{
public:
    friend class TaskPromise<void>;
    friend class Task<void>;

    ~TaskFuture()
    {
        if (m_task != nullptr)
        {
            delete m_task;
        }
    }

    TaskFuture(const TaskFuture& other) = delete;
    TaskFuture& operator=(const TaskFuture& other) = delete;

    TaskFuture(TaskFuture&& other) noexcept
        : m_task(other.m_task)
    {
        other.m_task = nullptr;
    }

    TaskFuture& operator=(TaskFuture&& other) noexcept
    {
        if (this == &other || m_task == other.m_task)
        {
            return *this;
        }

        AssertDebug(m_task != nullptr);

        delete m_task;

        m_task = other.m_task;
        other.m_task = nullptr;

        return *this;
    }

    void Result() const
    {
        AssertThrow(m_task != nullptr);

        m_task->Await();
    }

protected:
    TaskFuture(Task<void>* task)
        : m_task(task)
    {
    }

    Task<void>* m_task;
};
#endif

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

        for (SizeType i = 0; i < tasks.Size(); ++i)
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
        for (SizeType i = 0; i < tasks.Size(); ++i) {
            Task<ReturnType> &task = tasks[i];

            AssertThrow(task.IsValid());
        }

        Bitset completion_states;
        Bitset bound_states;

        //debug
        Array<int> called_states;
        called_states.Resize(tasks.Size());

        Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE> semaphore(tasks.Size());

        while ((completion_states | bound_states).Count() != tasks.Size()) {
            for (SizeType i = 0; i < tasks.Size(); ++i) {
                if (completion_states.Test(i) || bound_states.Test(i)) {
                    continue;
                }

                Task<ReturnType> &task = tasks[i];

                if (task.IsCompleted()) {
                    completion_states.Set(i, true);

                    semaphore.Release(1);

                    continue;
                }

                // @TODO : What if task finished right before callback was set?

                task.GetTaskExecutor()->GetCallbackChain().Add([&semaphore, &completion_states, &called_states, &tasks, task_index = i]()
                {
                    AssertThrow(called_states[task_index] == 0);

                    called_states[task_index] = 1;
                    DebugLog(LogType::Debug, "Call OnCompleted for task index %u (executor ptr: %p)\n", task_index, tasks[task_index].GetTaskExecutor());
                    semaphore.Release(1);
                });

                bound_states.Set(i, true);
            }
        }

        semaphore.Acquire();

        if constexpr (std::is_void_v<ReturnType>) {
            for (SizeType i = 0; i < tasks.Size(); ++i) {
                Task<ReturnType> &task = tasks[i];
                AssertThrow(task.IsCompleted());
                
                task.Await();
            }
        } else {
            Array<ReturnType> results;
            results.ResizeUninitialized(tasks.Size());

            for (SizeType i = 0; i < tasks.Size(); ++i) {
                Task<ReturnType> &task = tasks[i];
                AssertThrow(task.IsCompleted());
                
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
        for (SizeType i = 0; i < tasks.Size(); ++i)
        {
            Task<void>& task = tasks[i];

            if (!task.IsValid())
            {
                continue;
            }

            task.Await();
        }

#if 0
        for (SizeType i = 0; i < tasks.Size(); ++i) {
            Task<void> &task = tasks[i];

            AssertThrow(task.IsValid());
        }

        Bitset completion_states;
        Bitset bound_states;

        //debug
        Array<int> called_states;
        called_states.Resize(tasks.Size());

        Semaphore<int, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE> semaphore(int(tasks.Size()));

        while ((completion_states | bound_states).Count() != tasks.Size()) {
            for (SizeType i = 0; i < tasks.Size(); ++i) {
                if (completion_states.Test(i) || bound_states.Test(i)) {
                    continue;
                }

                Task<void> &task = tasks[i];

                if (task.IsCompleted()) {
                    completion_states.Set(i, true);

                    semaphore.Release(1);

                    continue;
                }

                // @TODO : What if task finished right before callback was set?

                task.GetTaskExecutor()->GetCallbackChain().Add([&semaphore, &completion_states, &called_states, &tasks, task_index = i]()
                {
                    AssertThrow(called_states[task_index] == 0);

                    called_states[task_index] = 1;
                    DebugLog(LogType::Debug, "Call OnCompleted for task index %u (executor ptr: %p)\n", task_index, tasks[task_index].GetTaskExecutor());
                    semaphore.Release(1);
                });

                bound_states.Set(i, true);
            }
        }

        semaphore.Acquire();

        for (SizeType i = 0; i < tasks.Size(); ++i) {
            Task<void> &task = tasks[i];
            AssertThrow(task.IsCompleted());
            
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

} // namespace hyperion

#endif