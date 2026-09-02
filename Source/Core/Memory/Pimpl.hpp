/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Memory/Memory.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>

#include <Core/Utilities/ByteUtil.hpp>

#include <Core/Types.hpp>
#include <Core/Constants.hpp>

#include <cstdlib>
#include <cstddef>
#include <new>

namespace Hyperion {
namespace memory {

namespace detail {

struct PimplHeader
{
    void (*destructObject)(void* pObj);
};

/// max_align_t is usually alignof(double) == 8 on x86_64.
/// we use 16 for our alignment of Pimpl, since we have a lot of SIMD types used throughout the engine.
inline constexpr size_t PimplMaxAlign = 16;

inline constexpr size_t PimplObjectOffset = ByteUtil::AlignAs(sizeof(PimplHeader), PimplMaxAlign);

HYP_FORCE_INLINE static PimplHeader* PimplHeaderOf(void* obj)
{
    return reinterpret_cast<PimplHeader*>(static_cast<char*>(obj) - PimplObjectOffset);
}

HYP_FORCE_INLINE static void* PimplBlockOf(void* obj)
{
    return static_cast<char*>(obj) - PimplObjectOffset;
}

} // namespace detail

/*! Hides implementation from outside observers using the PIMPL pattern (pointer to implementation
 *   Bit like a UniquePtr, but users don't need to have the type's implementation at the source */
template <class T>
class Pimpl
{
    template <class TOther>
    friend class Pimpl;

public:
    Pimpl()
        : m_ptr(nullptr)
    {
    }

    Pimpl(std::nullptr_t)
        : m_ptr(nullptr)
    {
    }

    Pimpl(const Pimpl& other) = delete;
    Pimpl& operator=(const Pimpl& other) = delete;

    Pimpl(Pimpl&& other) noexcept
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    Pimpl& operator=(Pimpl&& other) noexcept
    {
        if (m_ptr)
        {
            Reset();
        }

        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;

        return *this;
    }

    ~Pimpl()
    {
        Reset();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return m_ptr == nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const Pimpl& other) const
    {
        return m_ptr == other.m_ptr;
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return m_ptr == nullptr;
    }

    HYP_FORCE_INLINE bool operator!=(const Pimpl& other) const
    {
        return m_ptr != other.m_ptr;
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE T* Get() const
    {
        return static_cast<T*>(m_ptr);
    }

    HYP_FORCE_INLINE T* operator->() const
    {
        return Get();
    }

    HYP_FORCE_INLINE T& operator*() const
    {
        return *Get();
    }

    HYP_FORCE_INLINE bool operator<(const Pimpl& other) const
    {
        return UIntPtr(m_ptr) < UIntPtr(other.m_ptr);
    }

    HYP_FORCE_INLINE void Reset(std::nullptr_t)
    {
        Reset();
    }

    /*! \brief Destroys any currently held object.  */
    HYP_FORCE_INLINE void Reset()
    {
        if (m_ptr)
        {
            detail::PimplHeaderOf(m_ptr)->destructObject(m_ptr);

            m_ptr = nullptr;
        }
    }

    /*! \brief Like Reset(), but constructs the object in-place. */
    template <class... Args>
    HYP_FORCE_INLINE Pimpl& Emplace(Args&&... args)
    {
        return (*this = Construct(std::forward<Args>(args)...));
    }

    /*! \brief Like Emplace() but the first template parameter is specified as the type to construct. */
    template <class Ty, class... Args>
    HYP_FORCE_INLINE Pimpl& EmplaceAs(Args&&... args)
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        return (*this = MakePimpl<Ty>(std::forward<Args>(args)...));
    }

