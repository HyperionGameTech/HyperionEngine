/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Util.hpp>

#include <core/debug/Debug.hpp>

#include <core/memory/Memory.hpp>
#include <core/utilities/ValueStorage.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>
#include <cstddef>

namespace hyperion {
namespace functional {

template <class FunctionSignature, class MemoryType>
struct Proc_Impl;

template <class MemoryType, class ReturnType, class... Args>
struct Proc_Impl<ReturnType(Args...), MemoryType>
{
    ReturnType (*invokeFn)(void*, Args...);
    void (*moveFn)(Proc_Impl*, Proc_Impl*);
    void (*deleteFn)(void*);

    union
    {
        // For inline storage of functor objects - if moveFn is not nullptr, then this memory is used.
        MemoryType memory;

        // For heap-allocation or C function pointer
        void* ptr;
    };

    Proc_Impl()
        : invokeFn(nullptr),
          moveFn(nullptr),
          deleteFn(nullptr)
    {
    }

    Proc_Impl(const Proc_Impl& other) = delete;
    Proc_Impl& operator=(const Proc_Impl& other) = delete;

    Proc_Impl(Proc_Impl&& other) noexcept
        : moveFn(nullptr)
    {
        std::swap(other.moveFn, moveFn);

        if (moveFn != nullptr)
        {
            moveFn(&other, this);
        }
        else
        {
            ptr = other.ptr;
        }

        invokeFn = other.invokeFn;
        other.invokeFn = nullptr;

        deleteFn = other.deleteFn;
        other.deleteFn = nullptr;
    }

    Proc_Impl& operator=(Proc_Impl&& other) noexcept
    {
        if (std::addressof(other) == this)
        {
            return *this;
        }

        void (*otherMoveFn)(Proc_Impl*, Proc_Impl*) = nullptr;
        std::swap(other.moveFn, otherMoveFn);

        if (deleteFn != nullptr)
        {
            deleteFn(GetPointer());
        }

        if (otherMoveFn != nullptr)
        {
            otherMoveFn(&other, this);
        }
        else
        {
            ptr = other.ptr;
        }

        invokeFn = other.invokeFn;
        other.invokeFn = nullptr;

        moveFn = otherMoveFn;

        deleteFn = other.deleteFn;
        other.deleteFn = nullptr;

        return *this;
    }

    ~Proc_Impl()
    {
        if (deleteFn != nullptr)
        {
            deleteFn(GetPointer());
        }
    }

    HYP_FORCE_INLINE bool IsInlineStorage() const
    {
        return moveFn != nullptr;
    }

    HYP_FORCE_INLINE bool HasValue() const
    {
        return invokeFn != nullptr;
    }

    HYP_FORCE_INLINE void* GetPointer()
    {
        return IsInlineStorage()
            ? memory.GetPointer()
            : ptr;
    }

    HYP_FORCE_INLINE void Reset()
    {
        if (deleteFn)
        {
            deleteFn(GetPointer());
        }

        invokeFn = nullptr;
        moveFn = nullptr;
        deleteFn = nullptr;
    }
};

template <class ReturnType, class... Args>
struct Invoker
{
    template <class Func>
    static HYP_NODISCARD ReturnType InvokeFn(void* ptr, Args... args)
    {
        if constexpr (std::is_function_v<std::remove_pointer_t<Func>>)
        {
            return reinterpret_cast<Func>(ptr)(std::forward<Args>(args)...);
        }
        else
        {
            return (*HYP_ALIGN_PTR_AS(ptr, Func))(std::forward<Args>(args)...);
        }
    }
};

// void return type specialization
template <class... Args>
struct Invoker<void, Args...>
{
    template <class Func>
    static void InvokeFn(void* ptr, Args... args)
    {
        if constexpr (std::is_function_v<std::remove_pointer_t<Func>>)
        {
            reinterpret_cast<Func>(ptr)(std::forward<Args>(args)...);
        }
        else
        {
            (*HYP_ALIGN_PTR_AS(ptr, Func))(std::forward<Args>(args)...);
        }
    }
};

class ProcBase
{
};

class ProcRefBase
{
};

/*! \brief A callable object that can store a function pointer or functor object.
 *  This class is a non-copyable alternative to std::function, with inline storage for small functors to avoid heap allocation.
 *  It supports move-only types and can be used to store lambdas, function pointers, or any callable object. */
template <class FunctionSignature>
class Proc;

/*! \brief A reference to any callable object that can be invoked. The reference is non-owning, so is dependent on the lifetime of the original callable object
 *  remaining valid throughout the lifetime of the ProcRef object.
 *  This class is used to reference a Proc<>, functor, or function pointer without taking ownership of the callable object.
 *  It is useful for passing around callable objects without copying them, while still allowing invocation of any generic callable type. */
template <class FunctionSignature>
class ProcRef;

// non-copyable std::function alternative,
// with inline storage so no heap allocation occurs.
// supports move-only types.
template <class ReturnType, class... Args>
class Proc<ReturnType(Args...)> : ProcBase
{
    static constexpr uint32 inlineStorageSizeBytes = 256;

    using InlineStorageType = ValueStorage<char, inlineStorageSizeBytes>;
    using Impl = Proc_Impl<ReturnType(Args...), InlineStorageType>;

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

        m_impl.invokeFn = &Invoker<ReturnType, Args...>::template InvokeFn<FuncNormalized>;
        m_impl.moveFn = nullptr;

