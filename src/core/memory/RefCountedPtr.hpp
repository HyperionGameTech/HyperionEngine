/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/utilities/ValueStorage.hpp>
#include <core/utilities/TypeInfo.hpp>

#include <core/memory/Memory.hpp>
#include <core/memory/AnyRef.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/debug/Debug.hpp>

#include <core/utilities/ByteUtil.hpp>

#include <core/Types.hpp>
#include <core/Constants.hpp>

#include <atomic>
#include <cstdlib>

namespace hyperion {

class HypClass;

extern HYP_API const HypClass* GetClass(TypeId typeId);
extern HYP_API bool IsA(const HypClass* hypClass, const void* ptr, TypeId typeId);

namespace memory {

template <class T, class CountType = AtomicVar<uint32>>
class EnableRefCountedPtrFromThis;

template <class T, class CountType>
class RefCountedPtr;

template <class T, class CountType>
class WeakRefCountedPtr;

template <class CountType>
class EnableRefCountedPtrFromThisBase;

template <class CountType>
class WeakRefCountedPtrBase;

template <class CountType>
class RefCountedPtrBase;

template <class CountType>
struct RcBlock
{
    void* obj;
    TypeId typeId;
    CountType strong;
    CountType weak;
    void (*objectDtor)(void*); // destroys/deletes obj
    void (*freeBlock)(void*);  // frees this block
};

namespace detail {

// increment/decrement helpers for CountType (integral or AtomicVar)
template <class C>
HYP_FORCE_INLINE uint32 Inc(C& c)
{
    if constexpr (std::is_integral_v<C>)
        return ++c;
    else
        return c.Increment(1, MemoryOrder::ACQUIRE_RELEASE) + 1;
}

template <class C>
HYP_FORCE_INLINE uint32 Dec(C& c)
{
    if constexpr (std::is_integral_v<C>)
        return --c;
    else
        return c.Decrement(1, MemoryOrder::ACQUIRE_RELEASE) - 1;
}

HYP_FORCE_INLINE void DefaultFreeBlock(void* blk)
{
    HYP_FREE_ALIGNED(blk);
}

// inline object destroyer
template <class T>
HYP_FORCE_INLINE void DestroyInPlace(void* p)
{
    static_cast<T*>(p)->~T();
}

// external-pointer deleter (mirrors Any::ExternalBlockDeleter behavior)
HYP_FORCE_INLINE void ExternalBlockDeleter(void* blk)
{
    auto* b = reinterpret_cast<RcBlock<AtomicVar<uint32>>*>(blk); // CountType not used here
    if (b->obj && b->objectDtor)
        b->objectDtor(b->obj);
    HYP_FREE_ALIGNED(blk);
}

template <class CountType, class T, class... Args>
HYP_NODISCARD HYP_FORCE_INLINE RcBlock<CountType>* NewInlineBlock(Args&&... args)
{
    using U = NormalizedType<T>;
    constexpr SizeType headerSize = sizeof(RcBlock<CountType>);
    constexpr SizeType objAlign = alignof(U);
    constexpr SizeType objOffset = ByteUtil::AlignAs(headerSize, objAlign);
    constexpr SizeType totalSize = objOffset + sizeof(U);
    constexpr SizeType alignment = (alignof(RcBlock<CountType>) > objAlign ? alignof(RcBlock<CountType>) : objAlign);

    void* raw = HYP_ALLOC_ALIGNED(totalSize, alignment);
    void* objMem = reinterpret_cast<void*>(UIntPtr(raw) + objOffset);

    auto* b = new (raw) RcBlock<CountType> {
        TypeId::ForType<U>(),
        nullptr,
        CountType(1),
        CountType(1),
        &DestroyInPlace<U>,
        &DefaultFreeBlock
    };

    try
    {
        U* obj = new (objMem) U(std::forward<Args>(args)...);
        b->obj = obj;
    }
    catch (...)
    {
        // roll back header alloc
        b->freeBlock(b);
        throw;
    }
    return b;
}

template <class CountType, class T>
HYP_NODISCARD HYP_FORCE_INLINE RcBlock<CountType>* NewExternalOwnedBlock(T* ptr)
{
    using U = NormalizedType<T>;

    void* raw = HYP_ALLOC_ALIGNED(sizeof(RcBlock<CountType>), alignof(RcBlock<CountType>));

    auto* b = new (raw) RcBlock<CountType> {
        TypeId::ForType<U>(),
        ptr,
        CountType(1),
        CountType(1),
        &Memory::Delete<U>,
        &DefaultFreeBlock
    };

    return b;
}

template <class CountType>
HYP_FORCE_INLINE uint32 DecWeakAndMaybeFree(RcBlock<CountType>* block)
{
    uint32 count;

    if ((count = Dec(block->weak)) == 0 && block->freeBlock)
        block->freeBlock(block);

    return count;
}

template <class CountType>
HYP_FORCE_INLINE uint32 ReleaseStrong(RcBlock<CountType>* block)
{
    if (!block)
        return 0;

    uint32 count;

    if ((count = Dec(block->strong)) == 0)
    {
        if (block->objectDtor && block->obj)
            block->objectDtor(block->obj);

        block->obj = nullptr;

        DecWeakAndMaybeFree(block);
    }

    return count;
}

template <class CountType>
HYP_FORCE_INLINE uint32 IncStrong(RcBlock<CountType>* block)
{
    if (block)
        return Inc(block->strong);

    return 0;
}

template <class CountType>
HYP_FORCE_INLINE uint32 IncWeak(RcBlock<CountType>* block)
{
    if (block)
        return Inc(block->weak);

    return 0;
}

template <class CountType>
HYP_FORCE_INLINE uint32 ReleaseWeak(RcBlock<CountType>* block)
{
    if (block)
        return DecWeakAndMaybeFree(block);

    return 0;
}

} // namespace detail

template <class CountType>
class WeakRefCountedPtrBase;

template <class CountType>
class RefCountedPtrBase
{
    friend class WeakRefCountedPtrBase<CountType>;

public:
    using Block = RcBlock<CountType>;

