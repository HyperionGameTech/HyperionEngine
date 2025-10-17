/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/utilities/ValueStorage.hpp>
#include <core/reflection/TypeId.hpp>
#include <core/reflection/TypeInfo.hpp>

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
struct ControlBlock
{
    void* pObj;
    const TypeInfo* typeInfo;
    CountType strong;
    CountType weak;

    void (*pFnDestructObj)(void*); // destructs and/or deletes pObj (decided based on allocation strategy)
    void (*pFnFreeBlock)(void*);   // frees this block
};

namespace detail {

HYP_API extern void DefaultFreeBlock(void* blk);

HYP_API extern void ExternalBlockDeleter(void* blk);

template <class CountType>
HYP_API extern uint32 IncStrong(ControlBlock<CountType>* block);

template <class CountType>
HYP_API extern uint32 ReleaseStrong(ControlBlock<CountType>* block);

template <class CountType>
HYP_API extern uint32 IncWeak(ControlBlock<CountType>* block);

template <class CountType>
HYP_API extern uint32 ReleaseWeak(ControlBlock<CountType>* block);

template <class CountType, class T>
HYP_API ControlBlock<CountType>* NewExternalOwnedBlock(T* ptr)
{
    void* pBlock = HYP_ALLOC_ALIGNED(sizeof(ControlBlock<CountType>), alignof(ControlBlock<CountType>));

    return new (pBlock) ControlBlock<CountType> {
        ptr,
        &TypeInfo_ForType<T>(),
        CountType(1),
        CountType(1),
        &Memory::Delete<T>,
        &DefaultFreeBlock
    };
}

template <class CountType, class T, class... Args>
HYP_NODISCARD static inline ControlBlock<CountType>* NewInlineBlock(Args&&... args)
{
    constexpr SizeType headerSize = sizeof(ControlBlock<CountType>);
    constexpr SizeType objAlign = alignof(T);
    constexpr SizeType objOffset = ByteUtil::AlignAs(headerSize, objAlign);
    constexpr SizeType alignment = (alignof(ControlBlock<CountType>) > objAlign ? alignof(ControlBlock<CountType>) : objAlign);
    constexpr SizeType totalSize = ByteUtil::AlignAs(objOffset + sizeof(T), alignment);

    void* pBlock = HYP_ALLOC_ALIGNED(totalSize, alignment);

    // object is stored in the block
    void* pObj = reinterpret_cast<void*>(UIntPtr(pBlock) + objOffset);

    new (pBlock) ControlBlock<CountType> {
        static_cast<T*>(pObj),
        &TypeInfo_ForType<T>(),
        CountType(1),
        CountType(1),
        &Memory::Destruct<T>,
        &DefaultFreeBlock
    };

    new (pObj) T(std::forward<Args>(args)...);

    return static_cast<ControlBlock<CountType>*>(pBlock);
}

} // namespace detail

template <class CountType>
class WeakRefCountedPtrBase;

template <class CountType>
class RefCountedPtrBase
{
    friend class WeakRefCountedPtrBase<CountType>;

    template <class T, class OtherCountType>
    friend class RefCountedPtr;

public:
    using Block = ControlBlock<CountType>;

    RefCountedPtrBase();

protected:
    RefCountedPtrBase(const RefCountedPtrBase& other);
    RefCountedPtrBase& operator=(const RefCountedPtrBase& other);
    RefCountedPtrBase(RefCountedPtrBase&& other) noexcept;
    RefCountedPtrBase& operator=(RefCountedPtrBase&& other) noexcept;

public:
    ~RefCountedPtrBase();

    bool IsValid() const;
    void* GetVoid() const;
    const TypeInfo& GetTypeInfo() const;
    const TypeId& GetTypeId() const;
    void Reset();

    template <class T>
    void Reset(T* ptr);

    AnyRef ToRef() const;
    Block* GetBlock_Internal() const;
    void SetBlock_Internal(Block* block, bool incStrong);

private:
    Block* m_block;
};

template <class CountType>
class WeakRefCountedPtrBase
{
    friend class RefCountedPtrBase<CountType>;

public:
    using Block = ControlBlock<CountType>;

    WeakRefCountedPtrBase();
    WeakRefCountedPtrBase(const RefCountedPtrBase<CountType>& other);
    WeakRefCountedPtrBase(const WeakRefCountedPtrBase& other);
    WeakRefCountedPtrBase& operator=(const WeakRefCountedPtrBase& other);
    WeakRefCountedPtrBase(WeakRefCountedPtrBase&& other) noexcept;
    WeakRefCountedPtrBase& operator=(WeakRefCountedPtrBase&& other) noexcept;
    ~WeakRefCountedPtrBase();

    bool IsValid() const;
    void SetBlock_Internal(Block* block, bool incWeak);
    Block* GetBlock_Internal() const;

private:
    Block* m_block;
};

