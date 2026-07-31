/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Utilities/ValueStorage.hpp>
#include <Core/Reflection/TypeId.hpp>
#include <Core/Reflection/TypeInfo.hpp>

#include <Core/Memory/Memory.hpp>
#include <Core/Memory/AnyRef.hpp>

#include <Core/Threading/AtomicVar.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Utilities/ByteUtil.hpp>

#include <Core/Types.hpp>
#include <Core/Constants.hpp>

#include <atomic>
#include <cstdlib>

namespace Hyperion {

class Class;

CORE_API extern const Class* GetClass(const TypeId& typeId);
CORE_API extern bool IsA(const Class* cls, const void* ptr, const TypeId& typeId);

namespace memory {

template <class CountType = AtomicVar<uint32>>
class SharedFromThisBase;

template <class T, class CountType = AtomicVar<uint32>>
class SharedFromThis;

template <class CountType = AtomicVar<uint32>>
class SharedPtrBase;

template <class T, class CountType = AtomicVar<uint32>>
class SharedPtr;

template <class CountType = AtomicVar<uint32>>
class WeakPtrBase;

template <class T, class CountType = AtomicVar<uint32>>
class WeakPtr;

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

template <class AllocatorType>
inline void DefaultFreeBlock(void* blk)
{
    GetDefaultAllocatorInstance<AllocatorType>()->Free(blk);
}

template <class CountType>
CORE_API extern uint32 IncStrong(ControlBlock<CountType>* block);

template <class CountType>
CORE_API extern uint32 ReleaseStrong(ControlBlock<CountType>* block);

template <class CountType>
CORE_API extern uint32 IncWeak(ControlBlock<CountType>* block);

template <class CountType>
CORE_API extern uint32 ReleaseWeak(ControlBlock<CountType>* block);

template <class CountType, class AllocatorType, class T>
ControlBlock<CountType>* NewExternalOwnedBlock(T* ptr)
{
    void* pBlock = GetDefaultAllocatorInstance<AllocatorType>()->Allocate(sizeof(ControlBlock<CountType>), alignof(ControlBlock<CountType>));

    void (*deleter)(void*) = [](void* ptr) -> void
    {
        // Since we don't know where ptr is allocated from, we have to assume T overloads
        // operator delete to properly deallocate the memory for it.
        delete static_cast<T*>(ptr);
    };

    return new (pBlock) ControlBlock<CountType> {
        ptr,
        &TypeOf<T>(),
        CountType(1),
        CountType(1),
        deleter,
        &DefaultFreeBlock<AllocatorType>
    };
}

template <class CountType, class T, class AllocatorType, class... Args>
HYP_NODISCARD static inline ControlBlock<CountType>* NewInlineBlock(Args&&... args)
{
    constexpr size_t headerSize = sizeof(ControlBlock<CountType>);
    constexpr size_t objAlign = alignof(T);
    constexpr size_t objOffset = ByteUtil::AlignAs(headerSize, objAlign);
    constexpr size_t alignment = (alignof(ControlBlock<CountType>) > objAlign ? alignof(ControlBlock<CountType>) : objAlign);
    constexpr size_t totalSize = ByteUtil::AlignAs(objOffset + sizeof(T), alignment);

    void* pBlock = GetDefaultAllocatorInstance<AllocatorType>()->Allocate(totalSize, alignment);

    // object is stored in the block
    void* pObj = reinterpret_cast<void*>(UIntPtr(pBlock) + objOffset);

    new (pBlock) ControlBlock<CountType> {
        static_cast<T*>(pObj),
        &TypeOf<T>(),
        CountType(1),
        CountType(1),
        &Memory::Destruct<T>,
        &DefaultFreeBlock<AllocatorType>
    };

    new (pObj) T(std::forward<Args>(args)...);

    return static_cast<ControlBlock<CountType>*>(pBlock);
}

} // namespace detail

template <class CountType>
class WeakPtrBase;

template <class CountType>
class SharedPtrBase
{
    friend class WeakPtrBase<CountType>;

    template <class T, class OtherCountType>
    friend class SharedPtr;

public:
    using Block = ControlBlock<CountType>;