        if (sizeof(FuncNormalized) <= sizeof(InlineStorageType))
        {
            m_impl.memory = InlineStorageType();

            void* ptr = &m_impl.memory;
            const uintptr_t addressAligned = HYP_ALIGN_ADDRESS(ptr, alignof(FuncNormalized));

            if (addressAligned + sizeof(FuncNormalized) <= uintptr_t(ptr) + inlineStorageSizeBytes)
            {
                Memory::Construct<FuncNormalized>(std::assume_aligned<alignof(FuncNormalized)>(reinterpret_cast<FuncNormalized*>(addressAligned)), std::forward<Func>(fn));

                m_impl.moveFn = [](Impl* src, Impl* dest)
                {
                    // Gauranteed that src is this - so we can safely get InlineStorageType from src

                    dest->memory = InlineStorageType();

                    FuncNormalized* srcData = HYP_ALIGN_PTR_AS(&src->memory, FuncNormalized);

                    // Dest would already have its memory destroyed or not yet constructed, so we can safely construct into it here.
                    new (HYP_ALIGN_PTR_AS(&dest->memory, FuncNormalized)) FuncNormalized(std::move(*srcData));

                    // Destruct the source data after moving it - now InlineStorageType will not hold any valid data.
                    srcData->~FuncNormalized();

                    // Proc_Impl handles setting the invokeFn, moveFn, and deleteFn members after this is called.
                };

                if constexpr (!std::is_trivially_destructible_v<FuncNormalized>)
                {
                    m_impl.deleteFn = [](void* ptr)
                    {
                        HYP_ALIGN_PTR_AS(ptr, FuncNormalized)->~FuncNormalized();
                    };
                }

                return;
            }
        }

        // Use heap allocation if the functor is too large for inline storage
        m_impl.ptr = Memory::AllocateAndConstruct<FuncNormalized>(std::forward<Func>(fn));
        m_impl.deleteFn = &Memory::DestructAndFree<FuncNormalized>;
    }

    Proc(Proc* fn)
        : Proc()
    {
        if (fn != nullptr && fn->IsValid())
        {
            m_impl.invokeFn = &Invoker<ReturnType, Args...>::template InvokeFn<Proc>;
            m_impl.ptr = reinterpret_cast<void*>(fn);
        }
    }

    /*! \brief Constructs a Proc object from a function pointer or a pointer to a callable object.
     *  \detail If a pointer to a callable object is passed, its lifetime must outlive that of this Proc object, as the object will not be copied. */
    template <class Func, typename = std::enable_if_t<!std::is_base_of_v<ProcBase, Func>>>
    Proc(Func* fn)
        : Proc()
    {
        using FuncNormalized = NormalizedType<Func>;

        if (fn != nullptr)
        {
            m_impl.invokeFn = &Invoker<ReturnType, Args...>::template InvokeFn<FuncNormalized>;
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
        HYP_CORE_ASSERT(m_impl.HasValue());

        return m_impl.invokeFn(m_impl.GetPointer(), std::forward<Args>(args)...);
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
          m_invokeFn(nullptr)
    {
    }

    ProcRef(const Proc<ReturnType(Args...)>& proc)
        : ProcRef(nullptr)
    {
        if (proc.IsValid())
        {
            m_ptr = const_cast<void*>(static_cast<const void*>(&proc));

            m_invokeFn = [](void* ptr, Args... args) -> ReturnType
            {
                const Proc<ReturnType(Args...)>& proc = *static_cast<const Proc<ReturnType(Args...)>*>(ptr);
                HYP_CORE_ASSERT(proc.IsValid(), "Cannot invoke ProcRef referencing invalid Proc");

                return proc(std::forward<Args>(args)...);
            };
        }
    }

    ProcRef(Proc<ReturnType(Args...)>&& proc)
        : ProcRef(nullptr)
    {
        if (proc.IsValid())
        {
            m_ptr = static_cast<void*>(&proc);

            m_invokeFn = [](void* ptr, Args... args) -> ReturnType
            {
                Proc<ReturnType(Args...)>& proc = *static_cast<Proc<ReturnType(Args...)>*>(ptr);
                HYP_CORE_ASSERT(proc.IsValid(), "Cannot invoke ProcRef referencing invalid Proc");

                return proc(std::forward<Args>(args)...);
            };
        }
    }

    template <class Callable, typename = std::enable_if_t<!std::is_pointer_v<NormalizedType<Callable>> && std::is_invocable_v<NormalizedType<Callable>, Args...> && !std::is_base_of_v<ProcRefBase, NormalizedType<Callable>> && !std::is_base_of_v<ProcBase, NormalizedType<Callable>>>>
    ProcRef(Callable&& callable)
        : m_ptr(const_cast<void*>(static_cast<const void*>(&callable)))
    {
        m_invokeFn = [](void* ptr, Args... args) -> ReturnType
        {
            return (*static_cast<NormalizedType<Callable>*>(ptr))(std::forward<Args>(args)...);
        };
    }

    ProcRef(ReturnType (*fn)(Args...))
        : ProcRef(nullptr)
    {
        if (fn != nullptr)
        {
            m_ptr = reinterpret_cast<void*>(fn);

            m_invokeFn = [](void* ptr, Args... args) -> ReturnType
            {
                return (reinterpret_cast<ReturnType (*)(Args...)>(ptr))(std::forward<Args>(args)...);
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
        return m_invokeFn(m_ptr, std::forward<Args>(args)...);
    }

private:
    void* m_ptr;
    ReturnType (*m_invokeFn)(void*, Args...);
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

    ProcRef(T& callable)
        : m_ptr(&callable)
    {
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
