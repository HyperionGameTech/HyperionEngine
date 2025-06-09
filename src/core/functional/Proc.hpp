/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PROC_HPP
#define HYPERION_PROC_HPP

#include <core/Defines.hpp>
#include <core/Util.hpp>

#include <core/memory/Memory.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>
#include <cstddef>

namespace hyperion {
namespace functional {

template <class ReturnType, class T2 = void>
struct ProcDefaultReturn;

template <class ReturnType>
struct ProcDefaultReturn<ReturnType, std::enable_if_t<std::is_default_constructible_v<ReturnType> && !std::is_void_v<ReturnType>>>
{
    static ReturnType Get()
    {
        return ReturnType();
    }
};

template <class ReturnType>
struct ProcDefaultReturn<ReturnType, std::enable_if_t<!std::is_default_constructible_v<ReturnType> && !std::is_void_v<ReturnType>>>
{
    static ReturnType Get()
    {
        HYP_FAIL("Cannot get default proc return value for type %s - type is not default constructible", TypeNameHelper<ReturnType>::value.Data());

        HYP_UNREACHABLE();
    }
};

template <class FunctionSignature, class MemoryType>
struct Proc_Impl;

template <class MemoryType, class ReturnType, class... Args>
struct Proc_Impl<ReturnType(Args...), MemoryType>
{
    ReturnType (*invoke_fn)(void*, Args&...);
    void (*move_fn)(Proc_Impl*, Proc_Impl*);
    void (*delete_fn)(void*);

    union
    {
        // For inline storage of functor objects - if move_fn is not nullptr, then this memory is used.
        MemoryType memory;

        // For heap-allocation or C function pointer
        void* ptr;
    };

    Proc_Impl()
        : invoke_fn(nullptr),
          move_fn(nullptr),
          delete_fn(nullptr)
    {
    }

    Proc_Impl(const Proc_Impl& other) = delete;
    Proc_Impl& operator=(const Proc_Impl& other) = delete;

    Proc_Impl(Proc_Impl&& other) noexcept
        : move_fn(nullptr)
    {
        std::swap(other.move_fn, move_fn);

        if (move_fn != nullptr)
        {
            move_fn(&other, this);
        }
        else
        {
            ptr = other.ptr;
        }

        invoke_fn = other.invoke_fn;
        other.invoke_fn = nullptr;

        delete_fn = other.delete_fn;
        other.delete_fn = nullptr;
    }

    Proc_Impl& operator=(Proc_Impl&& other) noexcept
    {
        if (std::addressof(other) == this)
        {
            return *this;
        }

        void (*other_move_fn)(Proc_Impl*, Proc_Impl*) = nullptr;
        std::swap(other.move_fn, other_move_fn);

        if (delete_fn != nullptr)
        {
            delete_fn(GetPointer());
        }

        if (other_move_fn != nullptr)
        {
            other_move_fn(&other, this);
        }
        else
        {
            ptr = other.ptr;
        }

        invoke_fn = other.invoke_fn;
        other.invoke_fn = nullptr;

        move_fn = other_move_fn;

        delete_fn = other.delete_fn;
        other.delete_fn = nullptr;

        return *this;
    }

    ~Proc_Impl()
    {
        if (delete_fn != nullptr)
        {
            delete_fn(GetPointer());
        }
    }

    HYP_FORCE_INLINE bool IsInlineStorage() const
    {
        return move_fn != nullptr;
    }

    HYP_FORCE_INLINE bool HasValue() const
    {
        return invoke_fn != nullptr;
    }

    HYP_FORCE_INLINE void* GetPointer()
    {
        return IsInlineStorage()
            ? memory.GetPointer()
            : ptr;
    }

    HYP_FORCE_INLINE void Reset()
    {
        if (delete_fn)
        {
            delete_fn(GetPointer());
        }

        invoke_fn = nullptr;
        move_fn = nullptr;
        delete_fn = nullptr;
    }
};

template <class ReturnType, class... Args>
struct Invoker
{
    template <class Func>
    static HYP_NODISCARD ReturnType InvokeFn(void* ptr, Args&... args)
    {
        if constexpr (std::is_function_v<std::remove_pointer_t<Func>>)
        {
            return reinterpret_cast<Func>(ptr)(args...);
        }
        else
        {
            return (*HYP_ALIGN_PTR_AS(ptr, Func))(args...);
        }
    }
};

// void return type specialization
template <class... Args>
struct Invoker<void, Args...>
{
    template <class Func>
    static void InvokeFn(void* ptr, Args&... args)
    {
        if constexpr (std::is_function_v<std::remove_pointer_t<Func>>)
        {
            reinterpret_cast<Func>(ptr)(args...);
        }
        else
        {
            (*HYP_ALIGN_PTR_AS(ptr, Func))(args...);
        }
    }
};

class ProcBase
{
};

class ProcRefBase
{
};

template <class FunctionSignature>
class Proc;

template <class FunctionSignature>
class ProcRef;

template <class T>
struct IsProc
{
    static constexpr bool value = false;
};

template <class ReturnType, class... Args>
struct IsProc<Proc<ReturnType(Args...)>>
{
    static constexpr bool value = true;
};

// non-copyable std::function alternative,
// with inline storage so no heap allocation occurs.
// supports move-only types.
template <class ReturnType, class... Args>
class Proc<ReturnType(Args...)> : ProcBase
{
    static constexpr uint32 inline_storage_size_bytes = 256;

