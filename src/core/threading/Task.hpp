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

#include <core/functional/Proc.hpp>
#include <core/functional/Delegate.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/Util.hpp>

#include <Types.hpp>

namespace hyperion {

enum class TaskEnqueueFlags : uint32
{
    NONE            = 0x0,
    FIRE_AND_FORGET = 0x1
};

HYP_MAKE_ENUM_FLAGS(TaskEnqueueFlags)

namespace threading {

class TaskThread;
class TaskBatch;
class SchedulerBase;

using TaskSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>;

using OnTaskCompletedCallback = Proc<void>;

struct TaskID
{
    static TaskID Invalid()
    {
        return TaskID { 0 };
    }

    uint32 value { 0 };
    
    TaskID &operator=(uint32 id)
    {
        value = id;

        return *this;
    }
    
    TaskID &operator=(const TaskID &other) = default;

    HYP_FORCE_INLINE bool operator==(uint32 id) const
        { return value == id; }

    HYP_FORCE_INLINE bool operator!=(uint32 id) const
        { return value != id; }

    HYP_FORCE_INLINE bool operator==(const TaskID &other) const
        { return value == other.value; }

    HYP_FORCE_INLINE bool operator!=(const TaskID &other) const
        { return value != other.value; }

    HYP_FORCE_INLINE bool operator<(const TaskID &other) const
        { return value < other.value; }

    HYP_FORCE_INLINE explicit operator bool() const
        { return value != 0; }

    HYP_FORCE_INLINE bool operator!() const
        { return value == 0; }

    HYP_FORCE_INLINE bool IsValid() const
        { return value != 0; }
};

class ITaskExecutor
{
public:
    virtual ~ITaskExecutor()    = default;

    virtual TaskID GetTaskID() const = 0;

    virtual bool IsCompleted() const = 0;

    virtual Delegate<void> &GetOnCompleteDelegate() = 0;
};

template <class... ArgTypes>
class TaskExecutor : public ITaskExecutor
{
public:
    TaskExecutor()
        : m_id(TaskID::Invalid()),
          m_initiator_thread_id(ThreadID::Invalid()),
          m_assigned_scheduler(nullptr)
    {
        // set semaphore to initial value of 1 (one task)
        m_semaphore.Produce(1);
    }

    TaskExecutor(const TaskExecutor &other)             = delete;
    TaskExecutor &operator=(const TaskExecutor &other)  = delete;

    TaskExecutor(TaskExecutor &&other) noexcept
        : m_id(other.m_id),
          m_initiator_thread_id(other.m_initiator_thread_id),
          m_assigned_scheduler(other.m_assigned_scheduler),
          m_semaphore(std::move(other.m_semaphore)),
          OnComplete(std::move(other.OnComplete))
    {
        other.m_id = {};
        other.m_initiator_thread_id = {};
        other.m_assigned_scheduler = nullptr;
    }

    TaskExecutor &operator=(TaskExecutor &&other) noexcept = delete;

    // TaskExecutor &operator=(TaskExecutor &&other) noexcept
    // {
    //     if (this == &other) {
    //         return *this;
    //     }

    //     m_id = other.m_id;
    //     m_initiator_thread_id = other.m_initiator_thread_id;
    //     m_assigned_scheduler = other.m_assigned_scheduler;
    //     m_semaphore = std::move(other.m_semaphore);

    //     other.m_id = {};
    //     other.m_initiator_thread_id = {};
    //     other.m_assigned_scheduler = nullptr;

    //     return *this;
    // }

    virtual ~TaskExecutor() override = default;

    virtual TaskID GetTaskID() const override final
        { return m_id; }

    /*! \internal This function is used by the Scheduler to set the task ID. */
    HYP_FORCE_INLINE void SetTaskID(TaskID id)
        { m_id = id; }

    HYP_FORCE_INLINE ThreadID GetInitiatorThreadID() const
        { return m_initiator_thread_id; }

    /*! \internal This function is used by the Scheduler to set the initiator thread ID. */
    HYP_FORCE_INLINE void SetInitiatorThreadID(ThreadID initiator_thread_id)
        { m_initiator_thread_id = initiator_thread_id; }

