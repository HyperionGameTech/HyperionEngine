/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PROC_HPP
#define HYPERION_PROC_HPP

#include <core/Defines.hpp>
#include <core/Util.hpp>

#include <core/memory/Memory.hpp>

#include <core/utilities/Variant.hpp>
#include <core/utilities/Tuple.hpp>

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
    ReturnType                  (*invoke_fn)(void *, Args &...);
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
    template <class Func>
    static HYP_NODISCARD ReturnType InvokeFn(void *ptr, Args &... args)
    {
        if constexpr (std::is_function_v<std::remove_pointer_t<Func>>) {
            return reinterpret_cast<Func>(ptr)(args...);
        } else {
            return (*static_cast<Func *>(ptr))(args...);
        }
    }
};

// void return type specialization
template <class... Args>
struct Invoker<void, Args...>
{
    template <class Func>
    static void InvokeFn(void *ptr, Args &... args)
    {
        if constexpr (std::is_function_v<std::remove_pointer_t<Func>>) {
            reinterpret_cast<Func>(ptr)(args...);
        } else {
            (*static_cast<Func *>(ptr))(args...);
        }
    }
};

class ProcBase { };
class ProcRefBase { };

} // namespace detail

template <class ReturnType, class... Args>
class ProcRef;

// non-copyable std::function alternative,
// with inline storage so no heap allocation occurs.
// supports move-only types.
template <class ReturnType, class... Args>
class Proc : detail::ProcBase
{
    static constexpr uint inline_storage_size_bytes = 256;

    using InlineStorage = ValueStorage<ubyte[inline_storage_size_bytes], alignof(std::max_align_t)>;
    using FunctorDataType = detail::ProcFunctorInternal<InlineStorage, ReturnType, Args...>;

public:
    friend class ProcRef<ReturnType, Args...>;

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

    /*! \brief Constructs a Proc object from a callable object. */
    template <class Func, typename = std::enable_if_t< !std::is_pointer_v< NormalizedType<Func> > && !std::is_base_of_v< detail::ProcRefBase, NormalizedType<Func> > > >
    Proc(Func &&fn)
    {
        using FuncNormalized = NormalizedType<Func>;

        static_assert(!std::is_base_of_v<detail::ProcBase, FuncNormalized>, "Object should not be ProcBase");

        // if constexpr (std::is_function_v<std::remove_pointer_t<FuncNormalized>>) {

        functor.invoke_fn = &detail::Invoker<ReturnType, Args...>::template InvokeFn<FuncNormalized>;

        if constexpr (sizeof(FuncNormalized) <= sizeof(InlineStorage) && alignof(Func) <= InlineStorage::alignment) {
            functor.memory.template Set<InlineStorage>({ });
            Memory::Construct<FuncNormalized>(functor.GetPointer(), std::forward<Func>(fn));

            if constexpr (!std::is_trivially_destructible_v<FuncNormalized>) {
                functor.delete_fn = &Memory::Destruct<FuncNormalized>;
            }
        } else {
            // Use heap allocation if the functor is too large for inline storage or alignment doesn't match.
            functor.memory.template Set<void *>(Memory::AllocateAndConstruct<FuncNormalized>(std::forward<Func>(fn)));
            functor.delete_fn = &Memory::DestructAndFree<FuncNormalized>;
        }
    }