    SharedPtrBase();

protected:
    SharedPtrBase(const SharedPtrBase& other);
    SharedPtrBase& operator=(const SharedPtrBase& other);
    SharedPtrBase(SharedPtrBase&& other) noexcept;
    SharedPtrBase& operator=(SharedPtrBase&& other) noexcept;

public:
    ~SharedPtrBase();

    bool IsValid() const;
    void* GetVoid() const;
    const TypeInfo& GetTypeInfo() const;
    const TypeId& GetTypeId() const;
    void Reset();

    template <class T, class AllocatorType>
    void Reset(T* ptr);

    AnyRef ToRef() const;
    Block* GetBlock_Internal() const;
    void SetBlock_Internal(Block* block, bool incStrong);

private:
    Block* m_block;
};

template <class CountType>
class WeakPtrBase
{
    friend class SharedPtrBase<CountType>;

public:
    using Block = ControlBlock<CountType>;

    WeakPtrBase();
    WeakPtrBase(const SharedPtrBase<CountType>& other);
    WeakPtrBase(const WeakPtrBase& other);
    WeakPtrBase& operator=(const WeakPtrBase& other);
    WeakPtrBase(WeakPtrBase&& other) noexcept;
    WeakPtrBase& operator=(WeakPtrBase&& other) noexcept;
    ~WeakPtrBase();

    bool IsValid() const;
    void SetBlock_Internal(Block* block, bool incWeak);
    Block* GetBlock_Internal() const;

private:
    Block* m_block;
};

// Template member function implementations that can't be explicitly instantiated
template <class CountType>
template <class T, class AllocatorType>
void SharedPtrBase<CountType>::Reset(T* ptr)
{
    detail::ReleaseStrong(m_block);
    m_block = nullptr;

    if (!ptr)
    {
        return;
    }

    m_block = detail::NewExternalOwnedBlock<CountType, AllocatorType>(ptr);

    if constexpr (std::is_base_of_v<SharedFromThisBase<CountType>, NormalizedType<T>>)
    {
        ptr->Internal_InitializeWeakRef(m_block);
    }
}

// === Strong typed wrappers ===

template <class T, class CountType>
class SharedPtr : public SharedPtrBase<CountType>
{
public:
    using Base = SharedPtrBase<CountType>;
    using Block = typename Base::Block;

    template <class AllocatorType, class... Args>
    static SharedPtr<T, CountType> ConstructWithAllocator(Args&&... args)
    {
        SharedPtr result;

        auto* block = detail::NewInlineBlock<CountType, T, AllocatorType>(std::forward<Args>(args)...);
    
        result.SetBlock_Internal(block, false); 

        if constexpr (std::is_base_of_v<SharedFromThisBase<CountType>, NormalizedType<T>>)
        {
            T* obj = static_cast<T*>(block->pObj);
            obj->Internal_InitializeWeakRef(block);
        }

        return result;
    }
    
    template <class... Args>
    static SharedPtr<T, CountType> Construct(Args&&... args)
    {
        return ConstructWithAllocator<DynamicAllocator>(std::forward<Args>(args)...);
    }

    SharedPtr()
        : Base()
    {
    }

    template <class U, class AllocatorType = DynamicAllocator, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
    explicit SharedPtr(U* p)
        : Base()
    {
        Base::template Reset<U, AllocatorType>(p);
    }

    SharedPtr(std::nullptr_t)
        : Base()
    {
    }

    // delete parent constructors
    SharedPtr(const Base&) = delete;
    SharedPtr& operator=(const Base&) = delete;
    SharedPtr(Base&&) noexcept = delete;
    SharedPtr& operator=(Base&&) noexcept = delete;

    SharedPtr(const SharedPtr& other)
        : Base(static_cast<const Base&>(other))
    {
    }

    SharedPtr& operator=(const SharedPtr& other)
    {
        Base::operator=(static_cast<const Base&>(other));

        return *this;
    }

    SharedPtr(SharedPtr&& other) noexcept
        : Base(static_cast<Base&&>(std::move(other)))
    {
    }