    HYP_FORCE_INLINE SchedulerBase *GetAssignedScheduler() const
        { return m_assigned_scheduler; }

    /*! \internal This function is used by the Scheduler to set the assigned scheduler. */
    HYP_FORCE_INLINE void SetAssignedScheduler(SchedulerBase *assigned_scheduler)
        { m_assigned_scheduler = assigned_scheduler; }

    HYP_FORCE_INLINE TaskSemaphore &GetSemaphore()
        { return m_semaphore; }

    HYP_FORCE_INLINE const TaskSemaphore &GetSemaphore() const
        { return m_semaphore; }

    virtual bool IsCompleted() const override final
        { return m_semaphore.IsInSignalState(); }

    virtual void Execute(ArgTypes... args) = 0;

    virtual Delegate<void> &GetOnCompleteDelegate() override final
        { return OnComplete; }

    /*! \brief Not called if task is part of a TaskBatch. */
    Delegate<void>  OnComplete;

protected:
    TaskID          m_id;
    ThreadID        m_initiator_thread_id;
    SchedulerBase   *m_assigned_scheduler;
    TaskSemaphore   m_semaphore;
};

template <class ReturnType, class... ArgTypes>
class TaskExecutorInstance : public TaskExecutor<ArgTypes...>
{
public:
    using Function = Proc<ReturnType, ArgTypes...>;
    using Base = TaskExecutor<ArgTypes...>;

    template <class Lambda>
    TaskExecutorInstance(Lambda &&fn)
        : m_fn(std::forward<Lambda>(fn))
    {
    }

    TaskExecutorInstance(const TaskExecutorInstance &other)             = delete;
    TaskExecutorInstance &operator=(const TaskExecutorInstance &other)  = delete;

    TaskExecutorInstance(TaskExecutorInstance &&other) noexcept
        : Base(static_cast<Base &&>(other)),
          m_fn(std::move(other.m_fn)),
          m_result_value(std::move(other.m_result_value))
    {
    }

    TaskExecutorInstance &operator=(TaskExecutorInstance &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        Base::operator=(static_cast<Base &&>(other));

        m_fn = std::move(other.m_fn);
        m_result_value = std::move(other.m_result_value);

        return *this;
    }
    
    virtual ~TaskExecutorInstance() override = default;

    HYP_FORCE_INLINE ReturnType &Result() &
        { return m_result_value.Get(); }

    HYP_FORCE_INLINE const ReturnType &Result() const &
        { return m_result_value.Get(); }

    HYP_FORCE_INLINE ReturnType Result() &&
        { return std::move(m_result_value.Get()); }

    HYP_FORCE_INLINE ReturnType Result() const &&
        { return m_result_value.Get(); }

    virtual void Execute(ArgTypes... args) override
    {
        AssertThrow(m_fn.IsValid());

        m_result_value.Emplace(m_fn(std::forward<ArgTypes>(args)...));
    }

protected:
    Function                m_fn;
    Optional<ReturnType>    m_result_value;
};

/*! \brief Specialization for void return type. */
template <class... ArgTypes>
class TaskExecutorInstance<void, ArgTypes...> : public TaskExecutor<ArgTypes...>
{
    using Function = Proc<void, ArgTypes...>;

public:
    using Base = TaskExecutor<ArgTypes...>;

    template <class Lambda>
    TaskExecutorInstance(Lambda &&fn)
        : m_fn(std::forward<Lambda>(fn))
    {
    }

    TaskExecutorInstance(const TaskExecutorInstance &other)             = delete;
    TaskExecutorInstance &operator=(const TaskExecutorInstance &other)  = delete;

    TaskExecutorInstance(TaskExecutorInstance &&other) noexcept
        : Base(static_cast<Base &&>(other)),
          m_fn(std::move(other.m_fn))
    {
    }

    TaskExecutorInstance &operator=(TaskExecutorInstance &&other) noexcept
    {
        Base::operator=(static_cast<Base &&>(other));

        m_fn = std::move(other.m_fn);

        return *this;
    }
    
    virtual ~TaskExecutorInstance() override = default;

