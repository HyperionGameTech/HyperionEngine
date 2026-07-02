/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Reflection/BoxedValue.hpp>

#include <Core/Reflection/TypeInfo.hpp>

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
CORE_API uint32 IncWeak(ControlBlock<CountType>* block)
{
    if (block)
    {
        return Inc(block->weak);
    }

    return 0;
}

template <class CountType>
CORE_API uint32 ReleaseWeak(ControlBlock<CountType>* block)
{
    if (block)
    {
        return DecWeakAndMaybeFree(block);
    }

    return 0;
}

template <class CountType>
CORE_API uint32 IncStrong(ControlBlock<CountType>* block)
{
    if (block)
    {
        return Inc(block->strong);
    }

    return 0;
}

template <class CountType>
CORE_API uint32 ReleaseStrong(ControlBlock<CountType>* block)
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

CORE_API void DefaultFreeBlock(void* blk)
{
    Memory::FreeAligned(blk);
}

CORE_API void ExternalBlockDeleter(void* blk)
{
    ControlBlock<AtomicVar<uint32>>* block = reinterpret_cast<ControlBlock<AtomicVar<uint32>>*>(blk); // CountType not used here

    if (block->pObj && block->pFnDestructObj)
    {
        block->pFnDestructObj(block->pObj);
    }

    Memory::FreeAligned(block);
}

// instantiations
template CORE_API uint32 IncStrong<uint32>(ControlBlock<uint32>*);
template CORE_API uint32 IncStrong<AtomicVar<uint32>>(ControlBlock<AtomicVar<uint32>>*);

template CORE_API uint32 ReleaseStrong<uint32>(ControlBlock<uint32>*);
template CORE_API uint32 ReleaseStrong<AtomicVar<uint32>>(ControlBlock<AtomicVar<uint32>>*);

template CORE_API uint32 IncWeak<uint32>(ControlBlock<uint32>*);
template CORE_API uint32 IncWeak<AtomicVar<uint32>>(ControlBlock<AtomicVar<uint32>>*);

template CORE_API uint32 ReleaseWeak<uint32>(ControlBlock<uint32>*);
template CORE_API uint32 ReleaseWeak<AtomicVar<uint32>>(ControlBlock<AtomicVar<uint32>>*);
} // namespace detail

// SharedPtrBase implementations

template <class CountType>
SharedPtrBase<CountType>::SharedPtrBase()
    : m_block(nullptr)
{
}

template <class CountType>
SharedPtrBase<CountType>::SharedPtrBase(const SharedPtrBase& other)
    : m_block(other.m_block)
{
    detail::IncStrong(m_block);
}

template <class CountType>
SharedPtrBase<CountType>& SharedPtrBase<CountType>::operator=(const SharedPtrBase& other)
{
    if (this == &other || m_block == other.m_block)
        return *this;

    detail::ReleaseStrong(m_block);

    m_block = other.m_block;

    detail::IncStrong(m_block);

    return *this;
}

template <class CountType>
SharedPtrBase<CountType>::SharedPtrBase(SharedPtrBase&& other) noexcept
    : m_block(other.m_block)
{
    other.m_block = nullptr;
}

template <class CountType>
SharedPtrBase<CountType>& SharedPtrBase<CountType>::operator=(SharedPtrBase&& other) noexcept
{
    if (this == &other || m_block == other.m_block)
        return *this;

    detail::ReleaseStrong(m_block);

    m_block = other.m_block;
    other.m_block = nullptr;

    return *this;
}

template <class CountType>
SharedPtrBase<CountType>::~SharedPtrBase()
{
    detail::ReleaseStrong(m_block);
}

template <class CountType>
bool SharedPtrBase<CountType>::IsValid() const
{
    return m_block != nullptr;
}

template <class CountType>
void* SharedPtrBase<CountType>::GetVoid() const
{
    return m_block ? m_block->pObj : nullptr;
}

template <class CountType>
const TypeInfo& SharedPtrBase<CountType>::GetTypeInfo() const
{
    return m_block ? *m_block->typeInfo : TypeInfo_Void();
}

template <class CountType>
const TypeId& SharedPtrBase<CountType>::GetTypeId() const
{
    return TypeInfo_GetId(GetTypeInfo());
}

template <class CountType>
void SharedPtrBase<CountType>::Reset()
{
    detail::ReleaseStrong(m_block);

    m_block = nullptr;
}

template <class CountType>
AnyRef SharedPtrBase<CountType>::ToRef() const
{
    return AnyRef(&GetTypeInfo(), GetVoid());
}

template <class CountType>
typename SharedPtrBase<CountType>::Block* SharedPtrBase<CountType>::GetBlock_Internal() const
{
    return m_block;
}

template <class CountType>
void SharedPtrBase<CountType>::SetBlock_Internal(Block* block, bool incStrong)
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

// WeakPtrBase implementations

template <class CountType>
WeakPtrBase<CountType>::WeakPtrBase()
    : m_block(nullptr)
{
}

template <class CountType>
WeakPtrBase<CountType>::WeakPtrBase(const SharedPtrBase<CountType>& other)
    : m_block(other.m_block)
{
    detail::IncWeak(m_block);
}

template <class CountType>
WeakPtrBase<CountType>::WeakPtrBase(const WeakPtrBase& other)
    : m_block(other.m_block)
{
    detail::IncWeak(m_block);
}

template <class CountType>
WeakPtrBase<CountType>& WeakPtrBase<CountType>::operator=(const WeakPtrBase& other)
{
    if (this == &other || m_block == other.m_block)
        return *this;

    detail::ReleaseWeak(m_block);

    m_block = other.m_block;

    detail::IncWeak(m_block);

    return *this;
}

template <class CountType>
WeakPtrBase<CountType>::WeakPtrBase(WeakPtrBase&& other) noexcept
    : m_block(other.m_block)
{
    other.m_block = nullptr;
}

template <class CountType>
WeakPtrBase<CountType>& WeakPtrBase<CountType>::operator=(WeakPtrBase&& other) noexcept
{
    if (this == &other || m_block == other.m_block)
        return *this;

    detail::ReleaseWeak(m_block);

    m_block = other.m_block;
    other.m_block = nullptr;

    return *this;
}

template <class CountType>
WeakPtrBase<CountType>::~WeakPtrBase()
{
    detail::ReleaseWeak(m_block);
}

template <class CountType>
bool WeakPtrBase<CountType>::IsValid() const
{
    return m_block != nullptr;
}

template <class CountType>
void WeakPtrBase<CountType>::SetBlock_Internal(Block* block, bool incWeak)
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
typename WeakPtrBase<CountType>::Block* WeakPtrBase<CountType>::GetBlock_Internal() const
{
    return m_block;
}

// Explicit instantiations
template class CORE_API SharedPtrBase<uint32>;
template class CORE_API SharedPtrBase<AtomicVar<uint32>>;

template class CORE_API WeakPtrBase<uint32>;
template class CORE_API WeakPtrBase<AtomicVar<uint32>>;

} // namespace memory
} // namespace Hyperion