    using InlineStorage = ValueStorageArray<char, inline_storage_size_bytes>;
    using Impl = Proc_Impl<ReturnType(Args...), InlineStorage>;

public:
    friend class ProcRef<ReturnType(Args...)>;

    /*! \brief Constructs an empty Proc object. \ref{IsValid} will return false, indicating that the underlying functor object or function pointer is invalid. */
    Proc()
        : m_impl {}
    {
    }

    /*! \brief Constructs an empty Proc object. \ref{IsValid} will return false, indicating that the underlying functor object or function pointer is invalid. */
    Proc(std::nullptr_t)
        : Proc()
    {
    }

    /*! \brief Constructs a Proc object from a callable object. */
    template <class Func, typename = std::enable_if_t<!std::is_pointer_v<NormalizedType<Func>> && !std::is_base_of_v<ProcRefBase, NormalizedType<Func>>>>
    Proc(Func&& fn)
    {
        using FuncNormalized = NormalizedType<Func>;

        static_assert(!std::is_base_of_v<ProcBase, FuncNormalized>, "Object should not be ProcBase");

        m_impl.invoke_fn = &Invoker<ReturnType, Args...>::template InvokeFn<FuncNormalized>;
        m_impl.move_fn = nullptr;

        if (sizeof(FuncNormalized) <= sizeof(InlineStorage))
        {
            m_impl.memory = InlineStorage();

            void* ptr = &m_impl.memory;
            const uintptr_t address_aligned = HYP_ALIGN_ADDRESS(ptr, alignof(FuncNormalized));

            if (address_aligned + sizeof(FuncNormalized) <= uintptr_t(ptr) + inline_storage_size_bytes)
            {
                Memory::Construct<FuncNormalized>(std::assume_aligned<alignof(FuncNormalized)>(reinterpret_cast<FuncNormalized*>(address_aligned)), std::forward<Func>(fn));

                m_impl.move_fn = [](Impl* src, Impl* dest)
                {
                    // Gauranteed that src is this - so we can safely get InlineStorage from src

                    dest->memory = InlineStorage();

                    FuncNormalized* src_data = HYP_ALIGN_PTR_AS(&src->memory, FuncNormalized);

                    // Dest would already have its memory destroyed or not yet constructed, so we can safely construct into it here.
                    new (HYP_ALIGN_PTR_AS(&dest->memory, FuncNormalized)) FuncNormalized(std::move(*src_data));

                    // Destruct the source data after moving it - now InlineStorage will not hold any valid data.
                    src_data->~FuncNormalized();

                    // Proc_Impl handles setting the invoke_fn, move_fn, and delete_fn members after this is called.
                };

                if constexpr (!std::is_trivially_destructible_v<FuncNormalized>)
                {
                    m_impl.delete_fn = [](void* ptr)
                    {
                        HYP_ALIGN_PTR_AS(ptr, FuncNormalized)->~FuncNormalized();
                    };
                }

                return;
            }
        }

        // Use heap allocation if the functor is too large for inline storage
        m_impl.ptr = Memory::AllocateAndConstruct<FuncNormalized>(std::forward<Func>(fn));
        m_impl.delete_fn = &Memory::DestructAndFree<FuncNormalized>;
    }

    Proc(Proc* fn)
        : Proc()
    {
        if (fn != nullptr && fn->IsValid())
        {
            m_impl.invoke_fn = &Invoker<ReturnType, Args...>::template InvokeFn<Proc>;
            m_impl.ptr = reinterpret_cast<void*>(fn);
        }
    }

    /*! \brief Constructs a Proc object from a function pointer or a pointer to a callable object.
     *  \detail If a pointer to a callable object is passed, its lifetime must outlive that of this Proc object, as the object will not be copied. */
    template <class Func, typename = std::enable_if_t<!IsProc<Func>::value>>
    Proc(Func* fn)
        : Proc()
    {
        using FuncNormalized = NormalizedType<Func>;

        if (fn != nullptr)
        {
            m_impl.invoke_fn = &Invoker<ReturnType, Args...>::template InvokeFn<FuncNormalized>;
            m_impl.ptr = reinterpret_cast<void*>(fn);
        }
    }

    Proc(const Proc& other) = delete;
    Proc& operator=(const Proc& other) = delete;

    Proc(Proc&& other) noexcept
        : m_impl(std::move(other.m_impl))
    {
    }

    Proc& operator=(Proc&& other) noexcept
    {
        if (std::addressof(other) == this)
        {
            return *this;
        }

        m_impl = std::move(other.m_impl);

        return *this;
    }