    virtual void Execute(ArgTypes... args) override
    {
        AssertThrow(m_fn.IsValid());
        
        m_fn(std::forward<ArgTypes>(args)...);
    }

protected:
    Function    m_fn;
};

template <class ReturnType, class... ArgTypes>
class ManuallyFulfilledTaskExecutorInstance : public TaskExecutorInstance<ReturnType, ArgTypes...>
{
public:
    using Base = TaskExecutorInstance<ReturnType, ArgTypes...>;

    ManuallyFulfilledTaskExecutorInstance()
        : Base(static_cast<ReturnType(*)(void)>(nullptr))
    {
    }

    ManuallyFulfilledTaskExecutorInstance(const ManuallyFulfilledTaskExecutorInstance &other)               = delete;
    ManuallyFulfilledTaskExecutorInstance &operator=(const ManuallyFulfilledTaskExecutorInstance &other)    = delete;

    ManuallyFulfilledTaskExecutorInstance(ManuallyFulfilledTaskExecutorInstance &&other) noexcept
        : Base(static_cast<Base &&>(other))
    {
    }

    ManuallyFulfilledTaskExecutorInstance &operator=(ManuallyFulfilledTaskExecutorInstance &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        Base::operator=(static_cast<Base &&>(other));

        return *this;
    }
    
    virtual ~ManuallyFulfilledTaskExecutorInstance() override = default;

    void Fulfill(ReturnType &&value)
    {
        AssertThrow(!Base::IsCompleted());

        Base::m_result_value.Set(std::move(value));

        Base::GetSemaphore().Release(1);

        Base::OnComplete();
    }

    void Fulfill(const ReturnType &value)
    {
        AssertThrow(!Base::IsCompleted());
        
        Base::m_result_value.Set(value);

        Base::GetSemaphore().Release(1);
        
        Base::OnComplete();
    }

protected:
    virtual void Execute(ArgTypes... args) override final { }
};

template <class... ArgTypes>
class ManuallyFulfilledTaskExecutorInstance<void, ArgTypes...> : public TaskExecutorInstance<void, ArgTypes...>
{
public:
    using Base = TaskExecutorInstance<void, ArgTypes...>;

    ManuallyFulfilledTaskExecutorInstance()
        : Base(static_cast<void(*)(void)>(nullptr))
    {
    }

    ManuallyFulfilledTaskExecutorInstance(const ManuallyFulfilledTaskExecutorInstance &other)               = delete;
    ManuallyFulfilledTaskExecutorInstance &operator=(const ManuallyFulfilledTaskExecutorInstance &other)    = delete;

    ManuallyFulfilledTaskExecutorInstance(ManuallyFulfilledTaskExecutorInstance &&other) noexcept
        : Base(static_cast<Base &&>(other))
    {
    }

    ManuallyFulfilledTaskExecutorInstance &operator=(ManuallyFulfilledTaskExecutorInstance &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        Base::operator=(static_cast<Base &&>(other));

        return *this;
    }
    
    virtual ~ManuallyFulfilledTaskExecutorInstance() override = default;

    void Fulfill()
    {
        AssertThrow(!Base::IsCompleted());

        Base::GetSemaphore().Release(1);
        
        Base::OnComplete();
    }

protected:
    virtual void Execute(ArgTypes...) override final { }
};


template <class ReturnType, class... Args>
class Task;

struct TaskRef
{
    TaskID          id = { };
    SchedulerBase   *assigned_scheduler = nullptr;

    TaskRef() = default;

    TaskRef(TaskID id, SchedulerBase *assigned_scheduler)
        : id(id),
          assigned_scheduler(assigned_scheduler)
    {
    }

    template <class ReturnType, class... Args>
    TaskRef(const Task<ReturnType, Args...> &task)
        : id(task.GetTaskID()),
          assigned_scheduler(task.GetAssignedScheduler())
    {
    }

    TaskRef(const TaskRef &other)             = delete;
    TaskRef &operator=(const TaskRef &other)  = delete;

    TaskRef(TaskRef &&other) noexcept
        : id(other.id),
          assigned_scheduler(other.assigned_scheduler)
    {
        other.id = {};
        other.assigned_scheduler = nullptr;
    }

