#pragma once

#include <Core/Containers/String.hpp>
#include <Core/Containers/Map.hpp>
#include <Core/Containers/Set.hpp>
#include <Core/Containers/FlatMap.hpp>
#include <Core/Containers/FlatSet.hpp>
#include <Core/Containers/List.hpp>
#include <Core/Containers/SlimArray.hpp>

#include <Core/Reflection/TypeInfoFwd.hpp>
#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Memory/AnyRef.hpp>

namespace Hyperion {

#pragma region GenericArrayWrapper

struct GenericArrayWrapper;
struct BoxedValue;

struct GenericArrayWrapper
{
    enum AsReferenceTag
    {
        AS_REFERENCE
    };

    enum AsCopyTag
    {
        AS_COPY
    };

    struct
    {
        void* pInternalArray;
    };

    const TypeInfo* typeInfo;
    const TypeInfo* elementTypeInfo;

    struct FunctionTable
    {
        void (*dtor)(void*);
        void (*copyCtor)(void** pDst, void* src);

        AnyRef (*pushBack)(GenericArrayWrapper& array, BoxedValue&& value);
        AnyRef (*getElementAt)(GenericArrayWrapper& array, size_t index);
        bool (*getElementAt2)(GenericArrayWrapper& array, size_t index, BoxedValue& outValue);
        bool (*setElementAt)(GenericArrayWrapper& array, size_t index, BoxedValue&& value);
        size_t (*size)(const GenericArrayWrapper& array);
        bool (*resize)(GenericArrayWrapper& array, size_t newSize);
    };

    FunctionTable functionTable;

    GenericArrayWrapper()
        : pInternalArray(nullptr),
          typeInfo(&TypeInfo_Void()),
          elementTypeInfo(&TypeInfo_Void())
    {
        Memory::Zero(&functionTable, sizeof(FunctionTable));
    }

    GenericArrayWrapper(const GenericArrayWrapper& other)
        : GenericArrayWrapper()
    {
        if (other.functionTable.copyCtor)
        {
            other.functionTable.copyCtor(&pInternalArray, other.pInternalArray);
        }
        else
        {
            pInternalArray = other.pInternalArray;
        }

        typeInfo = other.typeInfo;
        elementTypeInfo = other.elementTypeInfo;
        functionTable = other.functionTable;
    }

    GenericArrayWrapper& operator=(const GenericArrayWrapper& other)
    {
        if (&other == this)
        {
            return *this;
        }

        if (functionTable.dtor)
        {
            functionTable.dtor(pInternalArray);
        }

        if (other.functionTable.copyCtor)
        {
            other.functionTable.copyCtor(&pInternalArray, other.pInternalArray);
        }
        else
        {
            pInternalArray = other.pInternalArray;
        }

        typeInfo = other.typeInfo;
        elementTypeInfo = other.elementTypeInfo;
        functionTable = other.functionTable;

        return *this;
    }

    GenericArrayWrapper(GenericArrayWrapper&& other) noexcept
        : pInternalArray(other.pInternalArray),
          typeInfo(other.typeInfo),
          elementTypeInfo(other.elementTypeInfo),
          functionTable(other.functionTable)
    {
        other.pInternalArray = nullptr;
        other.functionTable.copyCtor = nullptr;
        other.typeInfo = &TypeInfo_Void();
        other.elementTypeInfo = &TypeInfo_Void();

        Memory::Fill(&other.functionTable, 0, sizeof(FunctionTable));
    }

    GenericArrayWrapper& operator=(GenericArrayWrapper&& other) noexcept
    {
        if (&other == this)
        {
            return *this;
        }

        if (functionTable.dtor)
        {
            functionTable.dtor(pInternalArray);
        }

        pInternalArray = other.pInternalArray;
        typeInfo = other.typeInfo;
        elementTypeInfo = other.elementTypeInfo;
        functionTable = other.functionTable;

        other.pInternalArray = nullptr;
        other.typeInfo = &TypeInfo_Void();
        other.elementTypeInfo = &TypeInfo_Void();

        Memory::Zero(&other.functionTable, sizeof(FunctionTable));

        return *this;
    }

    // Array<T, AllocatorType>

    template <class T, class AllocatorType>
    GenericArrayWrapper(AsReferenceTag, Array<T, AllocatorType>& arr);

    template <class T, class AllocatorType>
    GenericArrayWrapper(AsCopyTag, const Array<T, AllocatorType>& arr);

    template <class T, class AllocatorType>
    GenericArrayWrapper(AsCopyTag, Array<T, AllocatorType>&& arr);

    // FixedArray<T, Size>

    template <class T, size_t Sz>
    GenericArrayWrapper(AsReferenceTag, FixedArray<T, Sz>& arr);

    template <class T, size_t Sz>
    GenericArrayWrapper(AsCopyTag, const FixedArray<T, Sz>& arr);

    template <class T, size_t Sz>
    GenericArrayWrapper(AsCopyTag, FixedArray<T, Sz>&& arr);

    // Set<T, AllocatorType>

