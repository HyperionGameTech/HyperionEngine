#ifndef HYPERION_V2_TASK_HPP
#define HYPERION_V2_TASK_HPP

#include <core/lib/Proc.hpp>
#include <core/Util.hpp>

// Debugging
#include <system/StackDump.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

struct TaskID
{
    UInt value { 0 };
    
    TaskID &operator=(UInt id)
    {
        value = id;

        return *this;
    }
    
    TaskID &operator=(const TaskID &other) = default;

    bool operator==(UInt id) const { return value == id; }
    bool operator!=(UInt id) const { return value != id; }
    bool operator==(const TaskID &other) const { return value == other.value; }
    bool operator!=(const TaskID &other) const { return value != other.value; }
    bool operator<(const TaskID &other) const { return value < other.value; }

    explicit operator bool() const { return value != 0; }
};

template <class ReturnType, class ...Args>
struct Task
{
    using Function = Proc<ReturnType, Args...>;

    TaskID      id;
    Function    fn;

    constexpr static TaskID empty_id = TaskID { 0 };

    template <class Lambda>
    Task(Lambda &&lambda)
        : id { },
          fn(std::forward<Lambda>(lambda))
    {
    }

    Task(const Task &other)             = delete;
    Task &operator=(const Task &other)  = delete;
    Task(Task &&other) noexcept
        : id(other.id),
          fn(std::move(other.fn))
    {
        other.id = {};
    }

    Task &operator=(Task &&other) noexcept
    {
        id = other.id;
        fn = std::move(other.fn);

        other.id = {};

        return *this;
    }
    
    ~Task() = default;
    
    HYP_FORCE_INLINE ReturnType Execute(Args... args)
        { return fn(std::forward<Args>(args)...); }
    
    HYP_FORCE_INLINE ReturnType operator()(Args... args)
        { return fn(std::forward<Args>(args)...); }
};

} // namespace hyperion::v2

#endif