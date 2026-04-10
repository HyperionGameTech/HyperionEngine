#pragma once

#include <Core/FxPanic.hpp>
#include <Core/FxSizedArray.hpp>
#include <Core/FxSlice.hpp>
#include <Core/FxTypes.hpp>
#include <cstring>

class FxAnonArray
{
public:
    FxAnonArray() = default;

    FxAnonArray(uint32 object_size, uint32 size) { Create(object_size, size); }
    FxAnonArray(FxAnonArray&& other);

    FxAnonArray& operator=(FxAnonArray&& other) noexcept;

    template <typename T>
    FxAnonArray(FxSizedArray<T>&& other)
    {
        (*this) = std::move(other);
    }

    FxAnonArray(const FxAnonArray& other) = delete;

    template <typename T>
    FxAnonArray& operator=(FxSizedArray<T>&& other) noexcept
    {
        pData = other.pData;
        Size = other.Size;
        Capacity = other.Capacity;
        ObjectSize = sizeof(T);

        other.pData = nullptr;
        other.Size = 0;
        other.Capacity = 0;

        return *this;
    }

    FxAnonArray& operator=(const FxAnonArray& other) = delete;

    void Create(uint32 object_size, uint32 size);

    template <typename T>
    void InitAsCopyOf(const FxSizedArray<T>& data)
    {
        FxAssert(pData == nullptr);

        Create(sizeof(T), data.Capacity);
        memcpy(pData, data.pData, data.GetSizeInBytes());
        Size = data.Size;
    }

    template <typename T>
    void Insert(const T& object)
    {
        FxAssert(sizeof(T) == ObjectSize);
        memcpy(&reinterpret_cast<T*>(pData)[Size], &object, sizeof(T));
        ++Size;
    }

    void InsertRaw(const void* value);

    FX_FORCE_INLINE bool IsEmpty() const { return pData == nullptr || Size == 0; }
    FX_FORCE_INLINE bool IsNotEmpty() const { return !IsEmpty(); }

    template <typename T>
    const T& Get(uint32 index) const
    {
        FxAssert(sizeof(T) == ObjectSize);
        FxAssert(index < Size);
        return reinterpret_cast<T*>(pData)[index];
    }

    template <typename T>
    T& Get(uint32 index)
    {
        FxAssert(sizeof(T) == ObjectSize);
        FxAssert(index < Size);
        return reinterpret_cast<T*>(pData)[index];
    }

    void* GetRaw(uint32 index);
    const void* GetRaw(uint32 index) const;

    template <typename T>
    T* GetPtr(uint32 index)
    {
        FxAssert(sizeof(T) == ObjectSize);
        FxAssert(index < Size);

        return reinterpret_cast<T*>(pData) + index;
    }

    void Free();

    ~FxAnonArray() { Free(); }

public:
    void* pData = nullptr;

    /// The size of each object to store
    uint32 ObjectSize = 0;

    uint32 Size = 0;
    uint32 Capacity = 0;
};