    SharedPtr& operator=(SharedPtr&& other) noexcept
    {
        Base::operator=(static_cast<Base&&>(std::move(other)));

        return *this;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    SharedPtr(const SharedPtr<Ty, CountType>& other)
        : Base(static_cast<const Base&>(other))
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Types not compatible for upcast!");
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    SharedPtr& operator=(const SharedPtr<Ty, CountType>& other)
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Types not compatible for upcast!");

        Base::operator=(static_cast<const Base&>(other));

        return *this;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    SharedPtr(SharedPtr<Ty, CountType>&& other) noexcept
        : Base(static_cast<Base&&>(std::move(other)))
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Types not compatible for upcast!");
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    SharedPtr& operator=(SharedPtr<Ty, CountType>&& other) noexcept
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Types not compatible for upcast!");

        Base::operator=(static_cast<Base&&>(std::move(other)));

        return *this;
    }

    ~SharedPtr() = default;

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

    HYP_FORCE_INLINE bool operator==(const SharedPtr& other) const
    {
        return Get() == other.Get();
    }

    HYP_FORCE_INLINE bool operator==(const WeakPtr<T, CountType>& other) const
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

    HYP_FORCE_INLINE bool operator!=(const SharedPtr& other) const
    {
        return Get() != other.Get();
    }

    HYP_FORCE_INLINE bool operator!=(const WeakPtr<T, CountType>& other) const
    {
        return Get() != other.Get();
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return Get() != nullptr;
    }

    HYP_FORCE_INLINE bool operator!=(const T* ptr) const
    {
        return Get() != ptr;
    }

    HYP_FORCE_INLINE bool operator<(const SharedPtr& other) const
    {
        return Get() < other.Get();
    }

    HYP_FORCE_INLINE bool operator<(const WeakPtr<T, CountType>& other) const
    {
        return Get() < other.Get();
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>> || std::is_same_v<Ty, void>), int> = 0>
    HYP_FORCE_INLINE explicit operator SharedPtr<Ty, CountType>&()
    {
        return reinterpret_cast<SharedPtr<Ty, CountType>&>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>> || std::is_same_v<Ty, void>), int> = 0>
    HYP_FORCE_INLINE explicit operator const SharedPtr<Ty, CountType>&() const
    {
        return reinterpret_cast<const SharedPtr<Ty, CountType>&>(*this);
    }

    template <class... Args>
    HYP_FORCE_INLINE SharedPtr& Emplace(Args&&... args)
    {
        *this = Construct(std::forward<Args>(args)...);
        return *this;
    }

    template <class U, class... Args>
    HYP_FORCE_INLINE SharedPtr& EmplaceAs(Args&&... args)
    {
        static_assert(std::is_convertible_v<U*, T*>);
        *this = SharedPtr<U, CountType>::Construct(std::forward<Args>(args)...);
        return *this;
    }

    HYP_FORCE_INLINE void Reset()
    {
        Base::Reset();
    }

    template <class U, class AllocatorType = DynamicAllocator>
    HYP_FORCE_INLINE void Reset(U* p)
    {
        static_assert(std::is_convertible_v<U*, T*>);
        Base::template Reset<U, AllocatorType>(p);
    }

    template <class U>
    HYP_FORCE_INLINE bool Is() const
    {
        if constexpr (std::is_void_v<U>)
            return true;

        const TypeId typeId = TypeId::ForType<U>();

        const void* ptr = Base::GetVoid();
        const TypeId currentTypeId = Base::GetTypeId();

        return currentTypeId == typeId || IsA(Hyperion::GetClass(typeId), ptr, currentTypeId);
    }

    template <class U>
    HYP_NODISCARD HYP_FORCE_INLINE SharedPtr<U, CountType> Cast() const
    {
        if (Is<U>())
        {
            SharedPtr<U, CountType> result;
            result.Base::SetBlock_Internal(Base::GetBlock_Internal(), true);

            return result;
        }

        return {};
    }

    template <class U>
    HYP_NODISCARD HYP_FORCE_INLINE SharedPtr<U, CountType> CastUnchecked() const
    {
        SharedPtr<U, CountType> result;
        result.Base::SetBlock_Internal(Base::GetBlock_Internal(), true);

        return result;
    }

