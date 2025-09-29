#pragma once

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/utilities/TypeId.hpp>
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

#pragma region HypDataArray

struct HypDataArray;
struct HypData;

struct HypDataArray
{
    using SerializeFunction = FBOMResult (*)(const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags);

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
        void (*dtor)(void*);
        void (*copyCtor)(void** pDst, void* src);
        SerializeFunction serializeFunction;
    };

    TypeId arrayTypeId;
    TypeId elementTypeId;

    struct FunctionTable
    {
        AnyRef (*pushBack)(HypDataArray& array, HypData&& value);
        bool (*elementAt)(HypDataArray& array, SizeType index, HypData& out);
        SizeType (*size)(const HypDataArray& array);
    };

    FunctionTable functionTable;

    HypDataArray()
        : pInternalArray(nullptr),
          dtor(nullptr),
          copyCtor(nullptr),
          arrayTypeId(TypeId::Void()),
          elementTypeId(TypeId::Void()),
          serializeFunction(nullptr)
    {
        Memory::MemSet(&functionTable, 0, sizeof(FunctionTable));
    }

    HypDataArray(const HypDataArray& other)
        : HypDataArray()
    {
        if (other.copyCtor)
        {
            other.copyCtor(&pInternalArray, other.pInternalArray);
        }
        else
        {
            pInternalArray = other.pInternalArray;
        }

        copyCtor = other.copyCtor;
        dtor = other.dtor;
        arrayTypeId = other.arrayTypeId;
        elementTypeId = other.elementTypeId;
        serializeFunction = other.serializeFunction;
        functionTable = other.functionTable;
    }

    HypDataArray& operator=(const HypDataArray& other)
    {
        if (&other == this)
        {
            return *this;
        }

        if (dtor)
        {
            dtor(pInternalArray);
        }

        if (other.copyCtor)
        {
            other.copyCtor(&pInternalArray, other.pInternalArray);
        }
        else
        {
            pInternalArray = other.pInternalArray;
        }

        copyCtor = other.copyCtor;
        dtor = other.dtor;
        arrayTypeId = other.arrayTypeId;
        elementTypeId = other.elementTypeId;
        serializeFunction = other.serializeFunction;
        functionTable = other.functionTable;

        return *this;
    }

    HypDataArray(HypDataArray&& other) noexcept
        : pInternalArray(other.pInternalArray),
          dtor(other.dtor),
          copyCtor(other.copyCtor),
          arrayTypeId(other.arrayTypeId),
          elementTypeId(other.elementTypeId),
          serializeFunction(other.serializeFunction),
          functionTable(other.functionTable)
    {
        other.pInternalArray = nullptr;
        other.dtor = nullptr;
        other.copyCtor = nullptr;
        other.arrayTypeId = TypeId::Void();
        other.elementTypeId = TypeId::Void();
        other.serializeFunction = nullptr;

        Memory::MemSet(&other.functionTable, 0, sizeof(FunctionTable));
    }

    HypDataArray& operator=(HypDataArray&& other) noexcept
    {
        if (&other == this)
        {
            return *this;
        }

        if (dtor)
        {
            dtor(pInternalArray);
        }

        pInternalArray = other.pInternalArray;
        dtor = other.dtor;
        copyCtor = other.copyCtor;
        arrayTypeId = other.arrayTypeId;
        elementTypeId = other.elementTypeId;
        serializeFunction = other.serializeFunction;
        functionTable = other.functionTable;

        other.pInternalArray = nullptr;
        other.dtor = nullptr;
        other.copyCtor = nullptr;
        other.arrayTypeId = TypeId::Void();
        other.elementTypeId = TypeId::Void();
        other.serializeFunction = nullptr;

        Memory::MemSet(&other.functionTable, 0, sizeof(FunctionTable));

        return *this;
    }

    // Array<T, AllocatorType>

    template <class T, class AllocatorType>
    HypDataArray(AsReferenceTag, Array<T, AllocatorType>& arr);

    template <class T, class AllocatorType>
    HypDataArray(AsCopyTag, const Array<T, AllocatorType>& arr);

    template <class T, class AllocatorType>
    HypDataArray(AsCopyTag, Array<T, AllocatorType>&& arr);

    // FixedArray<T, Size>

    template <class T, SizeType Sz>
    HypDataArray(AsReferenceTag, FixedArray<T, Sz>& arr);

    template <class T, SizeType Sz>
    HypDataArray(AsCopyTag, const FixedArray<T, Sz>& arr);

    template <class T, SizeType Sz>
    HypDataArray(AsCopyTag, FixedArray<T, Sz>&& arr);

    // HashSet<T, KeyByFunction, AllocatorType>

    template <class T, auto KeyByFunction, class AllocatorType>
    HypDataArray(AsReferenceTag, HashSet<T, KeyByFunction, AllocatorType>& set);

    template <class T, auto KeyByFunction, class AllocatorType>
    HypDataArray(AsCopyTag, const HashSet<T, KeyByFunction, AllocatorType>& set);

    template <class T, auto KeyByFunction, class AllocatorType>
    HypDataArray(AsCopyTag, HashSet<T, KeyByFunction, AllocatorType>&& set);

    // HashMap<K, V, AllocatorType>

    template <class K, class V, class AllocatorType>
    HypDataArray(AsReferenceTag, HashMap<K, V, AllocatorType>& map);

    template <class K, class V, class AllocatorType>
    HypDataArray(AsCopyTag, const HashMap<K, V, AllocatorType>& map);

    template <class K, class V, class AllocatorType>
    HypDataArray(AsCopyTag, HashMap<K, V, AllocatorType>&& map);

    // LinkedList<T>

    template <class T>
    HypDataArray(AsReferenceTag, LinkedList<T>& list);

    template <class T>
    HypDataArray(AsCopyTag, const LinkedList<T>& list);

    template <class T>
    HypDataArray(AsCopyTag, LinkedList<T>&& list);

    ~HypDataArray()
    {
        if (dtor)
        {
            dtor(pInternalArray);
        }
    }

    HYP_FORCE_INLINE bool OwnsArray() const
    {
        return dtor != nullptr;
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

        HYP_CORE_ASSERT(functionTable.size != nullptr, "HypDataArray size function pointer is null");

        return functionTable.size(*this);
    }

    HYP_FORCE_INLINE bool CanPushBack() const
    {
        return functionTable.pushBack != nullptr;
    }

    HYP_FORCE_INLINE AnyRef PushBack(HypData&& value)
    {
        HYP_CORE_ASSERT(IsValid());
        HYP_CORE_ASSERT(CanPushBack());

        return functionTable.pushBack(*this, std::move(value));
    }

    HYP_FORCE_INLINE bool CanGetElementByIndex() const
    {
        return functionTable.elementAt != nullptr;
    }

    HYP_FORCE_INLINE bool ElementAt(SizeType index, HypData& out)
    {
        if (!IsValid() || !CanGetElementByIndex() || index >= Size())
        {
            return false;
        }

        return functionTable.elementAt(*this, index, out);
    }
};

#pragma endregion HypDataArray

} // namespace hyperion