    RefCountedPtrBase()
        : m_block(nullptr)
    {
    }

protected:
    RefCountedPtrBase(const RefCountedPtrBase& other)
        : m_block(other.m_block)
    {
        detail::IncStrong(m_block);
    }

    RefCountedPtrBase& operator=(const RefCountedPtrBase& other)
    {
        if (this == &other || m_block == other.m_block)
            return *this;

        detail::ReleaseStrong(m_block);

        m_block = other.m_block;

        detail::IncStrong(m_block);

        return *this;
    }

    RefCountedPtrBase(RefCountedPtrBase&& other) noexcept
        : m_block(other.m_block)
    {
        other.m_block = nullptr;
    }

    RefCountedPtrBase& operator=(RefCountedPtrBase&& other) noexcept
    {
        if (this == &other || m_block == other.m_block)
            return *this;

        detail::ReleaseStrong(m_block);

        m_block = other.m_block;
        other.m_block = nullptr;

        return *this;
    }

public:
    ~RefCountedPtrBase()
    {
        detail::ReleaseStrong(m_block);
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_block != nullptr;
    }

    HYP_FORCE_INLINE bool Any() const
    {
        return m_block && (std::is_integral_v<CountType> ? m_block->strong > 0 : m_block->strong.Get(MemoryOrder::ACQUIRE) != 0);
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return !Any();
    }

    HYP_FORCE_INLINE void* GetVoid() const
    {
        return m_block ? m_block->obj : nullptr;
    }

    HYP_FORCE_INLINE TypeId GetTypeId() const
    {
        return m_block ? m_block->typeId : TypeId::Void();
    }

    HYP_FORCE_INLINE void Reset()
    {
        detail::ReleaseStrong(m_block);

        m_block = nullptr;
    }

    template <class T>
    HYP_FORCE_INLINE void Reset(T* ptr)
    {
        detail::ReleaseStrong(m_block);

        m_block = nullptr;

        if (!ptr)
            return;

        if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<CountType>, NormalizedType<T>>)
        {
            // share object's block
            m_block = ptr->template EnableRefCountedPtrFromThisBase<CountType>::weakThis.GetBlock_Internal();

            detail::IncStrong(m_block);
        }
        else
        {
            m_block = detail::NewExternalOwnedBlock<CountType>(ptr);
        }
    }

    HYP_FORCE_INLINE AnyRef ToRef() const
    {
        return AnyRef(GetTypeId(), GetVoid());
    }

    HYP_FORCE_INLINE Block* GetBlock_Internal() const
    {
        return m_block;
    }

    HYP_FORCE_INLINE void SetBlock_Internal(Block* block, bool incStrong)
    {
        detail::ReleaseStrong(m_block);

        m_block = block;

        if (incStrong)
            detail::IncStrong(m_block);
    }

