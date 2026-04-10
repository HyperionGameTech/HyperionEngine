#pragma once

// #include "FxAllocator.hpp"
#include "FxDefines.hpp"
#include "FxPanic.hpp"

#include <stddef.h>
#include <stdio.h>

#include <Core/MemPool/FxMemPool.hpp>
#include <FxEngine.hpp>
#include <cstdlib>
#include <initializer_list>

#ifdef FX_SIZED_ARRAY_DEBUG
#include "FxLog.hpp"
#endif

//#define FX_SIZED_ARRAY_NO_MEMPOOL 1

static inline void NoMemError()
{
    puts("FxSizedArray: out of memory");
    FxTerminate();
}

template <typename TElementType>
class FxSizedArray
{
public:
    static constexpr int64 scItemNotFound = -1;

    using SizeType = size_t;

public:
    struct Iterator
    {
        Iterator(TElementType* ptr, size_t index) : mPtr(ptr), mIndex(index) {}

        Iterator& operator++()
        {
            mIndex++;
            return *this;
        }

        Iterator& operator++(int value)
        {
            Iterator before = *this;
            mIndex++;
            return before;
        }

        TElementType& operator*() const { return mPtr[mIndex]; }

        bool operator==(const Iterator& b) const { return mPtr == b.mPtr && mIndex == b.mIndex; }


    private:
        TElementType* mPtr;
        size_t mIndex;
    };


public:
    static FxSizedArray<TElementType> CreateCopyOf(const TElementType* ptr, SizeType size)
    {
        FxSizedArray<TElementType> arr(size);
        arr.MarkFull();

        memcpy(arr.pData, ptr, arr.GetSizeInBytes());

        return std::move(arr);
    }

    static FxSizedArray<TElementType> CreateAsSize(SizeType size)
    {
        FxSizedArray<TElementType> arr;
        arr.InitSize(size);

        return std::move(arr);
    }

    static FxSizedArray<TElementType> CreateEmpty() { return FxSizedArray<TElementType>(nullptr, 0); }

public:
    FxSizedArray(TElementType* ptr, size_t size) : pData(ptr), Size(size), Capacity(size) {}

    FxSizedArray(size_t element_count) : Capacity(element_count)
    {
#ifdef FX_SIZED_ARRAY_DEBUG
        FxLogDebug("Allocating FxSizedArray of capacity {:d} (type: {:s})", Capacity, typeid(TElementType).name());
#endif
        InternalAllocateArray(element_count);
        Size = 0;
    }

    FxSizedArray(std::initializer_list<TElementType> list)
    {
        if (list.size() == 0) {
            return;
        }

        InitCapacity(list.size());

        for (const TElementType& obj : list) {
            Insert(obj);
        }
    };

    FxSizedArray(FxSizedArray<TElementType>&& other)
    {
        pData = std::move(other.pData);
        other.pData = nullptr;

        Capacity = other.Capacity;
        Size = other.Size;

        other.Size = 0;
        other.Capacity = 0;
    }

    FxSizedArray() = default;

    ~FxSizedArray() { FxSizedArray::Free(); }


    void Free()
    {
        if (pData == nullptr || DoNotDestroy) {
            return;
        }

#if !defined(FX_SIZED_ARRAY_NO_MEMPOOL)
        for (size_t i = 0; i < Size; i++) {
            TElementType& element = pData[i];
            element.~TElementType();
        }

        if (gEnginePool) {
            gEnginePool->FreeRaw(static_cast<void*>(pData));
        }
#else
        std::free(reinterpret_cast<void*>(pData));
#endif

#ifdef FX_SIZED_ARRAY_DEBUG
        FxLogDebug("Freeing FxSizedArray of size {:d} (type: {:s})", Size, typeid(TElementType).name());
#endif

        pData = nullptr;
        Capacity = 0;
        Size = 0;
    }

    Iterator begin() const { return Iterator(pData, 0); }

    Iterator end() const { return Iterator(pData, Size); }

    operator TElementType*() const { return pData; }

    operator TElementType&() const { return *pData; }

    const TElementType& operator[](size_t index) const
    {
        FxAssertMsg(index < Capacity, "Access out of range");
        return pData[index];
    }

    TElementType& operator[](size_t index)
    {
        FxAssertMsg(index < Capacity, "Access out of range");
        return pData[index];
    }

    FxSizedArray<TElementType>& operator=(std::initializer_list<TElementType>& list)
    {
        const size_t list_size = list.size();
        if (list_size > Capacity) {
            Free();
            InitCapacity(list.size());
        }

        Clear();

        for (const TElementType& obj : list) {
            Insert(obj);
        }

        return *this;
    }

    FxSizedArray<TElementType>& operator=(FxSizedArray<TElementType>&& other)
    {
        if (pData) {
            Free();
        }

        pData = other.pData;
        other.pData = nullptr;

        Size = other.Size;
        Capacity = other.Capacity;

        other.Size = 0;
        other.Capacity = 0;

        return *this;
    }

    template <typename T>
        requires C_IsSameOrConst<T, TElementType>
    void InitAsCopyOf(const FxSizedArray<T>& other)
    {
        InitCapacity(other.Capacity);
        Size = other.Size;

        if (std::is_copy_constructible_v<TElementType>) {
            for (SizeType i = 0; i < Size; i++) {
                new (other.pData + i) TElementType(other.pData[i]);
            }
        }
        else {
            std::memcpy(reinterpret_cast<void*>(pData), reinterpret_cast<const void*>(other.pData),
                        other.GetSizeInBytes());
        }
    }

