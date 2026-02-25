/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/memory/RefCountedPtr.hpp>

#include <Core/reflection/BoxedValue.hpp>

#include <Core/reflection/TypeInfo.hpp>

namespace Hyperion {
namespace memory {

namespace detail {

template <class CountType>
static inline uint32 Inc(CountType& count)
{
    if constexpr (std::is_integral_v<CountType>)
        return ++count;
    else
        return count.Increment(1, MemoryOrder::ACQUIRE_RELEASE) + 1;
}

template <class CountType>
static inline uint32 Dec(CountType& count)
{
    if constexpr (std::is_integral_v<CountType>)
        return --count;
    else
        return count.Decrement(1, MemoryOrder::ACQUIRE_RELEASE) - 1;
}

template <class CountType>
static inline uint32 DecWeakAndMaybeFree(ControlBlock<CountType>* block)
{
    uint32 count;

    if ((count = Dec(block->weak)) == 0 && block->pFnFreeBlock)
    {
        block->pFnFreeBlock(block);
    }

    return count;
}

template <class CountType>
HYP_API uint32 IncWeak(ControlBlock<CountType>* block)
{
    if (block)
    {
        return Inc(block->weak);
    }

    return 0;
}

template <class CountType>
HYP_API uint32 ReleaseWeak(ControlBlock<CountType>* block)
{
    if (block)
    {
        return DecWeakAndMaybeFree(block);
    }

    return 0;
}

template <class CountType>
HYP_API uint32 IncStrong(ControlBlock<CountType>* block)
{
    if (block)
    {
        return Inc(block->strong);
    }

    return 0;
}

template <class CountType>
HYP_API uint32 ReleaseStrong(ControlBlock<CountType>* block)
{
    if (!block)
    {
        return 0;
    }

    uint32 count;

    if ((count = Dec(block->strong)) == 0)
    {
        if (block->pFnDestructObj && block->pObj)
        {
            block->pFnDestructObj(block->pObj);
        }

        block->pObj = nullptr;

        DecWeakAndMaybeFree(block);
    }

    return count;
}

HYP_API void DefaultFreeBlock(void* blk)
{
    HYP_FREE_ALIGNED(blk);
}

HYP_API void ExternalBlockDeleter(void* blk)
{
    ControlBlock<AtomicVar<uint32>>* block = reinterpret_cast<ControlBlock<AtomicVar<uint32>>*>(blk); // CountType not used here

    if (block->pObj && block->pFnDestructObj)
    {
        block->pFnDestructObj(block->pObj);
    }

    HYP_FREE_ALIGNED(block);
}

// instantiations
template HYP_API uint32 IncStrong<uint32>(ControlBlock<uint32>*);
template HYP_API uint32 IncStrong<AtomicVar<uint32>>(ControlBlock<AtomicVar<uint32>>*);

template HYP_API uint32 ReleaseStrong<uint32>(ControlBlock<uint32>*);
template HYP_API uint32 ReleaseStrong<AtomicVar<uint32>>(ControlBlock<AtomicVar<uint32>>*);

template HYP_API uint32 IncWeak<uint32>(ControlBlock<uint32>*);
template HYP_API uint32 IncWeak<AtomicVar<uint32>>(ControlBlock<AtomicVar<uint32>>*);

template HYP_API uint32 ReleaseWeak<uint32>(ControlBlock<uint32>*);
template HYP_API uint32 ReleaseWeak<AtomicVar<uint32>>(ControlBlock<AtomicVar<uint32>>*);
} // namespace detail

// RefCountedPtrBase implementations

template <class CountType>
RefCountedPtrBase<CountType>::RefCountedPtrBase()
    : m_block(nullptr)
{
}

template <class CountType>
RefCountedPtrBase<CountType>::RefCountedPtrBase(const RefCountedPtrBase& other)
    : m_block(other.m_block)
{
    detail::IncStrong(m_block);
}

template <class CountType>
RefCountedPtrBase<CountType>& RefCountedPtrBase<CountType>::operator=(const RefCountedPtrBase& other)
{
    if (this == &other || m_block == other.m_block)
        return *this;

    detail::ReleaseStrong(m_block);

    m_block = other.m_block;

    detail::IncStrong(m_block);

    return *this;
}

template <class CountType>
RefCountedPtrBase<CountType>::RefCountedPtrBase(RefCountedPtrBase&& other) noexcept
    : m_block(other.m_block)
{
    other.m_block = nullptr;
}

template <class CountType>
RefCountedPtrBase<CountType>& RefCountedPtrBase<CountType>::operator=(RefCountedPtrBase&& other) noexcept
{
    if (this == &other || m_block == other.m_block)
        return *this;

    detail::ReleaseStrong(m_block);

    m_block = other.m_block;
    other.m_block = nullptr;

    return *this;
}

template <class CountType>
RefCountedPtrBase<CountType>::~RefCountedPtrBase()
{
    detail::ReleaseStrong(m_block);
}

template <class CountType>
bool RefCountedPtrBase<CountType>::IsValid() const
{
    return m_block != nullptr;
}

template <class CountType>
void* RefCountedPtrBase<CountType>::GetVoid() const
{
    return m_block ? m_block->pObj : nullptr;
}

template <class CountType>
const TypeInfo& RefCountedPtrBase<CountType>::GetTypeInfo() const
{
    return m_block ? *m_block->typeInfo : TypeInfo_Void();
}

template <class CountType>
const TypeId& RefCountedPtrBase<CountType>::GetTypeId() const
{
    return TypeInfo_GetId(GetTypeInfo());
}

template <class CountType>
void RefCountedPtrBase<CountType>::Reset()
{
    detail::ReleaseStrong(m_block);

    m_block = nullptr;
}

template <class CountType>
AnyRef RefCountedPtrBase<CountType>::ToRef() const
{
    return AnyRef(&GetTypeInfo(), GetVoid());
}

template <class CountType>
typename RefCountedPtrBase<CountType>::Block* RefCountedPtrBase<CountType>::GetBlock_Internal() const
{
    return m_block;
}

template <class CountType>
void RefCountedPtrBase<CountType>::SetBlock_Internal(Block* block, bool incStrong)
{
    if (m_block == block)
    {
        return;
    }

    detail::ReleaseStrong(m_block);

    m_block = block;

    if (incStrong)
    {
        detail::IncStrong(m_block);
    }
}

// WeakRefCountedPtrBase implementations

template <class CountType>
WeakRefCountedPtrBase<CountType>::WeakRefCountedPtrBase()
    : m_block(nullptr)
{
}

template <class CountType>
WeakRefCountedPtrBase<CountType>::WeakRefCountedPtrBase(const RefCountedPtrBase<CountType>& other)
    : m_block(other.m_block)
{
    detail::IncWeak(m_block);
}

template <class CountType>
WeakRefCountedPtrBase<CountType>::WeakRefCountedPtrBase(const WeakRefCountedPtrBase& other)
    : m_block(other.m_block)
{
    detail::IncWeak(m_block);
}

template <class CountType>
WeakRefCountedPtrBase<CountType>& WeakRefCountedPtrBase<CountType>::operator=(const WeakRefCountedPtrBase& other)
{
    if (this == &other || m_block == other.m_block)
        return *this;

    detail::ReleaseWeak(m_block);

    m_block = other.m_block;

    detail::IncWeak(m_block);

    return *this;
}

template <class CountType>
WeakRefCountedPtrBase<CountType>::WeakRefCountedPtrBase(WeakRefCountedPtrBase&& other) noexcept
    : m_block(other.m_block)
{
    other.m_block = nullptr;
}

template <class CountType>
WeakRefCountedPtrBase<CountType>& WeakRefCountedPtrBase<CountType>::operator=(WeakRefCountedPtrBase&& other) noexcept
{
    if (this == &other || m_block == other.m_block)
        return *this;

    detail::ReleaseWeak(m_block);

    m_block = other.m_block;
    other.m_block = nullptr;

    return *this;
}

template <class CountType>
WeakRefCountedPtrBase<CountType>::~WeakRefCountedPtrBase()
{
    detail::ReleaseWeak(m_block);
}

template <class CountType>
bool WeakRefCountedPtrBase<CountType>::IsValid() const
{
    return m_block != nullptr;
}

template <class CountType>
void WeakRefCountedPtrBase<CountType>::SetBlock_Internal(Block* block, bool incWeak)
{
    if (m_block == block)
    {
        return;
    }

    detail::ReleaseWeak(m_block);

    m_block = block;

    if (incWeak)
    {
        detail::IncWeak(m_block);
    }
}

template <class CountType>
typename WeakRefCountedPtrBase<CountType>::Block* WeakRefCountedPtrBase<CountType>::GetBlock_Internal() const
{
    return m_block;
}

// Explicit instantiations
template class HYP_API RefCountedPtrBase<uint32>;
template class HYP_API RefCountedPtrBase<AtomicVar<uint32>>;

template class HYP_API WeakRefCountedPtrBase<uint32>;
template class HYP_API WeakRefCountedPtrBase<AtomicVar<uint32>>;

} // namespace memory
} // namespace Hyperion