    Block* m_block;
};

template <class CountType>
class WeakRefCountedPtrBase
{
    friend class RefCountedPtrBase<CountType>;

public:
    using Block = RcBlock<CountType>;

    WeakRefCountedPtrBase()
        : m_block(nullptr)
    {
    }

    WeakRefCountedPtrBase(const RefCountedPtrBase<CountType>& other)
        : m_block(other.m_block)
    {
        detail::IncWeak(m_block);
    }

    WeakRefCountedPtrBase(const WeakRefCountedPtrBase& other)
        : m_block(other.m_block)
    {
        detail::IncWeak(m_block);
    }

    WeakRefCountedPtrBase& operator=(const WeakRefCountedPtrBase& other)
    {
        if (this == &other || m_block == other.m_block)
            return *this;

        detail::ReleaseWeak(m_block);

        m_block = other.m_block;

        detail::IncWeak(m_block);

        return *this;
    }

    WeakRefCountedPtrBase(WeakRefCountedPtrBase&& other) noexcept
        : m_block(other.m_block)
    {
        other.m_block = nullptr;
    }

    WeakRefCountedPtrBase& operator=(WeakRefCountedPtrBase&& other) noexcept
    {
        if (this == &other || m_block == other.m_block)
            return *this;

        detail::ReleaseWeak(m_block);

        m_block = other.m_block;
        other.m_block = nullptr;

        return *this;
    }

    ~WeakRefCountedPtrBase()
    {
        detail::ReleaseWeak(m_block);
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_block != nullptr;
    }
    HYP_FORCE_INLINE bool Any() const
    {
        return m_block && (std::is_integral_v<CountType> ? m_block->weak > 0 : m_block->weak.Get(MemoryOrder::ACQUIRE) != 0);
    }
    HYP_FORCE_INLINE bool Empty() const
    {
        return !Any();
    }

    HYP_FORCE_INLINE void SetBlock_Internal(Block* b, bool incWeak)
    {
        detail::ReleaseWeak(m_block);
        m_block = b;
        if (incWeak)
            detail::IncWeak(m_block);
    }

    HYP_FORCE_INLINE Block* GetBlock_Internal() const
    {
        return m_block;
    }

    Block* m_block;
};

// === Strong typed wrappers ===

template <class T, class CountType>
class RefCountedPtr : public RefCountedPtrBase<CountType>
{
public:
    using Base = RefCountedPtrBase<CountType>;
    using Block = typename Base::Block;

    template <class... Args>
    static RefCountedPtr Construct(Args&&... args)
    {
        RefCountedPtr r;
        r.Base::m_block = detail::NewInlineBlock<CountType, T>(std::forward<Args>(args)...);
        return r;
    }

    RefCountedPtr()
        : Base()
    {
    }

    template <class U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
    explicit RefCountedPtr(U* p)
        : Base()
    {
        Base::template Reset<U>(p);
    }

    RefCountedPtr(std::nullptr_t)
        : Base()
    {
    }

    // delete parent constructors
    RefCountedPtr(const Base&) = delete;
    RefCountedPtr& operator=(const Base&) = delete;
    RefCountedPtr(Base&&) noexcept = delete;
    RefCountedPtr& operator=(Base&&) noexcept = delete;

    RefCountedPtr(const RefCountedPtr& other)
        : Base(static_cast<const Base&>(other))
    {
    }

    RefCountedPtr& operator=(const RefCountedPtr& other)
    {
        Base::operator=(static_cast<const Base&>(other));

        return *this;
    }

    RefCountedPtr(RefCountedPtr&& other) noexcept
        : Base(static_cast<Base&&>(std::move(other)))
    {
    }

    RefCountedPtr& operator=(RefCountedPtr&& other) noexcept
    {
        Base::operator=(static_cast<Base&&>(std::move(other)));

        return *this;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    RefCountedPtr(const RefCountedPtr<Ty, CountType>& other)
        : Base(static_cast<const Base&>(other))
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Types not compatible for upcast!");
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    RefCountedPtr& operator=(const RefCountedPtr<Ty, CountType>& other)
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Types not compatible for upcast!");

        Base::operator=(static_cast<const Base&>(other));

        return *this;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    RefCountedPtr(RefCountedPtr<Ty, CountType>&& other) noexcept
        : Base(static_cast<Base&&>(std::move(other)))
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Types not compatible for upcast!");
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    RefCountedPtr& operator=(RefCountedPtr<Ty, CountType>&& other) noexcept
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Types not compatible for upcast!");

        Base::operator=(static_cast<Base&&>(std::move(other)));

        return *this;
    }

