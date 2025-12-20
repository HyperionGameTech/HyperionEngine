#pragma once

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/FlatSet.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/reflection/TypeInfoFwd.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/memory/AnyRef.hpp>

#include <core/serialization/fbom/FBOMResult.hpp>

namespace hyperion {

enum class FBOMDataFlags : uint32;

namespace serialization {
class FBOMData;
struct FBOMResult;
} // namespace serialization

using serialization::FBOMData;
using serialization::FBOMResult;

#pragma region GenericArrayWrapper

struct GenericArrayWrapper;
struct BoxedValue;

struct GenericArrayWrapper
{
    using SerializeFunction = FBOMResult (*)(const GenericArrayWrapper& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags);

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
        SerializeFunction serializeFunction;

        AnyRef (*pushBack)(GenericArrayWrapper& array, BoxedValue&& value);
        bool (*getElementAt)(GenericArrayWrapper& array, SizeType index, BoxedValue& out);
        bool (*setElementAt)(GenericArrayWrapper& array, SizeType index, BoxedValue&& value);
        SizeType (*size)(const GenericArrayWrapper& array);
        bool (*resize)(GenericArrayWrapper& array, SizeType newSize);
    };

    FunctionTable functionTable;

    GenericArrayWrapper()
        : pInternalArray(nullptr),
          typeInfo(&TypeInfo_Void()),
          elementTypeInfo(&TypeInfo_Void())
    {
        Memory::MemSet(&functionTable, 0, sizeof(FunctionTable));
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

        Memory::MemSet(&other.functionTable, 0, sizeof(FunctionTable));
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

        Memory::MemSet(&other.functionTable, 0, sizeof(FunctionTable));

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

    template <class T, SizeType Sz>
    GenericArrayWrapper(AsReferenceTag, FixedArray<T, Sz>& arr);

    template <class T, SizeType Sz>
    GenericArrayWrapper(AsCopyTag, const FixedArray<T, Sz>& arr);

    template <class T, SizeType Sz>
    GenericArrayWrapper(AsCopyTag, FixedArray<T, Sz>&& arr);

    // HashSet<T, KeyByFunction, AllocatorType>

    template <class T, auto KeyByFunction, class AllocatorType>
    GenericArrayWrapper(AsReferenceTag, HashSet<T, KeyByFunction, AllocatorType>& set);

    template <class T, auto KeyByFunction, class AllocatorType>
    GenericArrayWrapper(AsCopyTag, const HashSet<T, KeyByFunction, AllocatorType>& set);

    template <class T, auto KeyByFunction, class AllocatorType>
    GenericArrayWrapper(AsCopyTag, HashSet<T, KeyByFunction, AllocatorType>&& set);

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

    // HashMap<K, V, AllocatorType>

    template <class K, class V, class AllocatorType>
    GenericArrayWrapper(AsReferenceTag, HashMap<K, V, AllocatorType>& map);

    template <class K, class V, class AllocatorType>
    GenericArrayWrapper(AsCopyTag, const HashMap<K, V, AllocatorType>& map);

    template <class K, class V, class AllocatorType>
    GenericArrayWrapper(AsCopyTag, HashMap<K, V, AllocatorType>&& map);

    // LinkedList<T>

    template <class T>
    GenericArrayWrapper(AsReferenceTag, LinkedList<T>& list);

    template <class T>
    GenericArrayWrapper(AsCopyTag, const LinkedList<T>& list);

    template <class T>
    GenericArrayWrapper(AsCopyTag, LinkedList<T>&& list);

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

    HYP_FORCE_INLINE SizeType Size() const
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
        return functionTable.getElementAt != nullptr;
    }

    HYP_FORCE_INLINE bool GetElementAt(SizeType index, BoxedValue& out)
    {
        if (!IsValid() || !CanGetElementByIndex() || index >= Size())
        {
            return false;
        }

        return functionTable.getElementAt(*this, index, out);
    }

    HYP_FORCE_INLINE bool SetElementAt(SizeType index, BoxedValue&& value)
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

    HYP_FORCE_INLINE bool Resize(SizeType newSize)
    {
        if (!IsValid() || !CanResize())
        {
            return false;
        }

        return functionTable.resize(*this, newSize);
    }
};

#pragma endregion GenericArrayWrapper

} // namespace hyperion