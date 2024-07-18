/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PROC_HPP
#define HYPERION_PROC_HPP

#include <core/Defines.hpp>
#include <core/memory/Memory.hpp>
#include <core/utilities/Variant.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>
#include <cstddef>

namespace hyperion {
namespace functional {
namespace detail {

template <class MemoryType, class ReturnType, class... Args>
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

    ProcFunctorInternal(const ProcFunctorInternal &other)               = delete;
    ProcFunctorInternal &operator=(const ProcFunctorInternal &other)    = delete;

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

        if (delete_fn != nullptr) {
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
        if (delete_fn != nullptr) {
            delete_fn(GetPointer());
        }
    }

    HYP_FORCE_INLINE bool IsDynamicallyAllocated() const
        { return memory.template Is<void *>(); }
    
    HYP_FORCE_INLINE bool HasValue() const
        { return memory.HasValue(); }

    HYP_FORCE_INLINE void *GetPointer()
    {
        return IsDynamicallyAllocated()
            ? memory.template GetUnchecked<void *>()
            : memory.template GetUnchecked<MemoryType>().GetPointer();
    }

    HYP_FORCE_INLINE ReturnType Invoke(Args &&... args)
        { return invoke_fn(GetPointer(), std::forward<Args>(args)...); }

    HYP_FORCE_INLINE void Reset()
    {
        if (delete_fn) {
            delete_fn(GetPointer());
        }

        memory.Reset();
        invoke_fn = nullptr;
        delete_fn = nullptr;
    }
};

template <class ReturnType, class... Args>
struct Invoker
{
    template <class Functor>
    static HYP_NODISCARD ReturnType InvokeFn(void *ptr, Args &&... args)
    {
        return (*static_cast<Functor *>(ptr))(std::forward<Args>(args)...);
    }
};

// void return type specialization
template <class... Args>
struct Invoker<void, Args...>
{
    template <class Functor>
    static void InvokeFn(void *ptr, Args &&... args)
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
    static constexpr uint inline_storage_size_bytes = 256;

    using InlineStorage = ValueStorage<ubyte[inline_storage_size_bytes], alignof(std::max_align_t)>;
    using FunctorDataType = detail::ProcFunctorInternal<InlineStorage, ReturnType, Args...>;

public:
    /*! \brief Constructs an empty Proc object. \ref{IsValid} will return false, indicating that the underlying functor object or function pointer is invalid. */
    Proc()
        : functor { }
    {
    }

    /*! \brief Constructs an empty Proc object. \ref{IsValid} will return false, indicating that the underlying functor object or function pointer is invalid. */
    Proc(std::nullptr_t)
        : Proc()
    {
    }

    /*! \brief Constructs a Proc object from a lambda or function pointer. */
    template <class Functor>
    Proc(Functor &&fn)
    {
        using FunctorNormalized = NormalizedType<Functor>;

        static_assert(!std::is_base_of_v<detail::ProcBase, FunctorNormalized>, "Object should not be ProcBase");

        // if constexpr (std::is_function_v<std::remove_pointer_t<FunctorNormalized>>) {

        functor.invoke_fn = &detail::Invoker<ReturnType, Args...>::template InvokeFn<FunctorNormalized>;

        if constexpr (sizeof(FunctorNormalized) <= sizeof(InlineStorage) && alignof(Functor) <= InlineStorage::alignment) {
            functor.memory.template Set<InlineStorage>({ });
            Memory::Construct<FunctorNormalized>(functor.GetPointer(), std::forward<Functor>(fn));

            if constexpr (!std::is_trivially_destructible_v<FunctorNormalized>) {
                functor.delete_fn = &Memory::Destruct<FunctorNormalized>;
            }
        } else {
            // Use heap allocation if the functor is too large for inline storage or alignment doesn't match.
            functor.memory.template Set<void *>(Memory::AllocateAndConstruct<FunctorNormalized>(std::forward<Functor>(fn)));
            functor.delete_fn = &Memory::DestructAndFree<FunctorNormalized>;
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

    /*! \brief Returns true if the Proc object is valid, false otherwise. */
    HYP_FORCE_INLINE explicit operator bool() const
        { return functor.HasValue(); }

    /*! \brief Returns true if the Proc object is valid, false otherwise. */
    HYP_FORCE_INLINE bool IsValid() const
        { return functor.HasValue(); }

    /*! \brief Invokes the Proc object with the given arguments.
     *  \param args Arguments to pass to the underlying function or lambda.
     *  \return The return value of the underlying function or lambda. If the return type is void, no value is returned. */
    HYP_FORCE_INLINE ReturnType operator()(Args... args) const
        { return functor.Invoke(std::forward<Args>(args)...); }

    /*! \brief Resets the Proc object, releasing any resources it may hold. \ref{IsValid} will return false after calling this function. */
    HYP_FORCE_INLINE void Reset()
        { functor.Reset(); }

    mutable FunctorDataType functor;
};

} // namespace functional

using functional::Proc;

} // namespace hyperion

#endif