    ~RefCountedPtr() = default;

    HYP_FORCE_INLINE T* Get() const
    {
        return static_cast<T*>(Base::GetVoid());
    }

    HYP_FORCE_INLINE T* operator->() const
    {
        return Get();
    }

    HYP_FORCE_INLINE T& operator*() const
    {
        return *Get();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return Base::GetVoid() != nullptr;
    }

    HYP_FORCE_INLINE operator T*() const
    {
        return Get();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return Base::GetVoid() == nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const RefCountedPtr& other) const
    {
        return Base::GetVoid() == other.GetVoid();
    }

    HYP_FORCE_INLINE bool operator==(const WeakRefCountedPtr<T, CountType>& other) const
    {
        return Base::GetVoid() == other.GetVoid();
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return Base::GetVoid() == nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const T* ptr) const
    {
        return Base::GetVoid() == ptr;
    }

    HYP_FORCE_INLINE bool operator==(T* ptr) const
    {
        return Base::GetVoid() == ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const RefCountedPtr& other) const
    {
        return Base::GetVoid() != other.GetVoid();
    }

    HYP_FORCE_INLINE bool operator!=(const WeakRefCountedPtr<T, CountType>& other) const
    {
        return Base::GetVoid() != other.GetVoid();
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return Base::GetVoid() != nullptr;
    }

    HYP_FORCE_INLINE bool operator!=(T* ptr) const
    {
        return Base::GetVoid() != ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const T* ptr) const
    {
        return Base::GetVoid() != ptr;
    }

    HYP_FORCE_INLINE bool operator<(const RefCountedPtr& other) const
    {
        return Base::GetVoid() < other.GetVoid();
    }

    HYP_FORCE_INLINE bool operator<(const WeakRefCountedPtr<T, CountType>& other) const
    {
        return Base::GetVoid() < other.GetVoid();
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    HYP_FORCE_INLINE explicit operator RefCountedPtr<Ty, CountType>&()
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, "T must be convertible to Ty!");

        return reinterpret_cast<RefCountedPtr<Ty, CountType>&>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    HYP_FORCE_INLINE explicit operator const RefCountedPtr<Ty, CountType>&() const
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, "T must be convertible to Ty!");

        return reinterpret_cast<const RefCountedPtr<Ty, CountType>&>(*this);
    }

    template <class... Args>
    HYP_FORCE_INLINE RefCountedPtr& Emplace(Args&&... args)
    {
        *this = Construct(std::forward<Args>(args)...);
        return *this;
    }

    template <class U, class... Args>
    HYP_FORCE_INLINE RefCountedPtr& EmplaceAs(Args&&... args)
    {
        static_assert(std::is_convertible_v<U*, T*>);
        *this = RefCountedPtr<U, CountType>::Construct(std::forward<Args>(args)...);
        return *this;
    }

    HYP_FORCE_INLINE void Reset()
    {
        Base::Reset();
    }

    template <class U>
    HYP_FORCE_INLINE void Reset(U* p)
    {
        static_assert(std::is_convertible_v<U*, T*>);
        Base::template Reset<U>(p);
    }

    template <class U>
    HYP_FORCE_INLINE bool Is() const
    {
        if constexpr (std::is_void_v<U>)
            return true;

        constexpr TypeId q = TypeId::ForType<U>();

        const void* ptr = Base::GetVoid();
        const TypeId held = Base::GetTypeId();

        return held == q || IsA(GetClass(q), ptr, held);
    }

    template <class U>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<U, CountType> Cast() const
    {
        if (Is<U>())
        {
            RefCountedPtr<U, CountType> r;
            r.Base::SetBlock_Internal(Base::GetBlock_Internal(), true);

            Base::Reset();

            return r;
        }

        return {};
    }

    template <class U>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<U, CountType> CastUnchecked() const
    {
        RefCountedPtr<U, CountType> r;
        r.Base::SetBlock_Internal(Base::GetBlock_Internal(), true);

        return r;
    }

    HYP_NODISCARD HYP_FORCE_INLINE WeakRefCountedPtr<T, CountType> ToWeak() const
    {
        return WeakRefCountedPtr<T, CountType>(*this);
    }
};

