#include "FxAnonArray.hpp"

#include <Core/MemPool/FxMemPool.hpp>
#include <FxEngine.hpp>

void FxAnonArray::Create(uint32 object_size, uint32 size)
{
    ObjectSize = object_size;
    Capacity = size;
    Size = 0;

    // pData = FxMemPool::AllocRaw(object_size * size);
    pData = gEnginePool->AllocRaw(object_size * size);
}

FxAnonArray::FxAnonArray(FxAnonArray&& other) { (*this) = std::move(other); }

FxAnonArray& FxAnonArray::operator=(FxAnonArray&& other) noexcept
{
    pData = other.pData;
    Size = other.Size;
    Capacity = other.Capacity;
    ObjectSize = other.ObjectSize;

    other.pData = nullptr;
    other.Size = 0;
    other.Capacity = 0;
    other.ObjectSize = 0;

    return *this;
}

void FxAnonArray::Free()
{
    if (pData == nullptr) {
        return;
    }

    // FxMemPool::Free(pData);
    gEnginePool->FreeRaw(pData);

    Capacity = 0;
    Size = 0;
    ObjectSize = 0;
    pData = nullptr;
}

void FxAnonArray::InsertRaw(const void* value)
{
    memcpy(reinterpret_cast<uint8*>(pData) + (ObjectSize * Size), value, ObjectSize);
    ++Size;
}

const void* FxAnonArray::GetRaw(uint32 index) const
{
    FxAssert(index < Size);
    FxAssert(ObjectSize > 0);

    return reinterpret_cast<void*>(reinterpret_cast<uint8*>(pData) + (index * ObjectSize));
}

void* FxAnonArray::GetRaw(uint32 index)
{
    FxAssert(index < Size);
    FxAssert(ObjectSize > 0);

    return reinterpret_cast<void*>(reinterpret_cast<uint8*>(pData) + (index * ObjectSize));
}
