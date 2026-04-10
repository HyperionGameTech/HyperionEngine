#pragma once

#include <Core/FxTypes.hpp>
#include <algorithm>
#include <cstdlib>

namespace FxGrowthFunctions {
inline constexpr uint32 Double(uint32 v) { return v * 2; }
inline constexpr uint32 InPages(uint32 v, uint32 page_size) { return v + page_size; }
} // namespace FxGrowthFunctions

using FxGrowthFunction = uint32(uint32, uint32);

template <typename TElementType, FxGrowthFunction TGrow = FxGrowthFunctions::InPages>
class FxDynArray
{
    static constexpr uint32 scInitialSize = 2;

public:
    FxDynArray() = default;

    FxDynArray(FxDynArray&& other) { (*this) = std::move(other); }

    TElementType& operator[](uint32 index) noexcept { return pData[index]; }

    void Insert(const TElementType& object)
    {
        ResizeIfNeeded();

        TElementType* dst = &pData[Size++];
        if constexpr (std::is_copy_constructible_v<TElementType>) {
            new (dst) TElementType(object);
        }
        else {
            memcpy(dst, &object, sizeof(TElementType));
        }
    }

    FX_FORCE_INLINE void SetPageSize(uint32 page_size) { PageSize = page_size; }


    FxDynArray& operator=(FxDynArray<TElementType>&& other) noexcept
    {
        Size = other.Size;
        Capacity = other.Capacity;
        PageSize = other.PageSize;

        pData = other.pData;

        other.pData = nullptr;

        return *this;
    }

    ~FxDynArray()
    {
        // Destroy all objects

        if constexpr (std::is_destructible_v<TElementType>) {
            for (uint32 i = 0; i < Size; i++) {
                pData[i].~TElementType();
            }
        }


        if (pData) {
            std::free(reinterpret_cast<void*>(pData));
        }
    }

private:
    void ResizeIfNeeded()
    {
        // If the data has not been allocated yet, allocate it with a minimum size.
        if (pData == nullptr) {
            Capacity = PageSize;
            pData = reinterpret_cast<TElementType*>(std::malloc(sizeof(TElementType) * Capacity));
        }

        // If there is no remaining space, use the growth function and resize our buffer.
        if (pData && Size + 1 >= Capacity) {
            Capacity = TGrow(Capacity, PageSize);
            pData = reinterpret_cast<TElementType*>(std::realloc(pData, sizeof(TElementType) * Capacity));
        }
    }

public:
    uint32 Size = 0;
    uint32 Capacity = 0;

    uint32 PageSize = scInitialSize;

    TElementType* pData = nullptr;
};
