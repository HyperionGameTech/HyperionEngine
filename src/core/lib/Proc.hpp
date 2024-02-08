#ifndef HYPERION_V2_LIB_PROC_HPP
#define HYPERION_V2_LIB_PROC_HPP

#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>
#include <core/lib/CMemory.hpp>
#include <core/lib/Variant.hpp>
#include <util/Defines.hpp>

#include <type_traits>
#include <cstddef>

// temp
#include <functional>

namespace hyperion {
namespace detail {

template <SizeType Sz>
struct ProcFunctorMemory
{
    alignas(std::max_align_t) UByte bytes[Sz];

    HYP_FORCE_INLINE
    void *GetPointer()
        { return &bytes[0]; }

    HYP_FORCE_INLINE
    const void *GetPointer() const
        { return &bytes[0]; }
};

template <class MemoryType, class ReturnType, class ...Args>
struct ProcFunctorInternal
{
    Variant<MemoryType, void *> memory;
    ReturnType                  (*invoke_fn)(void *, Args &&...);
    void                        (*delete_fn)(void *);

    ProcFunctorInternal()
        : invoke_fn(nullptr),
          delete_fn(nullptr)
    {
    }

    ProcFunctorInternal(const ProcFunctorInternal &other) = delete;
    ProcFunctorInternal &operator=(const ProcFunctorInternal &other) = delete;

    ProcFunctorInternal(ProcFunctorInternal &&other) noexcept
    {
        memory = std::move(other.memory);

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
            delete_fn(GetPointer());
        }

        memory = std::move(other.memory);

        invoke_fn = other.invoke_fn;
        other.invoke_fn = nullptr;

        delete_fn = other.delete_fn;
        other.delete_fn = nullptr;

        return *this;
    }

    ~ProcFunctorInternal()
    {
        if (HasValue()) {
            delete_fn(GetPointer());
        }
    }

    HYP_FORCE_INLINE
    bool IsDynamic() const
        { return memory.template Is<void *>(); }
    
    HYP_FORCE_INLINE
    bool HasValue() const
        { return delete_fn != nullptr; }

    HYP_FORCE_INLINE
    void *GetPointer()
    {
        return IsDynamic()
            ? memory.template Get<void *>()
            : memory.template Get<MemoryType>().GetPointer();
    }

    HYP_FORCE_INLINE
    ReturnType Invoke(Args &&... args)
        { return invoke_fn(GetPointer(), std::forward<Args>(args)...); }
};

template <class ReturnType, class ...Args>
struct Invoker
{
    template <class Functor>
    static HYP_FORCE_INLINE
    ReturnType InvokeFn(void *ptr, Args &&... args)
    {
        return (*static_cast<Functor *>(ptr))(std::forward<Args>(args)...);
    }
};

// void return type specialization
template <class ...Args>
struct Invoker<void, Args...>
{
    template <class Functor>
    static HYP_FORCE_INLINE
    void InvokeFn(void *ptr, Args &&... args)
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

    using MemoryDataType = detail::ProcFunctorMemory<inline_storage_size_bytes>;
    using FunctorDataType = detail::ProcFunctorInternal<MemoryDataType, ReturnType, Args...>;

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

        functor.invoke_fn = &detail::Invoker<ReturnType, Args...>::template InvokeFn<Functor>;

        if constexpr (sizeof(Functor) <= sizeof(MemoryDataType::bytes) && alignof(Functor) <= alignof(std::max_align_t)) {
            functor.memory.template Set<MemoryDataType>({ });
            Memory::Construct<Functor>(functor.GetPointer(), std::forward<Functor>(fn));
            functor.delete_fn = &Memory::Destruct<Functor>;
        } else {
            functor.memory.template Set<void *>(Memory::AllocateAndConstruct<Functor>(std::forward<Functor>(fn)));
            functor.delete_fn = &Memory::DestructAndFree<Functor>;
        }
    }

    Proc(const Proc &other)             = delete;
    Proc &operator=(const Proc &other)  = delete;

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

    HYP_FORCE_INLINE
    ReturnType operator()(Args... args)
        { return functor.Invoke(std::forward<Args>(args)...); }
};

} // namespace hyperion

#endif