    /*! \brief Constructs a Pimpl<T> from the given arguments. */
    template <class AllocatorType, class... Args>
    HYP_NODISCARD HYP_FORCE_INLINE static Pimpl ConstructWithAllocator(Args&&... args)
    {
        static_assert(alignof(T) <= detail::PimplMaxAlign, "Pimpl<T> requires alignof(T) <= detail::PimplMaxAlign");
        static_assert(std::is_constructible_v<T, Args...>, "T must be constructible using the given args");

        static constexpr size_t TotalSize = detail::PimplObjectOffset + sizeof(T);
        static constexpr size_t BlockAlign = detail::PimplMaxAlign;

        AllocatorType* allocator = GetDefaultAllocatorInstance<AllocatorType>();
        HYP_CORE_ASSERT(allocator != nullptr);

        void* block = allocator->Allocate(TotalSize, BlockAlign);
        HYP_CORE_ASSERT(block != nullptr);

        detail::PimplHeader* header = new (block) detail::PimplHeader;

        header->destructObject = [](void* pObj)
        {
            static_cast<T*>(pObj)->~T();

            AllocatorType* allocator = GetDefaultAllocatorInstance<AllocatorType>();
            HYP_CORE_ASSERT(allocator != nullptr);

            // Free the whole block
            allocator->Free(detail::PimplBlockOf(pObj));
        };

        T* obj = new (static_cast<char*>(block) + detail::PimplObjectOffset) T(std::forward<Args>(args)...);

        Pimpl pimpl;
        pimpl.m_ptr = obj;

        return pimpl;
    }

    /*! \brief Constructs a Pimpl<T> from the given arguments. */
    template <class... Args>
    HYP_NODISCARD HYP_FORCE_INLINE static Pimpl Construct(Args&&... args)
    {
        return Pimpl<T>::template ConstructWithAllocator<DynamicAllocator>(std::forward<Args>(args)...);
    }

private:
    void* m_ptr;
};

/// void specialization
template <>
class Pimpl<void>
{
    template <class TOther>
    friend class Pimpl;

public:
    Pimpl()
        : m_ptr(nullptr)
    {
    }

    Pimpl(std::nullptr_t)
        : m_ptr(nullptr)
    {
    }

    Pimpl(const Pimpl& other) = delete;
    Pimpl& operator=(const Pimpl& other) = delete;

    Pimpl(Pimpl&& other) noexcept
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    Pimpl& operator=(Pimpl&& other) noexcept
    {
        if (m_ptr)
        {
            Reset();
        }

        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;

        return *this;
    }

    template <class Ty>
    Pimpl(Pimpl<Ty>&& other) noexcept
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    template <class Ty>
    Pimpl& operator=(Pimpl<Ty>&& other) noexcept
    {
        if (m_ptr)
        {
            Reset();
        }

        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;

        return *this;
    }

    ~Pimpl()
    {
        Reset();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return m_ptr == nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const Pimpl& other) const
    {
        return m_ptr == other.m_ptr;
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return m_ptr == nullptr;
    }

    HYP_FORCE_INLINE bool operator!=(const Pimpl& other) const
    {
        return m_ptr != other.m_ptr;
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE void* Get() const
    {
        return m_ptr;
    }

    HYP_FORCE_INLINE bool operator<(const Pimpl& other) const
    {
        return UIntPtr(m_ptr) < UIntPtr(other.m_ptr);
    }

    HYP_FORCE_INLINE void Reset(std::nullptr_t)
    {
        Reset();
    }

    /*! \brief Destroys any currently held object.  */
    HYP_FORCE_INLINE void Reset()
    {
        if (m_ptr)
        {
            detail::PimplHeaderOf(m_ptr)->destructObject(m_ptr);

            m_ptr = nullptr;
        }
    }

private:
    void* m_ptr;
};

template <class T, class AllocatorType = DynamicAllocator>
struct MakePimplHelper
{
    template <class... Args>
    static Pimpl<T> MakePimpl(Args&&... args)
    {
        return Pimpl<T>::template ConstructWithAllocator<AllocatorType>(std::forward<Args>(args)...);
    }
};

} // namespace memory

template <class T>
using Pimpl = memory::Pimpl<T>;

template <class T, class... Args>
HYP_FORCE_INLINE Pimpl<T> MakePimpl(Args&&... args)
{
    return memory::MakePimplHelper<T>::MakePimpl(std::forward<Args>(args)...);
}

template <class T, class AllocatorType, class... Args>
HYP_FORCE_INLINE Pimpl<T> MakePimplWithAllocator(Args&&... args)
{
    return memory::MakePimplHelper<T, AllocatorType>::MakePimpl(std::forward<Args>(args)...);
}

} // namespace Hyperion
