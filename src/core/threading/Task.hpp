/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_TASK_HPP
#define HYPERION_TASK_HPP

#include <core/functional/Proc.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Thread.hpp>
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
class SchedulerBase;
struct TaskBatch;

struct TaskID
{
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

class TaskExecutorBase
{
public:
    virtual ~TaskExecutorBase() = default;
};

template <class... ArgTypes>
class TaskExecutor  : public TaskExecutorBase
{
public:
    TaskExecutor()
        : m_id { },
          m_initiator_thread_id { },
          m_assigned_scheduler(nullptr),
          m_completed_flag(false)
    {
    }

    TaskExecutor(const TaskExecutor &other)             = delete;
    TaskExecutor &operator=(const TaskExecutor &other)  = delete;

    TaskExecutor(TaskExecutor &&other) noexcept
        : m_id(other.m_id),
          m_initiator_thread_id(other.m_initiator_thread_id),
          m_assigned_scheduler(other.m_assigned_scheduler),
          m_completed_flag(other.m_completed_flag.Exchange(false, MemoryOrder::ACQUIRE_RELEASE))
    {
        other.m_id = {};
        other.m_initiator_thread_id = {};
        other.m_assigned_scheduler = nullptr;
    }

    TaskExecutor &operator=(TaskExecutor &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        m_id = other.m_id;
        m_initiator_thread_id = other.m_initiator_thread_id;
        m_assigned_scheduler = other.m_assigned_scheduler;
        m_completed_flag.Set(other.m_completed_flag.Exchange(false, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);

        other.m_id = {};
        other.m_initiator_thread_id = {};
        other.m_assigned_scheduler = nullptr;

        return *this;
    }

    virtual ~TaskExecutor() = default;

    HYP_FORCE_INLINE TaskID GetTaskID() const
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

    HYP_FORCE_INLINE bool IsCompleted() const
        { return m_completed_flag.Get(MemoryOrder::ACQUIRE); }

    virtual void Execute(ArgTypes... args) = 0;

protected:
    void SetIsCompleted(bool is_completed)
        { m_completed_flag.Set(is_completed, MemoryOrder::RELEASE); }

    TaskID          m_id;
    ThreadID        m_initiator_thread_id;
    SchedulerBase   *m_assigned_scheduler;

    AtomicVar<bool> m_completed_flag;
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

    virtual void Execute(ArgTypes... args) override final
    {
        AssertThrow(m_fn.IsValid());

        m_result_value.Emplace(m_fn(std::forward<ArgTypes>(args)...));

        Base::SetIsCompleted(true);
    }

    HYP_FORCE_INLINE ReturnType &Result() &
        { return m_result_value.Get(); }

    HYP_FORCE_INLINE const ReturnType &Result() const &
        { return m_result_value.Get(); }

    HYP_FORCE_INLINE ReturnType Result() &&
        { return std::move(m_result_value.Get()); }

    HYP_FORCE_INLINE ReturnType Result() const &&
        { return m_result_value.Get(); }

private:
    Function                m_fn;
    Optional<ReturnType>    m_result_value;
};

/*! \brief Specialization for void return type. */
template <class... ArgTypes>
class TaskExecutorInstance<void, ArgTypes...> final : public TaskExecutor<ArgTypes...>
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

    virtual void Execute(ArgTypes... args) override final
    {
        AssertThrow(m_fn.IsValid());
        
        m_fn(std::forward<ArgTypes>(args)...);

        Base::SetIsCompleted(true);
    }

private:
    Function    m_fn;
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

using OnTaskCompletedCallback = Proc<void>;

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

    virtual bool IsValid() const
        { return m_id.IsValid() && m_assigned_scheduler != nullptr; }

    virtual bool IsCompleted() const = 0;

    /*! \brief Remove the task from the scheduler.
     *  \returns True if the task was successfully cancelled, false otherwise. */
    bool Cancel();

protected:
    void Await_Internal() const;

    // Message logging for tasks
    
    void LogWarning(ANSIStringView message) const;

    TaskID          m_id;
    SchedulerBase   *m_assigned_scheduler;
};

template <class ReturnType, class... Args>
class Task final : public TaskBase
{
public:
    using Base = TaskBase;
    using TaskExecutorType = TaskExecutorInstance<ReturnType, Args...>;

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

    virtual bool IsValid() const override
    {
        return TaskBase::IsValid()
            && m_executor != nullptr
            && m_executor->GetTaskID() != 0;
    }

    virtual bool IsCompleted() const override
    {
        return m_executor->IsCompleted();
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

    virtual bool IsValid() const override
    {
        return TaskBase::IsValid()
            && m_executor != nullptr
            && m_executor->GetTaskID() != 0;
    }

    virtual bool IsCompleted() const override
    {
        return m_executor->IsCompleted();
    }

    HYP_FORCE_INLINE void Await()
    {
        Await_Internal();
    }

private:
    TaskExecutorType    *m_executor;
    bool                m_owns_executor;
};

} // namespace threading

using threading::Task;
using threading::TaskExecutor;
using threading::TaskID;

} // namespace hyperion

#endif