template <class CountType>
class RefCountedPtr<void, CountType> : public RefCountedPtrBase<CountType>
{
public:
    using Base = RefCountedPtrBase<CountType>;

    RefCountedPtr()
        : Base()
    {
    }

    RefCountedPtr(std::nullptr_t)
        : Base()
    {
    }

    // delete parent constructors
    RefCountedPtr(const Base&) = delete;
    RefCountedPtr& operator=(const Base&) = delete;
    RefCountedPtr(Base&&) noexcept = delete;
    RefCountedPtr& operator=(Base&&) noexcept = delete;

    template <class Ty>
    RefCountedPtr(const RefCountedPtr<Ty, CountType>& other)
        : Base(static_cast<const Base&>(other))
    {
    }

    template <class Ty>
    RefCountedPtr& operator=(const RefCountedPtr<Ty, CountType>& other)
    {
        Base::operator=(static_cast<const Base&>(other));

        return *this;
    }

    template <class Ty>
    RefCountedPtr(RefCountedPtr<Ty, CountType>&& other) noexcept
        : Base(std::move(other))
    {
    }

    template <class Ty>
    RefCountedPtr& operator=(RefCountedPtr<Ty, CountType>&& other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~RefCountedPtr() = default;

    HYP_FORCE_INLINE void* Get() const
    {
        return Base::GetVoid();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return Base::GetVoid() != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return Base::GetVoid() == nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const RefCountedPtr& other) const
    {
        return Base::GetVoid() == other.GetVoid();
    }

    HYP_FORCE_INLINE bool operator==(const WeakRefCountedPtr<void, CountType>& other) const
    {
        return Base::GetVoid() == other.GetVoid();
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return Base::GetVoid() == nullptr;
    }

    HYP_FORCE_INLINE bool operator==(void* ptr) const
    {
        return Base::GetVoid() == ptr;
    }

    HYP_FORCE_INLINE bool operator==(const void* ptr) const
    {
        return Base::GetVoid() == ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const RefCountedPtr& other) const
    {
        return Base::GetVoid() != other.GetVoid();
    }

    HYP_FORCE_INLINE bool operator!=(const WeakRefCountedPtr<void, CountType>& other) const
    {
        return Base::GetVoid() != other.GetVoid();
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return Base::GetVoid() != nullptr;
    }

    HYP_FORCE_INLINE bool operator!=(void* ptr) const
    {
        return Base::GetVoid() != ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const void* ptr) const
    {
        return Base::GetVoid() != ptr;
    }

    HYP_FORCE_INLINE bool operator<(const RefCountedPtr& other) const
    {
        return Base::GetVoid() < other.GetVoid();
    }

    HYP_FORCE_INLINE bool operator<(const WeakRefCountedPtr<void, CountType>& other) const
    {
        return Base::GetVoid() < other.GetVoid();
    }

    template <class U>
    HYP_FORCE_INLINE bool Is() const
    {
        constexpr TypeId q = TypeId::ForType<U>();
        const void* ptr = Base::GetVoid();
        const TypeId held = Base::GetTypeId();

        return std::is_same_v<U, void> || held == q || IsA(GetClass(q), ptr, held);
    }

    template <class U>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<U, CountType> Cast() const
    {
        if (Is<U>())
        {
            RefCountedPtr<U, CountType> r;
            r.RefCountedPtrBase<CountType>::SetBlock_Internal(Base::GetBlock_Internal(), true);

            return r;
        }

        return {};
    }

    template <class U>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<U, CountType> CastUnchecked() const
    {
        RefCountedPtr<U, CountType> r;
        r.RefCountedPtrBase<CountType>::SetBlock_Internal(Base::GetBlock_Internal(), true);

        return r;
    }

    template <class U>
    HYP_FORCE_INLINE void Reset(U* p)
    {
        Base::template Reset<U>(p);
    }

    HYP_FORCE_INLINE void Reset()
    {
        Base::Reset();
    }
};

template <class T, class CountType>
class WeakRefCountedPtr : public WeakRefCountedPtrBase<CountType>
{
protected:
    using Base = WeakRefCountedPtrBase<CountType>;
    using Block = typename Base::Block;

public:
    WeakRefCountedPtr()
        : Base()
    {
    }

    WeakRefCountedPtr(const WeakRefCountedPtr& other)
        : Base(other)
    {
    }