    template <class T, class AllocatorType>
    GenericArrayWrapper(AsReferenceTag, Set<T, AllocatorType>& set);

    template <class T, class AllocatorType>
    GenericArrayWrapper(AsCopyTag, const Set<T, AllocatorType>& set);

    template <class T, class AllocatorType>
    GenericArrayWrapper(AsCopyTag, Set<T, AllocatorType>&& set);

    // FlatSet<T>

    template <class T>
    GenericArrayWrapper(AsReferenceTag, FlatSet<T>& set);

    template <class T>
    GenericArrayWrapper(AsCopyTag, const FlatSet<T>& set);

    template <class T>
    GenericArrayWrapper(AsCopyTag, FlatSet<T>&& set);

    // FlatMap<K, V>

    template <class K, class V>
    GenericArrayWrapper(AsReferenceTag, FlatMap<K, V>& map);

    template <class K, class V>
    GenericArrayWrapper(AsCopyTag, const FlatMap<K, V>& map);

    template <class K, class V>
    GenericArrayWrapper(AsCopyTag, FlatMap<K, V>&& map);

    // Map<K, V, AllocatorType>

    template <class K, class V, class AllocatorType>
    GenericArrayWrapper(AsReferenceTag, Map<K, V, AllocatorType>& map);

    template <class K, class V, class AllocatorType>
    GenericArrayWrapper(AsCopyTag, const Map<K, V, AllocatorType>& map);

    template <class K, class V, class AllocatorType>
    GenericArrayWrapper(AsCopyTag, Map<K, V, AllocatorType>&& map);

    // List<T>

    template <class T>
    GenericArrayWrapper(AsReferenceTag, List<T>& list);

    template <class T>
    GenericArrayWrapper(AsCopyTag, const List<T>& list);

    template <class T>
    GenericArrayWrapper(AsCopyTag, List<T>&& list);

#if !defined(HYP_USE_SLIM_ARRAY) || !HYP_USE_SLIM_ARRAY
    // SlimArray<TElemType, TAllocator>

    template <class TElemType, class TAllocator>
    GenericArrayWrapper(AsReferenceTag, SlimArray<TElemType, TAllocator>& arr);

    template <class TElemType, class TAllocator>
    GenericArrayWrapper(AsCopyTag, const SlimArray<TElemType, TAllocator>& arr);

    template <class TElemType, class TAllocator>
    GenericArrayWrapper(AsCopyTag, SlimArray<TElemType, TAllocator>&& arr);
#endif // !HYP_USE_SLIM_ARRAY

    ~GenericArrayWrapper()
    {
        if (functionTable.dtor)
        {
            functionTable.dtor(pInternalArray);
        }
    }

    HYP_FORCE_INLINE bool OwnsArray() const
    {
        return functionTable.dtor != nullptr;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return pInternalArray != nullptr;
    }

    HYP_FORCE_INLINE size_t Size() const
    {
        if (!IsValid())
        {
            return 0;
        }

        HYP_CORE_ASSERT(functionTable.size != nullptr, "GenericArrayWrapper size function pointer is null");

        return functionTable.size(*this);
    }

    HYP_FORCE_INLINE bool CanPushBack() const
    {
        return functionTable.pushBack != nullptr;
    }

    HYP_FORCE_INLINE AnyRef PushBack(BoxedValue&& value)
    {
        HYP_CORE_ASSERT(IsValid());
        HYP_CORE_ASSERT(CanPushBack());

        return functionTable.pushBack(*this, std::move(value));
    }

    HYP_FORCE_INLINE bool CanGetElementByIndex() const
    {
        return functionTable.getElementAt != nullptr
            && functionTable.getElementAt2 != nullptr;
    }

    /*! \brief Get a reference to element at \p index */
    HYP_FORCE_INLINE AnyRef GetElementAt(size_t index)
    {
        if (!IsValid() || !CanGetElementByIndex() || index >= Size())
        {
            return AnyRef();
        }

        return functionTable.getElementAt(*this, index);
    }

    /*! \brief Get the element at \p index by value and store it in \p outValue
     *   \returns True on success, false otherwise. If false was returned, \p outValue has
     *   not been modified. */
    HYP_FORCE_INLINE bool GetElementAt(size_t index, BoxedValue& outValue)
    {
        if (!IsValid() || !CanGetElementByIndex() || index >= Size())
        {
            return false;
        }

        return functionTable.getElementAt2(*this, index, outValue);
    }

    HYP_FORCE_INLINE bool SetElementAt(size_t index, BoxedValue&& value)
    {
        if (!IsValid() || !CanGetElementByIndex() || index >= Size())
        {
            return false;
        }

        return functionTable.setElementAt(*this, index, std::move(value));
    }

    HYP_FORCE_INLINE bool CanResize() const
    {
        return functionTable.resize != nullptr;
    }

    HYP_FORCE_INLINE bool Resize(size_t newSize)
    {
        if (!IsValid() || !CanResize())
        {
            return false;
        }

        return functionTable.resize(*this, newSize);
    }
};

#pragma endregion GenericArrayWrapper

} // namespace Hyperion