    HYP_NODISCARD HYP_FORCE_INLINE WeakPtr<T, CountType> ToWeak() const
    {
        return WeakPtr<T, CountType>(*this);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(Get());
    }
};

template <class CountType>
class SharedPtr<void, CountType> : public SharedPtrBase<CountType>
{
public:
    using Base = SharedPtrBase<CountType>;

    SharedPtr()
        : Base()
    {
    }

    SharedPtr(std::nullptr_t)
        : Base()
    {
    }

    // delete parent constructors
    SharedPtr(const Base&) = delete;
    SharedPtr& operator=(const Base&) = delete;
    SharedPtr(Base&&) noexcept = delete;
    SharedPtr& operator=(Base&&) noexcept = delete;

    template <class Ty>
    SharedPtr(const SharedPtr<Ty, CountType>& other)
        : Base(static_cast<const Base&>(other))
    {
    }

    template <class Ty>
    SharedPtr& operator=(const SharedPtr<Ty, CountType>& other)
    {
        Base::operator=(static_cast<const Base&>(other));

        return *this;
    }

    template <class Ty>
    SharedPtr(SharedPtr<Ty, CountType>&& other) noexcept
        : Base(std::move(other))
    {
    }

    template <class Ty>
    SharedPtr& operator=(SharedPtr<Ty, CountType>&& other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~SharedPtr() = default;

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

    HYP_FORCE_INLINE bool operator==(const SharedPtr& other) const
    {
        return Base::GetVoid() == other.GetVoid();
    }

    HYP_FORCE_INLINE bool operator==(const WeakPtr<void, CountType>& other) const
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

    HYP_FORCE_INLINE bool operator!=(const SharedPtr& other) const
    {
        return Base::GetVoid() != other.GetVoid();
    }

    HYP_FORCE_INLINE bool operator!=(const WeakPtr<void, CountType>& other) const
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

    HYP_FORCE_INLINE bool operator<(const SharedPtr& other) const
    {
        return Base::GetVoid() < other.GetVoid();
    }

    HYP_FORCE_INLINE bool operator<(const WeakPtr<void, CountType>& other) const
    {
        return Base::GetVoid() < other.GetVoid();
    }

    template <class U>
    HYP_FORCE_INLINE bool Is() const
    {
        const TypeId typeId = TypeId::ForType<U>();

        const void* ptr = Base::GetVoid();
        const TypeId currentTypeId = Base::GetTypeId();

        return std::is_same_v<U, void> || currentTypeId == typeId || IsA(Hyperion::GetClass(typeId), ptr, currentTypeId);
    }

    template <class U>
    HYP_NODISCARD HYP_FORCE_INLINE SharedPtr<U, CountType> Cast() const
    {
        if (Is<U>())
        {
            SharedPtr<U, CountType> result;
            result.SharedPtrBase<CountType>::SetBlock_Internal(Base::GetBlock_Internal(), true);

            return result;
        }

        return {};
    }

    template <class U>
    HYP_NODISCARD HYP_FORCE_INLINE SharedPtr<U, CountType> CastUnchecked() const
    {
        SharedPtr<U, CountType> result;
        result.SharedPtrBase<CountType>::SetBlock_Internal(Base::GetBlock_Internal(), true);

        return result;
    }

    template <class U, class AllocatorType = DynamicAllocator>
    HYP_FORCE_INLINE void Reset(U* p)
    {
        Base::template Reset<U, AllocatorType>(p);
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
class WeakPtr : public WeakPtrBase<CountType>
{
protected:
    using Base = WeakPtrBase<CountType>;
    using Block = typename Base::Block;

public:
    WeakPtr()
        : Base()
    {
    }

    WeakPtr(const WeakPtr& other)
        : Base(other)
    {
    }

    WeakPtr& operator=(const WeakPtr& other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakPtr(const SharedPtr<T, CountType>& other)
        : Base(other)
    {
    }

    WeakPtr& operator=(const SharedPtr<T, CountType>& other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakPtr(SharedPtr<T, CountType>&& other) noexcept = delete;
    WeakPtr& operator=(SharedPtr<T, CountType>&& other) noexcept = delete;

    WeakPtr(WeakPtr&& other) noexcept
        : Base(std::move(other))
    {
    }

    WeakPtr& operator=(WeakPtr&& other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~WeakPtr() = default;

    HYP_FORCE_INLINE T* GetUnsafe() const
    {
        Block* block = Base::GetBlock_Internal();

        return block ? reinterpret_cast<T*>(block->pObj) : nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const WeakPtr& other) const
    {
        return GetUnsafe() == other.GetUnsafe();
    }

    HYP_FORCE_INLINE bool operator==(const SharedPtr<T, CountType>& other) const
    {
        return GetUnsafe() == other.Get();
    }

    HYP_FORCE_INLINE bool operator!=(const WeakPtr& other) const
    {
        return GetUnsafe() != other.GetUnsafe();
    }

    HYP_FORCE_INLINE bool operator!=(const SharedPtr<T, CountType>& other) const
    {
        return GetUnsafe() != other.Get();
    }

    HYP_FORCE_INLINE bool operator<(const SharedPtr<T, CountType>& other) const
    {
        return GetUnsafe() < other.Get();
    }

    HYP_FORCE_INLINE bool operator<(const WeakPtr& other) const
    {
        return GetUnsafe() < other.GetUnsafe();
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>> || std::is_same_v<Ty, void>), int> = 0>
    HYP_FORCE_INLINE explicit operator WeakPtr<Ty, CountType>&()
    {
        return reinterpret_cast<WeakPtr<Ty, CountType>&>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>> || std::is_same_v<Ty, void>), int> = 0>
    HYP_FORCE_INLINE explicit operator const WeakPtr<Ty, CountType>&() const
    {
        return reinterpret_cast<const WeakPtr<Ty, CountType>&>(*this);
    }

    HYP_NODISCARD SharedPtr<T, CountType> Lock() const
    {
        SharedPtr<T, CountType> result;

        auto* block = Base::GetBlock_Internal();

        if (!block)
        {
            return result;
        }

        if constexpr (std::is_integral_v<CountType>) // not atomic
        {
            if (block->strong > 0)
            {
                result.SharedPtrBase<CountType>::SetBlock_Internal(block, true);
            }
        }
        else
        {
            uint32 expected = block->strong.Get(MemoryOrder::ACQUIRE);
        
            while (expected > 0)
            {
                if (block->strong.CompareExchangeWeak(expected, expected + 1, MemoryOrder::ACQUIRE_RELEASE))
                {
                    result.SharedPtrBase<CountType>::SetBlock_Internal(block, false);
                    break;
                }
            }
        }

        return result;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(GetUnsafe());
    }
};

// WeakPtr<void> specialization
template <class CountType>
class WeakPtr<void, CountType> : public WeakPtrBase<CountType>
{
protected:
    using Base = WeakPtrBase<CountType>;

public:
    WeakPtr()
        : Base()
    {
    }

    WeakPtr(const WeakPtr& other)
        : Base(other)
    {
    }

    WeakPtr& operator=(const WeakPtr& other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakPtr(const SharedPtr<void, CountType>& other)
        : Base(other)
    {
    }

    WeakPtr& operator=(const SharedPtr<void, CountType>& other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakPtr(SharedPtr<void, CountType>&& other) noexcept = delete;
    WeakPtr& operator=(SharedPtr<void, CountType>&& other) noexcept = delete;

    WeakPtr(WeakPtr&& other) noexcept
        : Base(std::move(other))
    {
    }

    WeakPtr& operator=(WeakPtr&& other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~WeakPtr() = default;

    HYP_FORCE_INLINE void* GetUnsafe() const
    {
        auto* block = Base::GetBlock_Internal();

        return block ? block->pObj : nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const WeakPtr& other) const
    {
        return GetUnsafe() == other.GetUnsafe();
    }

    HYP_FORCE_INLINE bool operator==(const SharedPtr<void, CountType>& other) const
    {
        return GetUnsafe() == other.Get();
    }

    HYP_FORCE_INLINE bool operator!=(const WeakPtr& other) const
    {
        return GetUnsafe() != other.GetUnsafe();
    }

    HYP_FORCE_INLINE bool operator!=(const SharedPtr<void, CountType>& other) const
    {
        return GetUnsafe() != other.Get();
    }

    HYP_FORCE_INLINE bool operator<(const SharedPtr<void, CountType>& other) const
    {
        return GetUnsafe() < other.Get();
    }

    HYP_FORCE_INLINE bool operator<(const WeakPtr& other) const
    {
        return GetUnsafe() < other.GetUnsafe();
    }

    template <class Ty>
    HYP_NODISCARD HYP_FORCE_INLINE WeakPtr<Ty, CountType> CastUnchecked() const
    {
        WeakPtr<Ty, CountType> weak;
        weak.SetBlock_Internal(Base::GetBlock_Internal(), true);

        return weak;
    }

    HYP_NODISCARD SharedPtr<void, CountType> Lock() const
    {
        SharedPtr<void, CountType> result;

        auto* block = Base::GetBlock_Internal();

        if (!block)
        {
            return result;
        }

        if constexpr (std::is_integral_v<CountType>) // not atomic
        {
            if (block->strong > 0)
            {
                result.SharedPtrBase<CountType>::SetBlock_Internal(block, true);
            }
        }
        else
        {
            uint32 expected = block->strong.Get(MemoryOrder::ACQUIRE);
        
            while (expected > 0)
            {
                if (block->strong.CompareExchangeWeak(expected, expected + 1, MemoryOrder::ACQUIRE_RELEASE))
                {
                    result.SharedPtrBase<CountType>::SetBlock_Internal(block, false);
                    break;
                }
            }
        }

        return result;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(GetUnsafe());
    }
};

template <class T, class CountType>
class SharedFromThis;

template <class CountType>
class SharedFromThisBase
{
public:
    // Called internally by SharedPtr when it assumes ownership.
    void Internal_InitializeWeakRef(ControlBlock<CountType>* block) const
    {
        if (!m_weakBlock)
        {
            m_weakBlock = block;
            detail::IncWeak(m_weakBlock);
        }
    }

protected:
    SharedFromThisBase() noexcept : m_weakBlock(nullptr) {}

    SharedFromThisBase(const SharedFromThisBase&) noexcept : m_weakBlock(nullptr) {}
    SharedFromThisBase& operator=(const SharedFromThisBase&) noexcept { return *this; }
    
    virtual ~SharedFromThisBase()
    {
        detail::ReleaseWeak(m_weakBlock);
    }

    mutable ControlBlock<CountType>* m_weakBlock;
};

template <class T, class CountType>
class SharedFromThis : public SharedFromThisBase<CountType>
{
    using Base = SharedFromThisBase<CountType>;

public:
    SharedFromThis() = default;

    WeakPtr<T, CountType> WeakThis() const
    {
        WeakPtr<T, CountType> result;
        if (Base::m_weakBlock)
        {
            result.SetBlock_Internal(Base::m_weakBlock, /* incWeak */ true);
        }
        return result;
    }

    SharedPtr<T, CountType> SharedThis() const
    {
        return WeakThis().Lock();
    }
};

template <class T, class CountType = AtomicVar<uint32>, class AllocatorType = DynamicAllocator>
struct MakeSharedHelper
{
    template <class... Args>
    static SharedPtr<T, CountType> MakeShared(Args&&... args)
    {
        return SharedPtr<T, CountType>::template ConstructWithAllocator<AllocatorType>(std::forward<Args>(args)...);
    }
};

} // namespace memory

using memory::SharedPtr;
using memory::WeakPtr;

using memory::SharedFromThisBase;
using memory::SharedFromThis;

template <class T, class... Args>
HYP_NODISCARD HYP_FORCE_INLINE SharedPtr<T> MakeShared(Args&&... args)
{
    return memory::MakeSharedHelper<T, AtomicVar<uint32>>::MakeShared(std::forward<Args>(args)...);
}

template <class T, class AllocatorType, class... Args>
HYP_NODISCARD HYP_FORCE_INLINE SharedPtr<T> MakeSharedWithAllocator(Args&&... args)
{
    return memory::MakeSharedHelper<T, AtomicVar<uint32>, AllocatorType>::MakeShared(std::forward<Args>(args)...);
}

} // namespace Hyperion