    TaskRef &operator=(TaskRef &&other) noexcept
    {
        id = other.id;
        assigned_scheduler = other.assigned_scheduler;

        other.id = {};
        other.assigned_scheduler = nullptr;

        return *this;
    }

    ~TaskRef() = default;

    HYP_FORCE_INLINE bool IsValid() const
        { return id.IsValid() && assigned_scheduler != nullptr; }
};

class TaskBase
{
public:
    TaskBase(TaskID id, SchedulerBase *assigned_scheduler)
        : m_id(id),
          m_assigned_scheduler(assigned_scheduler)
    {
    }

    TaskBase(const TaskBase &other)             = delete;
    TaskBase &operator=(const TaskBase &other)  = delete;

    TaskBase(TaskBase &&other) noexcept
        : m_id(other.m_id),
          m_assigned_scheduler(other.m_assigned_scheduler)
    {
        other.m_id = {};
        other.m_assigned_scheduler = nullptr;
    }

    TaskBase &operator=(TaskBase &&other) noexcept
    {
        m_id = other.m_id;
        m_assigned_scheduler = other.m_assigned_scheduler;

        other.m_id = {};
        other.m_assigned_scheduler = nullptr;

        return *this;
    }

    virtual ~TaskBase() = default;

    HYP_FORCE_INLINE TaskID GetTaskID() const
        { return m_id; }

    HYP_FORCE_INLINE SchedulerBase *GetAssignedScheduler() const
        { return m_assigned_scheduler; }

    virtual ITaskExecutor *GetTaskExecutor() const = 0;

    virtual bool IsValid() const
    {
        const ITaskExecutor *executor = GetTaskExecutor();

        return executor != nullptr && executor->GetTaskID().IsValid();
    }

    virtual bool IsCompleted() const final
        { return GetTaskExecutor()->IsCompleted(); }

    /*! \brief Remove the task from the scheduler.
     *  \returns True if the task was successfully cancelled, false otherwise. */
    bool Cancel();

protected:
    virtual void Await_Internal() const;

    void Reset()
    {
        m_id = TaskID::Invalid();
        m_assigned_scheduler = nullptr;
    }

    // Message logging for tasks
    
    void LogWarning(ANSIStringView message) const;

    TaskID          m_id;
    SchedulerBase   *m_assigned_scheduler;
};

// @TODO: Refactor so we can use a custom deleter for the task executor.
// use pre-allocated memory for manually fulfilled instances, so that Fulfill() and Await()
// on a task from the same thread doesn't cost much more than a typical function call.

template <class ReturnType, class... Args>
class Task final : public TaskBase
{
public:
    using Base = TaskBase;
    using TaskExecutorType = TaskExecutorInstance<ReturnType, Args...>;

    // Default constructor, sets task as invalid
    Task()
        : TaskBase({ }, nullptr),
          m_executor(nullptr),
          m_owns_executor(false)
    {
    }

    Task(TaskID id, SchedulerBase *assigned_scheduler, TaskExecutorType *executor, bool owns_executor)
        : TaskBase(id, assigned_scheduler),
          m_executor(executor),
          m_owns_executor(owns_executor)
    {
    }

    Task(const Task &other)             = delete;
    Task &operator=(const Task &other)  = delete;

    Task(Task &&other) noexcept
        : TaskBase(static_cast<TaskBase &&>(other)),
          m_executor(other.m_executor),
          m_owns_executor(other.m_owns_executor)
    {
        other.m_executor = nullptr;
        other.m_owns_executor = false;
    }