    WeakRefCountedPtr& operator=(const WeakRefCountedPtr& other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakRefCountedPtr(const RefCountedPtr<T, CountType>& other)
        : Base(other)
    {
    }

    WeakRefCountedPtr& operator=(const RefCountedPtr<T, CountType>& other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakRefCountedPtr(RefCountedPtr<T, CountType>&& other) noexcept = delete;
    WeakRefCountedPtr& operator=(RefCountedPtr<T, CountType>&& other) noexcept = delete;

    WeakRefCountedPtr(WeakRefCountedPtr&& other) noexcept
        : Base(std::move(other))
    {
    }

    WeakRefCountedPtr& operator=(WeakRefCountedPtr&& other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~WeakRefCountedPtr() = default;

    HYP_FORCE_INLINE T* GetUnsafe() const
    {
        Block* block = Base::GetBlock_Internal();

        return block ? static_cast<T*>(block->obj) : nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const WeakRefCountedPtr& other) const
    {
        return GetUnsafe() == other.GetUnsafe();
    }

    HYP_FORCE_INLINE bool operator==(const RefCountedPtr<T, CountType>& other) const
    {
        return GetUnsafe() == other.Get();
    }

    HYP_FORCE_INLINE bool operator!=(const WeakRefCountedPtr& other) const
    {
        return GetUnsafe() != other.GetUnsafe();
    }

    HYP_FORCE_INLINE bool operator!=(const RefCountedPtr<T, CountType>& other) const
    {
        return GetUnsafe() != other.Get();
    }

    HYP_FORCE_INLINE bool operator<(const RefCountedPtr<T, CountType>& other) const
    {
        return GetUnsafe() < other.Get();
    }

    HYP_FORCE_INLINE bool operator<(const WeakRefCountedPtr& other) const
    {
        return GetUnsafe() < other.GetUnsafe();
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    HYP_FORCE_INLINE explicit operator WeakRefCountedPtr<Ty, CountType>&()
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, "T must be convertible to Ty!");

        return reinterpret_cast<WeakRefCountedPtr<Ty, CountType>&>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    HYP_FORCE_INLINE explicit operator const WeakRefCountedPtr<Ty, CountType>&() const
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, "T must be convertible to Ty!");

        return reinterpret_cast<const WeakRefCountedPtr<Ty, CountType>&>(*this);
    }

    HYP_FORCE_INLINE RefCountedPtr<T, CountType> Lock() const
    {
        RefCountedPtr<T, CountType> result;
        Block* block = Base::GetBlock_Internal();
        if (!block)
            return result;

        // @TODO: Fix thread safety issue, check Handle.hpp for proper handling.
        uint32 count;

        if constexpr (std::is_integral_v<CountType>)
        {
            count = block->strong;
        }
        else
        {
            count = block->strong.Get(MemoryOrder::ACQUIRE);
        }

        if (count == 0)
        {
            return result;
        }

        result.RefCountedPtrBase<CountType>::SetBlock_Internal(block, true);

        return result;
    }
};

// Weak<void> specialization
template <class CountType>
class WeakRefCountedPtr<void, CountType> : public WeakRefCountedPtrBase<CountType>
{
protected:
    using Base = WeakRefCountedPtrBase<CountType>;

public:
    WeakRefCountedPtr()
        : Base()
    {
    }

    WeakRefCountedPtr(const WeakRefCountedPtr& other)
        : Base(other)
    {
    }