    ~Proc() = default;

    /*! \brief Returns true if the Proc object is valid, false otherwise. */
    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_impl.HasValue();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !m_impl.HasValue();
    }

    /*! \brief Returns true if the Proc object is valid, false otherwise. */
    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_impl.HasValue();
    }

    /*! \brief Invokes the Proc object with the given arguments.
     *  \param args Arguments to pass to the underlying function or lambda.
     *  \return The return value of the underlying function or lambda. If the return type is void, no value is returned. */
    HYP_FORCE_INLINE ReturnType operator()(Args... args) const
    {
        AssertDebug(m_impl.HasValue());

        return m_impl.invoke_fn(m_impl.GetPointer(), args...);
    }

    /*! \brief Resets the Proc object, releasing any resources it may hold. \ref{IsValid} will return false after calling this function. */
    HYP_FORCE_INLINE void Reset()
    {
        m_impl.Reset();
    }

private:
    mutable Impl m_impl;
};

template <class ReturnType, class... Args>
class ProcRef<ReturnType(Args...)> : public ProcRefBase
{
public:
    explicit ProcRef(std::nullptr_t)
        : m_ptr(nullptr),
          m_invoke_fn(nullptr)
    {
    }

    ProcRef(const Proc<ReturnType(Args...)>& proc)
        : ProcRef(nullptr)
    {
        if (proc.IsValid())
        {
            m_ptr = const_cast<void*>(static_cast<const void*>(&proc));

            m_invoke_fn = [](void* ptr, Args&... args) -> ReturnType
            {
                const Proc<ReturnType(Args...)>& proc = *static_cast<const Proc<ReturnType(Args...)>*>(ptr);
                AssertThrowMsg(proc.IsValid(), "Cannot invoke ProcRef referencing invalid Proc");

                return proc(args...);
            };
        }
    }

    template <class Callable, typename = std::enable_if_t<!std::is_pointer_v<NormalizedType<Callable>> && std::is_invocable_v<NormalizedType<Callable>, Args...> && !std::is_base_of_v<ProcRefBase, NormalizedType<Callable>> && !std::is_base_of_v<ProcBase, NormalizedType<Callable>>>>
    ProcRef(Callable&& callable)
        : m_ptr(const_cast<void*>(static_cast<const void*>(&callable)))
    {
        m_invoke_fn = [](void* ptr, Args&... args) -> ReturnType
        {
            return (*static_cast<NormalizedType<Callable>*>(ptr))(args...);
        };
    }

    ProcRef(ReturnType (*fn)(Args...))
        : ProcRef(nullptr)
    {
        if (fn != nullptr)
        {
            m_ptr = reinterpret_cast<void*>(fn);

            m_invoke_fn = [](void* ptr, Args&... args) -> ReturnType
            {
                return (reinterpret_cast<ReturnType (*)(Args...)>(ptr))(args...);
            };
        }
    }

    ProcRef(const ProcRef& other) = default;
    ProcRef& operator=(const ProcRef& other) = default;

    ProcRef(ProcRef&& other) noexcept = default;
    ProcRef& operator=(ProcRef&& other) noexcept = default;

    ~ProcRef() = default;

    /*! \brief Returns true if the Proc object is valid, false otherwise. */
    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return m_ptr == nullptr;
    }

    /*! \brief Returns true if the Proc object is valid, false otherwise. */
    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE ReturnType operator()(Args... args) const
    {
        return m_invoke_fn(m_ptr, args...);
    }

private:
    void* m_ptr;
    ReturnType (*m_invoke_fn)(void*, Args&...);
};

// General specialization for when return type and args cannot be deduced (in the case of a lambda)
// Must be a reference that will not expire until the ProcRef is destroyed.
template <class T>
class ProcRef : public ProcRefBase
{
public:
    explicit ProcRef(std::nullptr_t)
        : m_ptr(nullptr)
    {
    }

    template <class Callable>
    ProcRef(Callable& callable)
        : m_ptr(&callable)
    {
        static_assert(!std::is_rvalue_reference_v<Callable>, "ProcRef cannot be constructed from an rvalue reference. The callable must outlive the ProcRef.");
    }

    ProcRef(const ProcRef& other) = default;
    ProcRef& operator=(const ProcRef& other) = default;

    ProcRef(ProcRef&& other) noexcept = default;
    ProcRef& operator=(ProcRef&& other) noexcept = default;

    ~ProcRef() = default;

    /*! \brief Returns true if the Proc object is valid, false otherwise. */
    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return m_ptr == nullptr;
    }

    /*! \brief Returns true if the Proc object is valid, false otherwise. */
    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_ptr != nullptr;
    }

    template <class... Args>
    HYP_FORCE_INLINE decltype(auto) operator()(Args&&... args) const
    {
        return (*m_ptr)(std::forward<Args>(args)...);
    }

private:
    T* m_ptr;
};

} // namespace functional

using functional::Proc;
using functional::ProcRef;

} // namespace hyperion

#endif