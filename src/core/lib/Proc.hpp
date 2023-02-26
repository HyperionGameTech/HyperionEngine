#ifndef HYPERION_V2_LIB_PROC_HPP
#define HYPERION_V2_LIB_PROC_HPP

#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>
#include <core/lib/CMemory.hpp>
#include <util/Defines.hpp>

#include <type_traits>
#include <cstddef>

namespace hyperion {
namespace detail {

template <SizeType Sz>
struct ProcFunctorMemory
{
    alignas(std::max_align_t) UByte bytes[Sz];

    HYP_FORCE_INLINE void *GetPointer()
        { return &bytes[0]; }

    HYP_FORCE_INLINE const void *GetPointer() const
        { return &bytes[0]; }
};

template <SizeType Sz, class ReturnType, class ...Args>
struct ProcFunctorInternal
{
    ProcFunctorMemory<Sz> memory;
    //static constexpr SizeType alignment = alignof(ProcFunctorMemory<Sz>);

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
        Memory::MemCpy(memory.GetPointer(), other.memory.GetPointer(), sizeof(memory.bytes));

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

        Memory::MemCpy(memory.GetPointer(), other.memory.GetPointer(), sizeof(memory.bytes));

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
    static constexpr UInt inline_storage_size_bytes = 256;

    using FunctorDataType = detail::ProcFunctorInternal<inline_storage_size_bytes, ReturnType, Args...>;

public:

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

        static_assert(alignof(Functor) <= alignof(std::max_align_t), "Alignment not valid to be used as functor!");

        /*if constexpr (std::is_trivially_copyable_v<Functor>) {
            Memory::MemCpy(functor.memory.GetPointer(), &fn, sizeof(Functor));

            if constexpr (sizeof(Functor) < inline_storage_size_bytes) {
                Memory::MemSet(functor.memory.GetPointer() + sizeof(Functor), 0, inline_storage_size_bytes - sizeof(Functor));
            }

            functor.delete_fn = [](void *ptr) { Memory::MemSet(ptr, 0, sizeof(Functor)); };
        } else {*/
            Memory::Construct<Functor>(functor.memory.GetPointer(), std::forward<Functor>(fn));
            functor.delete_fn = &Memory::Destruct<Functor>;
        //}
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

    FunctorDataType functor;

    HYP_FORCE_INLINE ReturnType operator()(Args... args)
        { return functor.Invoke(std::forward<Args>(args)...); }
};

} // namespace hyperion

#endif