    WeakRefCountedPtr& operator=(const WeakRefCountedPtr& other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakRefCountedPtr(const RefCountedPtr<void, CountType>& other)
        : Base(other)
    {
    }

    WeakRefCountedPtr& operator=(const RefCountedPtr<void, CountType>& other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakRefCountedPtr(RefCountedPtr<void, CountType>&& other) noexcept = delete;
    WeakRefCountedPtr& operator=(RefCountedPtr<void, CountType>&& other) noexcept = delete;

    WeakRefCountedPtr(WeakRefCountedPtr&& other) noexcept
        : Base(std::move(other))
    {
    }

    WeakRefCountedPtr& operator=(WeakRefCountedPtr&& other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~WeakRefCountedPtr() = default;

    HYP_FORCE_INLINE void* GetUnsafe() const
    {
        auto* b = Base::GetBlock_Internal();
        return b ? b->obj : nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const WeakRefCountedPtr& other) const
    {
        return GetUnsafe() == other.GetUnsafe();
    }

    HYP_FORCE_INLINE bool operator==(const RefCountedPtr<void, CountType>& other) const
    {
        return GetUnsafe() == other.Get();
    }

    HYP_FORCE_INLINE bool operator!=(const WeakRefCountedPtr& other) const
    {
        return GetUnsafe() != other.GetUnsafe();
    }

    HYP_FORCE_INLINE bool operator!=(const RefCountedPtr<void, CountType>& other) const
    {
        return GetUnsafe() != other.Get();
    }

    HYP_FORCE_INLINE bool operator<(const RefCountedPtr<void, CountType>& other) const
    {
        return GetUnsafe() < other.Get();
    }

    HYP_FORCE_INLINE bool operator<(const WeakRefCountedPtr& other) const
    {
        return GetUnsafe() < other.GetUnsafe();
    }

    template <class Ty>
    HYP_NODISCARD HYP_FORCE_INLINE WeakRefCountedPtr<Ty, CountType> CastUnchecked() const
    {
        WeakRefCountedPtr<Ty, CountType> weak;

        if (Base::IsValid())
        {
            weak.SetBlock_Internal(Base::GetBlock_Internal(), true);
        }

        return weak;
    }
};

// enable-from-this using an external owned block pointing at `this`
template <class T, class CountType>
class EnableRefCountedPtrFromThis;

template <class CountType>
class EnableRefCountedPtrFromThisBase
{
public:
    WeakRefCountedPtr<void, CountType> weakThis;

protected:
    EnableRefCountedPtrFromThisBase() = default;
    virtual ~EnableRefCountedPtrFromThisBase() = default;
};

template <class T, class CountType>
class EnableRefCountedPtrFromThis : public EnableRefCountedPtrFromThisBase<CountType>
{
    using Base = EnableRefCountedPtrFromThisBase<CountType>;

public:
    EnableRefCountedPtrFromThis()
    {
        using BlockType = RcBlock<CountType>;

        void* raw = HYP_ALLOC_ALIGNED(sizeof(BlockType), alignof(BlockType));
        BlockType* block = new (raw) BlockType {
            TypeId::ForType<T>(),
            static_cast<T*>(this),
            CountType(0),
            CountType(1),
            &Memory::Delete<T>,
            &detail::DefaultFreeBlock
        };

        Base::weakThis.WeakRefCountedPtrBase<CountType>::SetBlock_Internal(block, /*incWeak*/ false);
    }

    RefCountedPtr<T, CountType> RefCountedPtrFromThis() const
    {
        return Base::weakThis.template CastUnchecked<T>().Lock();
    }

    WeakRefCountedPtr<T, CountType> WeakRefCountedPtrFromThis() const
    {
        WeakRefCountedPtr<T, CountType> weak;
        weak.WeakRefCountedPtrBase<CountType>::SetBlock_Internal(Base::weakThis.WeakRefCountedPtrBase<CountType>::GetBlock_Internal(), true);

        return weak;
    }
};

template <class T, class CountType = AtomicVar<uint32>>
struct MakeRefCountedPtrHelper
{
    template <class... Args>
    static RefCountedPtr<T, CountType> MakeRefCountedPtr(Args&&... args)
    {
        return RefCountedPtr<T, CountType>::Construct(std::forward<Args>(args)...);
    }
};

template <class T, class CountType = AtomicVar<uint32>>
using RefCountedPtrAlias = RefCountedPtr<T, CountType>;

} // namespace memory

template <class T, class CountType = AtomicVar<uint32>>
using RC = hyperion::memory::RefCountedPtr<T, CountType>;

template <class T, class CountType = AtomicVar<uint32>>
using Weak = hyperion::memory::WeakRefCountedPtr<T, CountType>;

template <class CountType = AtomicVar<uint32>>
using EnableRefCountedPtrFromThisBase = hyperion::memory::EnableRefCountedPtrFromThisBase<CountType>;

template <class T, class CountType = AtomicVar<uint32>>
using EnableRefCountedPtrFromThis = hyperion::memory::EnableRefCountedPtrFromThis<T, CountType>;

template <class T, class... Args>
HYP_FORCE_INLINE RC<T> MakeRefCountedPtr(Args&&... args)
{
    return memory::MakeRefCountedPtrHelper<T, AtomicVar<uint32>>::MakeRefCountedPtr(std::forward<Args>(args)...);
}

} // namespace hyperion