    Task &operator=(Task &&other) noexcept
    {
        Reset();

        TaskBase::operator=(static_cast<TaskBase &&>(other));

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

    virtual ITaskExecutor *GetTaskExecutor() const override
    {
        return m_executor;
    }

    /*! \brief Initialize the task without scheduling it.
     *  The task must be resolved with the \ref{Fulfill} method. */
    ManuallyFulfilledTaskExecutorInstance<ReturnType, Args...> *Initialize()
    {
        Reset();

        m_id = TaskID { ~0u };

        m_executor = new ManuallyFulfilledTaskExecutorInstance<ReturnType, Args...>();
        m_owns_executor = true;

        return static_cast<ManuallyFulfilledTaskExecutorInstance<ReturnType, Args...> *>(m_executor);
    }

    template <class... ArgTypes>
    void Fulfill(ArgTypes &&... args)
    {
        AssertThrowMsg(m_assigned_scheduler == nullptr, "Cannot call Fulfill() on a task that has already been initialized");

        ManuallyFulfilledTaskExecutorInstance<ReturnType, Args...> *executor = Initialize();

        executor->Fulfill(ReturnType(std::forward<ArgTypes>(args)...));
    }

    /*! \brief Wait for the task to complete.
     *  \note This function will block the current thread until the task is completed.
     *  \returns The result of the task. */
    HYP_FORCE_INLINE ReturnType &Await() &
    {
        Await_Internal();

        return m_executor->Result();
    }

    /*! \brief Wait for the task to complete.
     *  \note This function will block the current thread until the task is completed.
     *  \returns The result of the task. */
    HYP_FORCE_INLINE const ReturnType &Await() const &
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

    /*! \brief Wait for the task to complete.
     *  \note This function will block the current thread until the task is completed.
     *  \returns The result of the task. */
    HYP_FORCE_INLINE ReturnType Await() const &&
    {
        Await_Internal();

        return m_executor->Result();
    }

protected:
    virtual void Await_Internal() const override
    {
        m_executor->GetSemaphore().Acquire();

#ifdef HYP_DEBUG_MODE
        // Sanity Check
        AssertThrow(IsCompleted());
#endif
    }

    void Reset()
    {
        if (m_owns_executor) {
            // Wait for the task to complete when not in debug mode
            if (IsValid() && !IsCompleted()) {
#ifdef HYP_DEBUG_MODE
                HYP_FAIL("Task was destroyed before it was completed. Waiting on task to complete. Create a fire-and-forget task to prevent this.");
#else
                Base::LogWarning("Task was destroyed before it was completed. Waiting on task to complete. Create a fire-and-forget task to prevent this.");

                Base::Await_Internal();
#endif
            }

            delete m_executor;
        }

        m_executor = nullptr;
        m_owns_executor = false;

        TaskBase::Reset();
    }

private:
    TaskExecutorType    *m_executor;
    bool                m_owns_executor;
};

template <class... Args>
class Task<void, Args...> final : public TaskBase
{
public:
    using Base = TaskBase;
    using TaskExecutorType = TaskExecutorInstance<void, Args...>;

    // Default constructor, sets task as invalid
    Task()
        : TaskBase({ }, nullptr),
          m_executor(nullptr),
          m_owns_executor(false)
    {
    }

    Task(TaskID id, SchedulerBase *assigned_scheduler, TaskExecutorType *executor, bool owns_executor)
        : TaskBase(id, assigned_scheduler),
          m_executor(executor),
          m_owns_executor(owns_executor)
    {
    }

    Task(const Task &other)             = delete;
    Task &operator=(const Task &other)  = delete;

    Task(Task &&other) noexcept
        : TaskBase(static_cast<TaskBase &&>(other)),
          m_executor(other.m_executor),
          m_owns_executor(other.m_owns_executor)
    {
        other.m_executor = nullptr;
        other.m_owns_executor = false;
    }

    Task &operator=(Task &&other) noexcept
    {
        TaskBase::operator=(static_cast<TaskBase &&>(other));

        m_executor = other.m_executor;
        m_owns_executor = other.m_owns_executor;

        other.m_executor = nullptr;
        other.m_owns_executor = false;

        return *this;
    }

    virtual ~Task() override
    {
        if (m_owns_executor) {
            // Wait for the task to complete when not in debug mode
            if (IsValid() && !IsCompleted()) {
#ifdef HYP_DEBUG_MODE
                HYP_FAIL("Task was destroyed before it was completed. Waiting on task to complete. Create a fire-and-forget task to prevent this.");
#else
                Base::LogWarning("Task was destroyed before it was completed. Waiting on task to complete. Create a fire-and-forget task to prevent this.");

                Base::Await_Internal();
#endif
            }

            delete m_executor;
        }

        // otherwise, the executor will be freed when the task is completed
    }