    /*! \brief Constructs a Proc object from a function pointer or a pointer to a callable object.
     *  \detail If a pointer to a callable object is passed, its lifetime must outlive that of this Proc object, as the object will not be copied. */
    template <class Func>
    Proc(Func *fn)
        : Proc()
    {
        using FuncNormalized = NormalizedType<Func>;

        if (fn != nullptr) {
            functor.invoke_fn = &detail::Invoker<ReturnType, Args...>::template InvokeFn<FuncNormalized>;
            functor.memory.template Set<void *>(reinterpret_cast<void *>(fn));
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

    HYP_FORCE_INLINE bool operator!() const
        { return !functor.HasValue(); }

    /*! \brief Returns true if the Proc object is valid, false otherwise. */
    HYP_FORCE_INLINE bool IsValid() const
        { return functor.HasValue(); }

    /*! \brief Invokes the Proc object with the given arguments.
     *  \param args Arguments to pass to the underlying function or lambda.
     *  \return The return value of the underlying function or lambda. If the return type is void, no value is returned. */
    HYP_FORCE_INLINE ReturnType operator()(Args... args) const
        { return functor.invoke_fn(functor.GetPointer(), args...); }

    /*! \brief Resets the Proc object, releasing any resources it may hold. \ref{IsValid} will return false after calling this function. */
    HYP_FORCE_INLINE void Reset()
        { functor.Reset(); }

private:
    mutable FunctorDataType functor;
};

template <class ReturnType, class... Args>
class ProcRef : public detail::ProcRefBase
{
    static ReturnType(*const s_invalid_invoke_fn)(void *, Args &...);

public:
    explicit ProcRef(std::nullptr_t)
        : m_ptr(nullptr),
          m_invoke_fn(s_invalid_invoke_fn)
    {
    }
    
    ProcRef(Proc<ReturnType, Args...> &proc)
        : ProcRef(nullptr)
    {
        if (proc.IsValid()) {
            m_ptr = const_cast<void *>(static_cast<const void *>(&proc));

            m_invoke_fn = [](void *ptr, Args &... args) -> ReturnType
            {
                const Proc<ReturnType, Args...> &proc = *static_cast<const Proc<ReturnType, Args...> *>(ptr);
                AssertThrowMsg(proc.IsValid(), "Cannot invoke ProcRef referencing invalid Proc");

                return proc(args...);
            };
        }
    }

    template <class Callable, typename = std::enable_if_t< !std::is_pointer_v< NormalizedType<Callable> > && std::is_invocable_v<NormalizedType<Callable>, Args...> && !std::is_base_of_v< detail::ProcRefBase, NormalizedType< Callable > > && !std::is_base_of_v< detail::ProcBase, NormalizedType< Callable > > > >
    ProcRef(Callable &&callable)
        : m_ptr(const_cast<void *>(static_cast<const void *>(&callable)))
    {
        m_invoke_fn = [](void *ptr, Args &... args) -> ReturnType
        {
            return (*static_cast<NormalizedType<Callable> *>(ptr))(args...);
        };
    }

    ProcRef(ReturnType(*fn)(Args...))
        : ProcRef(nullptr)
    {
        if (fn != nullptr) {
            m_ptr = reinterpret_cast<void *>(fn);

            m_invoke_fn = [](void *ptr, Args &... args) -> ReturnType
            {
                return (reinterpret_cast<ReturnType(*)(Args...)>(ptr))(args...);
            };
        }
    }

    ProcRef(const ProcRef &other)                   = default;
    ProcRef &operator=(const ProcRef &other)        = default;

    ProcRef(ProcRef &&other) noexcept               = default;
    ProcRef &operator=(ProcRef &&other) noexcept    = default;

    ~ProcRef()                                      = default;

    /*! \brief Returns true if the Proc object is valid, false otherwise. */
    HYP_FORCE_INLINE explicit operator bool() const
        { return m_ptr != nullptr; }

    HYP_FORCE_INLINE bool operator!() const
        { return m_ptr == nullptr; }

    /*! \brief Returns true if the Proc object is valid, false otherwise. */
    HYP_FORCE_INLINE bool IsValid() const
        { return m_ptr != nullptr; }

    HYP_FORCE_INLINE ReturnType operator()(Args... args) const
        { return m_invoke_fn(m_ptr, args...); }

private:
    void        *m_ptr;
    ReturnType  (*m_invoke_fn)(void *, Args &...);
};

template <class ReturnType, class... Args>
ReturnType(*const ProcRef<ReturnType, Args...>::s_invalid_invoke_fn)(void *, Args &...) = [](void *, Args &...) -> ReturnType
{
    HYP_FAIL("Cannot invoke ProcRef that is in invalid state");
};

} // namespace functional

using functional::Proc;
using functional::ProcRef;

} // namespace hyperion

#endif