    template <typename T>
        requires std::is_same_v<typename std::remove_const<T>::type, TElementType>
    void InitAsCopyOf(const FxSizedArray<T>& other, SizeType copy_capacity)
    {
        InitCapacity(max(other.Capacity, copy_capacity));
        Size = other.Size;

        memcpy(pData, other.pData, other.GetSizeInBytes());
    }

    void InitAsCopyOf(const TElementType* ptr, SizeType size)
    {
        InitSize(size);
        memcpy(pData, ptr, GetSizeInBytes());
    }

    void CloneFrom(const FxSizedArray<TElementType>& other)
    {
        InitCapacity(other.Capacity);
        Size = other.Size;
        memcpy(pData, other.pData, other.GetSizeInBytes());
    }

    void Clear()
    {
        for (size_t i = 0; i < Size; i++) {
            TElementType& element = pData[i];
            element.~TElementType();
        }

        Size = 0;
    }

    FX_FORCE_INLINE bool IsEmpty() const { return Size == 0; }
    FX_FORCE_INLINE bool IsNotEmpty() const { return !IsEmpty(); }

    TElementType& Insert(const TElementType& object)
    {
        FxAssertMsg(Size < Capacity, "Insert will exceed capacity!");

        TElementType* element = &pData[Size++];
        new (element) TElementType(object);

        return *element;
    }

    void Insert(TElementType&& object)
    {
        FxAssertMsg(Size < Capacity, "Insert will exceed capacity!");

        TElementType* element = &pData[Size++];

        new (element) TElementType(std::move(object));
    }

    /** Inserts a new empty element into the array and returns a pointer to the element */
    TElementType* Insert()
    {
        FxAssertMsg(Size < Capacity, "Insert will exceed capacity!");

        TElementType* element = &pData[Size++];

        new (element) TElementType;

        return element;
    }

    void RemoveLast()
    {
        FxAssertMsg(Size > 0, "No elements remaining!");
        TElementType* element = &pData[Size--];

        if (std::is_destructible_v<TElementType>) {
            element->~TElementType();
        }
    }

    void InitCapacity(size_t element_count)
    {
        FxAssertMsg(pData == nullptr, "SizedArray has already been initialized!");

        InternalAllocateArray(element_count);

        Capacity = element_count;
    }

    void MarkFull() { Size = Capacity; }

    bool ContainsItem(TElementType* ptr) const
    {
        const TElementType* end_ptr = (pData + Size);

        if (ptr >= pData && ptr <= end_ptr) {
            return true;
        }

        return false;
    }

    int64 GetItemIndex(TElementType* ptr) const
    {
        const TElementType* end_ptr = (pData + Size);

        if (ptr < pData || ptr > end_ptr) {
            return scItemNotFound;
        }

        FxAssert(ptr >= pData);

        return ptr - pData;
    }

    /**
     *   Initializes an array to contain `element_count` elements, which can be modified externally.
     *
     *   Example:
     *   ```cpp
     *      FxSizedArray<int32> int_array(10);
     *      int_array.InitSize();
     *
     *      FxSizedArray<int32> other_array;
     *      other_array.InitSize(15);
     *   ```
     */
    void InitSize(size_t element_count)
    {
        if (Capacity == 0) {
            InitCapacity(element_count);
        }

        Size = Capacity;

#if !defined(FX_SIZED_ARRAY_NO_MEMPOOL)
        for (uint64 i = 0; i < Size; i++) {
            new (&pData[i]) TElementType;
        }
#endif
    }

    size_t GetSizeInBytes() const { return Size * sizeof(TElementType); }

    size_t GetCapacityInBytes() const { return Capacity * sizeof(TElementType); }

    void Print()
    {
        FxLogChannelText<FxLogChannel::Info>();

        FxLogDirect("Array: {{ ");

        for (size_t i = 0; i < Size; i++) {
            FxLogDirect("{}", (*this)[i]);

            if (i < Size - 1) {
                FxLogDirect(", ");
            }
        }

        FxLogDirect(" }}\n");
    }

    FX_FORCE_INLINE bool IsInited() const { return (pData != nullptr && Capacity > 0); }


protected:
    void InternalAllocateArray(size_t element_count)
    {
#ifdef FX_SIZED_ARRAY_DEBUG
        FxLogDebug("Allocating FxSizedArray of capacity {:d} (type: {:s})", element_count, typeid(TElementType).name());
#endif
#if !defined(FX_SIZED_ARRAY_NO_MEMPOOL)
        pData = static_cast<TElementType*>(gEnginePool->AllocRaw(sizeof(TElementType) * element_count));
        // for (uint32 i = 0; i < Capacity; i++) {
        //     ::new (pData[i]) TElementType();
        // }

        if (pData == nullptr) {
            NoMemError();
        }
#else
        // pData = new TElementType[element_count];
        pData = reinterpret_cast<TElementType*>(std::malloc(sizeof(TElementType) * element_count));

        if (pData == nullptr) {
            NoMemError();
        }

#endif

        Capacity = element_count;
    }

public:
    TElementType* pData = nullptr;
    SizeType Size = 0;
    SizeType Capacity = 0;


    bool DoNotDestroy = false;
};