// Template member function implementations that can't be explicitly instantiated
template <class CountType>
template <class T>
void RefCountedPtrBase<CountType>::Reset(T* ptr)
{
    detail::ReleaseStrong(m_block);

    m_block = nullptr;

    if (!ptr)
    {
        return;
    }

    if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<CountType>, NormalizedType<T>>)
    {
        // share object's block
        m_block = ptr->template EnableRefCountedPtrFromThisBase<CountType>::m_block;

        detail::IncStrong(m_block);
    }
    else
    {
        m_block = detail::NewExternalOwnedBlock<CountType>(ptr);
    }
}

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
        RefCountedPtr result;

        if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<CountType>, NormalizedType<T>>)
        {
            T* obj = new T(std::forward<Args>(args)...);

            result.SetBlock_Internal(obj->template EnableRefCountedPtrFromThisBase<CountType>::m_block, /* incStrong */ false);
        }
        else
        {
            result.SetBlock_Internal(detail::NewInlineBlock<CountType, T>(std::forward<Args>(args)...), /* incStrong */ false);
        }

        return result;
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
        return Get() != nullptr;
    }

    HYP_FORCE_INLINE operator T*() const
    {
        return Get();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return Get() == nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const RefCountedPtr& other) const
    {
        return Get() == other.Get();
    }

    HYP_FORCE_INLINE bool operator==(const WeakRefCountedPtr<T, CountType>& other) const
    {
        return Get() == other.Get();
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return Get() == nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const T* ptr) const
    {
        return Get() == ptr;
    }

    HYP_FORCE_INLINE bool operator==(T* ptr) const
    {
        return Get() == ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const RefCountedPtr& other) const
    {
        return Get() != other.Get();
    }

    HYP_FORCE_INLINE bool operator!=(const WeakRefCountedPtr<T, CountType>& other) const
    {
        return Get() != other.Get();
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return Get() != nullptr;
    }

    HYP_FORCE_INLINE bool operator!=(T* ptr) const
    {
        return Get() != ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const T* ptr) const
    {
        return Get() != ptr;
    }

    HYP_FORCE_INLINE bool operator<(const RefCountedPtr& other) const
    {
        return Get() < other.Get();
    }

    HYP_FORCE_INLINE bool operator<(const WeakRefCountedPtr<T, CountType>& other) const
    {
        return Get() < other.Get();
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

        constexpr TypeId typeId = TypeId::ForType<U>();

        const void* ptr = Base::GetVoid();
        const TypeId currentTypeId = Base::GetTypeId();

        return currentTypeId == typeId || IsA(GetClass(typeId), ptr, currentTypeId);
    }

    template <class U>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<U, CountType> Cast() const
    {
        if (Is<U>())
        {
            RefCountedPtr<U, CountType> result;
            result.Base::SetBlock_Internal(Base::GetBlock_Internal(), true);

            return result;
        }

        return {};
    }

    template <class U>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<U, CountType> CastUnchecked() const
    {
        RefCountedPtr<U, CountType> result;
        result.Base::SetBlock_Internal(Base::GetBlock_Internal(), true);

        return result;
    }

    HYP_NODISCARD HYP_FORCE_INLINE WeakRefCountedPtr<T, CountType> ToWeak() const
    {
        return WeakRefCountedPtr<T, CountType>(*this);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(Get());
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
        constexpr TypeId typeId = TypeId::ForType<U>();

        const void* ptr = Base::GetVoid();
        const TypeId currentTypeId = Base::GetTypeId();

        return std::is_same_v<U, void> || currentTypeId == typeId || IsA(GetClass(typeId), ptr, currentTypeId);
    }

    template <class U>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<U, CountType> Cast() const
    {
        if (Is<U>())
        {
            RefCountedPtr<U, CountType> result;
            result.RefCountedPtrBase<CountType>::SetBlock_Internal(Base::GetBlock_Internal(), true);

            return result;
        }

        return {};
    }

    template <class U>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<U, CountType> CastUnchecked() const
    {
        RefCountedPtr<U, CountType> result;
        result.RefCountedPtrBase<CountType>::SetBlock_Internal(Base::GetBlock_Internal(), true);

        return result;
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

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(Get());
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

        return block ? reinterpret_cast<T*>(block->pObj) : nullptr;
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
        {
            return result;
        }

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

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(GetUnsafe());
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
        auto* block = Base::GetBlock_Internal();

        return block ? block->pObj : nullptr;
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
        weak.SetBlock_Internal(Base::GetBlock_Internal(), true);

        return weak;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(GetUnsafe());
    }
};

// enable-from-this using an external owned block pointing at `this`
template <class T, class CountType>
class EnableRefCountedPtrFromThis;

template <class CountType>
class EnableRefCountedPtrFromThisBase
{
    template <class OtherCountType>
    friend class RefCountedPtrBase;

    template <class OtherT, class OtherCountType>
    friend class RefCountedPtr;

protected:
    using BlockType = ControlBlock<CountType>;

    EnableRefCountedPtrFromThisBase() = default;
    virtual ~EnableRefCountedPtrFromThisBase() = default;

    BlockType* m_block;
};

template <class T, class CountType>
class EnableRefCountedPtrFromThis : public EnableRefCountedPtrFromThisBase<CountType>
{
    using Base = EnableRefCountedPtrFromThisBase<CountType>;

public:
    EnableRefCountedPtrFromThis()
    {
        using BlockType = typename Base::BlockType;

        Base::m_block = (BlockType*)HYP_ALLOC_ALIGNED(sizeof(BlockType), alignof(BlockType));

        new (Base::m_block) BlockType {
            static_cast<T*>(this),
            &TypeInfo_ForType<T>(),
            CountType(1),
            CountType(1),
            &Memory::Destruct<T>,
            &detail::DefaultFreeBlock
        };
    }

    RefCountedPtr<T, CountType> RefCountedPtrFromThis() const
    {
        RefCountedPtr<T, CountType> result;
        result.RefCountedPtrBase<CountType>::SetBlock_Internal(Base::m_block, /* incStrong */ true);

        return result;
    }

    WeakRefCountedPtr<T, CountType> WeakRefCountedPtrFromThis() const
    {
        WeakRefCountedPtr<T, CountType> result;
        result.WeakRefCountedPtrBase<CountType>::SetBlock_Internal(Base::m_block, /* incWeak */ true);

        return result;
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
