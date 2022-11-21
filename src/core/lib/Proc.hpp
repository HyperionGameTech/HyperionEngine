#ifndef HYPERION_V2_LIB_PROC_HPP
#define HYPERION_V2_LIB_PROC_HPP

#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>
#include <core/lib/CMemory.hpp>
#include <util/Defines.hpp>

#include <type_traits>

namespace hyperion {
namespace detail {

template <SizeType Sz>
struct ProcFunctorMemory
{
    UByte bytes[Sz];

    HYP_FORCE_INLINE void *GetPointer()
        { return &bytes[0]; }

    HYP_FORCE_INLINE const void *GetPointer() const
        { return &bytes[0]; }
};

template <SizeType Sz, class ReturnType, class ...Args>
struct ProcFunctorInternal
{
    ProcFunctorMemory<Sz> memory;
    ReturnType (*invoke_fn)(void *, Args &&...);
    void (*delete_fn)(void *);

    ProcFunctorInternal()
        : invoke_fn(nullptr),
          delete_fn(nullptr)
    {
#ifdef HYP_DEBUG_MODE
        Memory::Garble(memory.GetPointer(), sizeof(memory.bytes));
#endif
    }

    ProcFunctorInternal(const ProcFunctorInternal &other) = delete;
    ProcFunctorInternal &operator=(const ProcFunctorInternal &other) = delete;

    ProcFunctorInternal(ProcFunctorInternal &&other) noexcept
    {
        Memory::Copy(memory.GetPointer(), other.memory.GetPointer(), sizeof(memory.bytes));

#ifdef HYP_DEBUG_MODE
        Memory::Garble(other.memory.GetPointer(), sizeof(other.memory.bytes));
#endif

        invoke_fn = other.invoke_fn;
        other.invoke_fn = nullptr;

        delete_fn = other.delete_fn;
        other.delete_fn = nullptr;
    }

    ProcFunctorInternal &operator=(ProcFunctorInternal &&other) noexcept
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        if (HasValue()) {
            delete_fn(memory.GetPointer());
        }

        Memory::Copy(memory.GetPointer(), other.memory.GetPointer(), sizeof(memory.bytes));

#ifdef HYP_DEBUG_MODE
        Memory::Garble(other.memory.GetPointer(), sizeof(other.memory.bytes));
#endif

        invoke_fn = other.invoke_fn;
        other.invoke_fn = nullptr;

        delete_fn = other.delete_fn;
        other.delete_fn = nullptr;

        return *this;
    }

    ~ProcFunctorInternal()
    {
        if (HasValue()) {
            delete_fn(memory.GetPointer());
        }
    }
    
    HYP_FORCE_INLINE bool HasValue() const
        { return delete_fn != nullptr; }

    HYP_FORCE_INLINE ReturnType Invoke(Args &&... args)
        { return invoke_fn(memory.GetPointer(), std::forward<Args>(args)...); }
};

template <class ReturnType, class ...Args>
struct Invoker
{
    template <class Functor>
    static HYP_FORCE_INLINE ReturnType InvokeFn(void *ptr, Args &&... args)
    {
        return (*static_cast<Functor *>(ptr))(std::forward<Args>(args)...);
    }
};

// void return type specialization
template <class ...Args>
struct Invoker<void, Args...>
{
    template <class Functor>
    static HYP_FORCE_INLINE void InvokeFn(void *ptr, Args &&... args)
    {
        (*static_cast<Functor *>(ptr))(std::forward<Args>(args)...);
    }
};

struct ProcBase { };

} // namespace detail

// non-copyable std::function alternative,
// with inline storage so no heap allocation occurs.
// supports move-only types.
template <class ReturnType, class ...Args>
struct Proc : detail::ProcBase
{
    static constexpr UInt inline_storage_size_bytes = 512u;

    Proc()
        : functor { }
    {
    }

    Proc(std::nullptr_t)
        : Proc()
    {
    }

    template <class Functor>
    Proc(Functor &&fn)
    {
        static_assert(!std::is_base_of_v<detail::ProcBase, NormalizedType<Functor>>, "Object should not be ProcBase");

        static_assert(sizeof(Functor) <= sizeof(functor.memory.bytes),
            "Functor object too large; must fit into inline storage");

        functor.invoke_fn = &detail::Invoker<ReturnType, Args...>::template InvokeFn<Functor>;
        functor.delete_fn = &Memory::Destruct<Functor>;

        // static_assert(alignof(Functor) == alignof(decltype(functor.memory)),
        //     "Alignment not valid to be used as functor!");

        {
            Functor tmp = std::forward<Functor>(fn);
            Memory::Set(functor.memory.GetPointer(), 0, inline_storage_size_bytes);
            Memory::Copy(functor.memory.GetPointer(), &tmp, sizeof(tmp));
            Memory::Set(&tmp, 0, sizeof(tmp));
        }
        
        // move-construct the functor object into inline storage
        // Memory::Construct<Functor>(functor.memory.GetPointer(), std::forward<Functor>(fn));
    }

    Proc(const Proc &other) = delete;
    Proc &operator=(const Proc &other) = delete;

    Proc(Proc &&other) noexcept
        : functor(std::move(other.functor))
    {
    }

    Proc &operator=(Proc &&other) noexcept
    {
        functor = std::move(other.functor);

        return *this;
    }

    ~Proc() = default;

    explicit operator bool() const
        { return functor.HasValue(); }

    detail::ProcFunctorInternal<inline_storage_size_bytes, ReturnType, Args...> functor;

    HYP_FORCE_INLINE ReturnType operator()(Args... args)
        { return functor.Invoke(std::forward<Args>(args)...); }
};

} // namespace hyperion

#endif