    virtual ITaskExecutor *GetTaskExecutor() const override
    {
        return m_executor;
    }

    /*! \brief Initialize the task without scheduling it.
     *  The task must be resolved with the \ref{Fulfill} method. */
    ManuallyFulfilledTaskExecutorInstance<void, Args...> *Initialize()
    {
        Reset();

        m_id = TaskID { ~0u };

        m_executor = new ManuallyFulfilledTaskExecutorInstance<void, Args...>();
        m_owns_executor = true;

        return static_cast<ManuallyFulfilledTaskExecutorInstance<void, Args...> *>(m_executor);
    }

    // /*! \brief Emplace a value of type \ref{ReturnType} to resolve the task with. Constructs it in place. */
    // void Fulfill(Args... args)
    // {
    //     AssertThrowMsg(m_assigned_scheduler == nullptr, "Cannot manually Fulfill() scheduled tasks!");

    //     m_executor->Execute(std::forward<Args>(args)...);
    //     m_executor->GetSemaphore().Release(1);
    // }

    HYP_FORCE_INLINE void Await()
    {
        Await_Internal();
    }

protected:
    virtual void Await_Internal() const override
    {
        m_executor->GetSemaphore().Acquire();

#ifdef HYP_DEBUG_MODE
        // Sanity Check
        AssertThrow(IsCompleted());
#endif
    }

    void Reset()
    {
        if (m_owns_executor) {
            // Wait for the task to complete when not in debug mode
            if (IsValid() && !IsCompleted()) {
#ifdef HYP_DEBUG_MODE
                HYP_FAIL("Task was destroyed before it was completed. Waiting on task to complete. Create a fire-and-forget task to prevent this.");
#else
                Base::LogWarning("Task was destroyed before it was completed. Waiting on task to complete. Create a fire-and-forget task to prevent this.");

                Base::Await_Internal();
#endif
            }

            delete m_executor;
        }

        m_executor = nullptr;
        m_owns_executor = false;

        TaskBase::Reset();
    }

private:
    TaskExecutorType    *m_executor;
    bool                m_owns_executor;
};

#pragma region AwaitAll

namespace detail {

template <class TaskType>
struct TaskAwaitAll_Impl;

template <class ReturnType, class... ArgTypes>
struct TaskAwaitAll_Impl<Task<ReturnType, ArgTypes...>>
{
    Array<ReturnType> operator()(Span<Task<ReturnType, ArgTypes...>> tasks) const
    {
        for (SizeType i = 0; i < tasks.Size(); ++i) {
            Task<ReturnType, ArgTypes...> &task = tasks[i];

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

                Task<ReturnType, ArgTypes...> &task = tasks[i];

                if (task.IsCompleted()) {
                    completion_states.Set(i, true);

                    semaphore.Release(1);

                    continue;
                }

                // @TODO : What if task finished right before delegate handler was bound?

                task.GetTaskExecutor()->GetOnCompleteDelegate().Bind([&semaphore, &completion_states, &called_states, &tasks, task_index = i]()
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

        Array<ReturnType> results;
        results.ResizeUninitialized(tasks.Size());

        for (SizeType i = 0; i < tasks.Size(); ++i) {
            Task<ReturnType, ArgTypes...> &task = tasks[i];
            AssertThrow(task.IsCompleted());
            
            Memory::Construct<ReturnType>(&results[i], std::move(task.Await()));
        }

        return results;
    }
};

template <class... ArgTypes>
struct TaskAwaitAll_Impl<Task<void, ArgTypes...>>
{
    void operator()(Span<Task<void, ArgTypes...>> tasks) const
    {
        HYP_NOT_IMPLEMENTED_VOID();
    }
};

} // namespace detail

template <class TaskType>
decltype(auto) AwaitAll(Span<TaskType> tasks)
{
    return detail::TaskAwaitAll_Impl<TaskType>{}(tasks);
}

#pragma endregion AwaitAll

} // namespace threading

using threading::Task;
using threading::TaskExecutor;
using threading::TaskID;
using threading::AwaitAll;

} // namespace hyperion

#endif