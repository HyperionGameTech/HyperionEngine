/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/object/ObjId.hpp>
#include <core/object/Handle.hpp>
#include <core/object/HypObjectFwd.hpp>

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>

#include <core/utilities/TypeId.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/Pair.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Result.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/ByteBuffer.hpp>

#include <core/serialization/fbom/FBOMResult.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOMObject.hpp>
#include <core/serialization/fbom/FBOMArray.hpp>
#include <core/serialization/fbom/FBOM.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/Types.hpp>

#include <type_traits>

namespace hyperion {

struct HypData;

class Node;
class Entity;

namespace filesystem {
class FilePath;
} // namespace filesystem

using filesystem::FilePath;

extern HYP_API const HypClass* GetClass(TypeId typeId);
extern HYP_API bool IsA(const HypClass* hypClass, const HypClass* instanceHypClass);

template <class T, class T2 = void>
struct HypDataHelper;

template <class T, class T2 = void>
struct HypDataHelperDecl;

template <class T, class ConvertibleFromTuple>
struct HypDataTypeChecker_Tuple;

template <class ReturnType, class T, class ConvertibleFromTuple>
struct HypDataGetter_Tuple;

template <class T, bool IsConst>
struct HypDataGetReturnTypeHelper
{
    using Type = decltype(std::declval<HypDataHelper<T>>().Get(*std::declval<std::add_pointer_t<std::conditional_t<IsConst, std::add_const_t<T>, T>>>()));
};

using HypDataSerializeFunction = FBOMResult (*)(const HypData& data, FBOMData& out, EnumFlags<FBOMDataFlags> flags);

extern HYP_API HypDataSerializeFunction GetHypDataSerializeFunction(TypeId typeId);
extern HYP_API void RegisterHypDataSerializeFunction(TypeId typeId, HypDataSerializeFunction func);

/*! \brief A struct that can hold 128 bits (16 bytes) of user data.
 *  Useful for storing small amounts of data directly in HypData without heap allocation.
 *  \note This is primarily for internal use and should be used with care to avoid alignment issues.
 */
struct alignas(std::max_align_t) HypData_UserData128
{
    uint64 data[2];
};

struct HypDataArray;

#ifdef HYP_SCRIPT
enum class GCIndex : uint32;

static constexpr GCIndex INVALID_GC_INDEX = GCIndex(0);
static constexpr GCIndex GARBAGE_GC_INDEX = GCIndex(~0u);
#endif

/*! \brief A type-safe union that can store multiple different types of run-time data, abstracting away internal engine structures such as Handle<T>, RC<T>, etc.
 *  Providing a unified way of accessing the data via Get<T>() and TryGet<T>() methods.
 *  \note Used in serialization, reflection, scripting, and other systems where data needs to be stored in a generic way.
 */
struct HypData
{
    using VariantType = Variant<
        int8,
        int16,
        int32,
        int64,
        uint8,
        uint16,
        uint32,
        uint64,
        char,
        float,
        double,
        bool,
        void*,
        Name,
        ObjIdBase,
        HypClassRef,
        AnyHandle,
        RC<void>,
        AnyRef,
        Any,
        HypData_UserData128>;

    template <class T>
    static constexpr bool canStoreDirectly =
        /* Fundamental types - can be stored inline */
        std::is_same_v<T, int8> || std::is_same_v<T, int16> | std::is_same_v<T, int32> | std::is_same_v<T, int64>
        || std::is_same_v<T, uint8> || std::is_same_v<T, uint16> || std::is_same_v<T, uint32> || std::is_same_v<T, uint64>

        || std::is_same_v<T, char>
        || std::is_same_v<T, float> || std::is_same_v<T, double>
        || std::is_same_v<T, bool>
        || std::is_same_v<T, void*>

        /*! Name is 32 bits and can be stored inline */
        || std::is_same_v<T, Name>

        /*! All ObjId<T> are stored as ObjIdBase */
        || std::is_base_of_v<ObjIdBase, T>

        || std::is_same_v<T, HypClassRef>

        /*! Handle<T> gets stored as AnyHandle, which holds TypeId for conversion */
        || std::is_base_of_v<HandleBase, T> || std::is_same_v<T, AnyHandle>

        /*! RC<T> gets stored as RC<void> and can be converted back */
        || std::is_base_of_v<typename RC<void>::RefCountedPtrBase, T>

        /*! Pointers are stored as AnyRef which holds TypeId for conversion */
        || std::is_same_v<T, AnyRef> || std::is_pointer_v<T>

        || std::is_same_v<T, Any>
        || std::is_same_v<T, HypData_UserData128>;

    VariantType value;

    union
    {
        struct alignas(8)
        {
#ifdef HYP_SCRIPT
            GCIndex scriptGcIndex; // HypScript only - index into the pool of tracked objects
#endif
        };

        uint64 num;
    } extData;

    HypData()
    {
        Memory::MemSet(&extData, 0, sizeof(extData));
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<T, HypData>>>
    explicit HypData(T&& value)
        : HypData()
    {
        HYP_NAMED_SCOPE("Construct HypData from T");

        HypDataHelper<NormalizedType<T>> {}.Set(*this, std::forward<T>(value));
    }

    HypData(const HypData& other)
        : value(other.value)
    {
        Memory::MemCpy(&extData, &other.extData, sizeof(extData));
    }

    HypData& operator=(const HypData& other)
    {
        if (&other == this)
        {
            return *this;
        }

        value = other.value;

        Memory::MemCpy(&extData, &other.extData, sizeof(extData));

        return *this;
    }

    HypData(HypData&& other) noexcept
        : value(std::move(other.value))
    {
        Memory::MemCpy(&extData, &other.extData, sizeof(extData));
        Memory::MemSet(&other.extData, 0, sizeof(extData));
    }

    HypData& operator=(HypData&& other) noexcept
    {
        if (&other == this)
        {
            return *this;
        }

        value = std::move(other.value);

        Memory::MemCpy(&extData, &other.extData, sizeof(extData));
        Memory::MemSet(&other.extData, 0, sizeof(extData));

        return *this;
    }

    ~HypData()
    {
#ifdef HYP_DEBUG_MODE
#ifdef HYP_SCRIPT
        HYP_CORE_ASSERT(extData.scriptGcIndex == INVALID_GC_INDEX, "HypData being destroyed while still registered with the GC (index = %u)", uint32(extData.scriptGcIndex));
        extData.scriptGcIndex = GARBAGE_GC_INDEX;
#endif
#endif
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return value.IsValid();
    }

    HYP_FORCE_INLINE bool IsNull() const
    {
        return !ToRef().HasValue();
    }

    HYP_FORCE_INLINE bool IsArray() const
    {
        return Is<HypDataArray>();
    }

    HYP_FORCE_INLINE TypeId GetTypeId() const
    {
        return ToRef().GetTypeId();
    }

    HYP_FORCE_INLINE void Reset()
    {
        value.Reset();
    }

    HYP_FORCE_INLINE AnyRef ToRef()
    {
        HYP_SCOPE;

        if (!IsValid())
        {
            return AnyRef();
        }

        if (AnyHandle* anyHandlePtr = value.TryGet<AnyHandle>())
        {
            return anyHandlePtr->ToRef();
        }

        if (RC<void>* rcPtr = value.TryGet<RC<void>>())
        {
            return rcPtr->ToRef();
        }

        if (AnyRef* anyRefPtr = value.TryGet<AnyRef>())
        {
            return *anyRefPtr;
        }

        if (Any* anyPtr = value.TryGet<Any>())
        {
            return anyPtr->ToRef();
        }

        return AnyRef(value.GetTypeId(), value.GetPointer());
    }

    HYP_FORCE_INLINE AnyRef ToRef() const
    {
        return const_cast<HypData*>(this)->ToRef();
    }

    template <class T>
    HYP_FORCE_INLINE bool Is(bool strict = false) const
    {
        HYP_SCOPE;

        if constexpr (std::is_same_v<T, HypData>)
        {
            return true;
        }
        else
        {
            if (!value.IsValid())
            {
                return false;
            }

            if (strict)
            {
                return HypDataTypeChecker_Tuple<T, Tuple<>> {}(value);
            }

            return HypDataTypeChecker_Tuple<T, typename HypDataHelper<T>::ConvertibleFrom> {}(value);
        }
    }

    template <class T>
    HYP_FORCE_INLINE auto Get() -> std::conditional_t<std::is_same_v<T, HypData>, HypData&, typename HypDataGetReturnTypeHelper<T, false>::Type>
    {
        HYP_SCOPE;

        if constexpr (std::is_same_v<T, HypData>)
        {
            return *this;
        }
        else
        {
            using ReturnType = typename HypDataGetReturnTypeHelper<T, false>::Type;

            Optional<ReturnType> resultValue;

            HypDataGetter_Tuple<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> getterInstance {};

            HYP_CORE_ASSERT(getterInstance(value, resultValue),
                "Failed to invoke HypData Get method with T = %s - Mismatched types or T could not be converted to the held type (current TypeId = %u)",
                TypeName<T>().Data(),
                GetTypeId().Value());

            return *resultValue;
        }
    }

    template <class T>
    HYP_FORCE_INLINE auto Get() const -> std::conditional_t<std::is_same_v<T, HypData>, const HypData&, typename HypDataGetReturnTypeHelper<T, true>::Type>
    {
        HYP_SCOPE;

        if constexpr (std::is_same_v<T, HypData>)
        {
            return *this;
        }
        else
        {
            using ReturnType = typename HypDataGetReturnTypeHelper<T, true>::Type;

            Optional<ReturnType> resultValue;

            HypDataGetter_Tuple<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> getterInstance {};

            HYP_CORE_ASSERT(getterInstance(value, resultValue),
                "Failed to invoke HypData Get method with T = %s - Mismatched types or T could not be converted to the held type (current TypeId = %u)",
                TypeName<T>().Data(),
                GetTypeId().Value());

            return *resultValue;
        }
    }

    template <class T>
    HYP_FORCE_INLINE auto TryGet() -> Optional<std::conditional_t<std::is_same_v<T, HypData>, HypData&, typename HypDataGetReturnTypeHelper<T, false>::Type>>
    {
        HYP_SCOPE;

        if constexpr (std::is_same_v<T, HypData>)
        {
            return *this;
        }
        else
        {
            using ReturnType = typename HypDataGetReturnTypeHelper<T, false>::Type;

            Optional<ReturnType> resultValue;

            HypDataGetter_Tuple<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> getterInstance {};
            getterInstance(value, resultValue);

            return resultValue;
        }
    }

    template <class T>
    HYP_FORCE_INLINE auto TryGet() const -> Optional<std::conditional_t<std::is_same_v<T, HypData>, const HypData&, typename HypDataGetReturnTypeHelper<T, true>::Type>>
    {
        HYP_SCOPE;

        if constexpr (std::is_same_v<T, HypData>)
        {
            return *this;
        }
        else
        {
            using ReturnType = typename HypDataGetReturnTypeHelper<T, true>::Type;

            Optional<ReturnType> resultValue;

            HypDataGetter_Tuple<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> getterInstance {};
            getterInstance(value, resultValue);

            return resultValue;
        }
    }

    /*! \brief Serialize this instance to an FBOMData object.
     *  \param out The FBOMData object to serialize to.
     *  \return The result of the serialization operation.
     */
    FBOMResult Serialize(FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE) const
    {
        if (!IsValid())
        {
            out = FBOMData();

            return {};
        }

        const HypDataSerializeFunction serializeFunction = GetHypDataSerializeFunction(GetTypeId());

        if (!serializeFunction)
        {
            return { FBOMResult::FBOM_ERR, "No serialization function provided" };
        }

        return serializeFunction(*this, out, flags);
    }

    template <class T>
    static FBOMResult Serialize(T&& value, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        return HypDataHelper<NormalizedType<T>>::Serialize(value, out, flags);
    }

    template <class T>
    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, T& out)
    {
        HypData outData;

        if (FBOMResult err = HypDataHelper<NormalizedType<T>>::Deserialize(context, data, outData))
        {
            return err;
        }

        out = std::move(outData.Get<T>());

        return {};
    }

    template <class T>
    void Set_Internal(T&& value)
    {
        static struct InitializeSerializeFunction
        {
            InitializeSerializeFunction()
            {
                RegisterHypDataSerializeFunction(
                    TypeId::ForType<NormalizedType<T>>(),
                    [](const HypData& data, FBOMData& out, EnumFlags<FBOMDataFlags> flags) -> FBOMResult
                    {
                        return HypDataHelper<NormalizedType<T>>::Serialize(data.Get<NormalizedType<T>>(), out, flags);
                    });
            }
        } s_initializeSerializeFunction;

        this->value.Set<NormalizedType<T>>(std::forward<T>(value));
    }
};

#pragma region HypDataArray

struct HypDataArray
{
    using SerializeFunction = FBOMResult (*)(const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags);

    Any internalArray;
    TypeId elementTypeId;

    SerializeFunction serializeFunction;

    struct FunctionTable
    {
        AnyRef (*pushBack)(HypDataArray& array, HypData&& value);
        AnyRef (*elementAt)(HypDataArray& array, SizeType index);
        SizeType (*size)(const HypDataArray& array);
    };

    FunctionTable functionTable;

    HypDataArray()
        : elementTypeId(TypeId::Void()),
          serializeFunction(nullptr)
    {
        Memory::MemSet(&functionTable, 0, sizeof(FunctionTable));
    }

    template <class T, class AllocatorType>
    explicit HypDataArray(const Array<T, AllocatorType>& arr)
        : HypDataArray()
    {
        internalArray = Any::Construct<Array<T, AllocatorType>>(arr), TypeId::ForType<T>();
        elementTypeId = TypeId::ForType<T>();
        serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
        {
            return HypDataHelper<Array<T, AllocatorType>>::Serialize(array.internalArray.Get<Array<T, AllocatorType>>(), outData, flags);
        };
        functionTable.pushBack = [](HypDataArray& array, HypData&& value) -> AnyRef
        {
            auto& arr = array.internalArray.Get<Array<T, AllocatorType>>();
            return AnyRef(arr.PushBack(std::move(value.Get<T>())));
        };
        functionTable.elementAt = [](HypDataArray& array, SizeType index) -> AnyRef
        {
            auto& arr = array.internalArray.Get<Array<T, AllocatorType>>();
            HYP_CORE_ASSERT(index < arr.Size(), "Index out of bounds");
            return AnyRef(arr[index]);
        };
        functionTable.size = [](const HypDataArray& array) -> SizeType
        {
            return array.internalArray.Get<Array<T, AllocatorType>>().Size();
        };
    }

    template <class T, class AllocatorType>
    explicit HypDataArray(Array<T, AllocatorType>&& arr)
        : HypDataArray()
    {
        internalArray = Any::Construct<Array<T, AllocatorType>>(std::move(arr)), TypeId::ForType<T>();
        elementTypeId = TypeId::ForType<T>();
        serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
        {
            return HypDataHelper<Array<T, AllocatorType>>::Serialize(array.internalArray.Get<Array<T, AllocatorType>>(), outData, flags);
        };
        functionTable.pushBack = [](HypDataArray& array, HypData&& value) -> AnyRef
        {
            auto& arr = array.internalArray.Get<Array<T, AllocatorType>>();
            return AnyRef(arr.PushBack(std::move(value.Get<T>())));
        };
        functionTable.elementAt = [](HypDataArray& array, SizeType index) -> AnyRef
        {
            auto& arr = array.internalArray.Get<Array<T, AllocatorType>>();
            HYP_CORE_ASSERT(index < arr.Size(), "Index out of bounds");
            return AnyRef(arr[index]);
        };
        functionTable.size = [](const HypDataArray& array) -> SizeType
        {
            return array.internalArray.Get<Array<T, AllocatorType>>().Size();
        };
    }

    template <class T, SizeType Sz>
    explicit HypDataArray(const FixedArray<T, Sz>& arr)
        : HypDataArray()
    {
        internalArray = Any::Construct<FixedArray<T, Sz>>(arr), TypeId::ForType<T>();
        elementTypeId = TypeId::ForType<T>();
        serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
        {
            return HypDataHelper<FixedArray<T, Sz>>::Serialize(array.internalArray.Get<FixedArray<T, Sz>>(), outData, flags);
        };
        functionTable.elementAt = [](HypDataArray& array, SizeType index) -> AnyRef
        {
            auto& arr = array.internalArray.Get<FixedArray<T, Sz>>();
            HYP_CORE_ASSERT(index < arr.Size(), "Index out of bounds");
            return AnyRef(arr[index]);
        };
        functionTable.size = [](const HypDataArray& array) -> SizeType
        {
            return array.internalArray.Get<FixedArray<T, Sz>>().Size();
        };
    }

    template <class T, SizeType Sz>
    explicit HypDataArray(FixedArray<T, Sz>&& arr)
        : HypDataArray()
    {
        internalArray = Any::Construct<FixedArray<T, Sz>>(std::move(arr)), TypeId::ForType<T>();
        elementTypeId = TypeId::ForType<T>();
        serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
        {
            return HypDataHelper<FixedArray<T, Sz>>::Serialize(array.internalArray.Get<FixedArray<T, Sz>>(), outData, flags);
        };
        functionTable.elementAt = [](HypDataArray& array, SizeType index) -> AnyRef
        {
            auto& arr = array.internalArray.Get<FixedArray<T, Sz>>();
            HYP_CORE_ASSERT(index < arr.Size(), "Index out of bounds");
            return AnyRef(arr[index]);
        };
        functionTable.size = [](const HypDataArray& array) -> SizeType
        {
            return array.internalArray.Get<FixedArray<T, Sz>>().Size();
        };
    }

    template <class T, auto KeyByFunction, class AllocatorType>
    explicit HypDataArray(const HashSet<T, KeyByFunction, AllocatorType>& set)
        : HypDataArray()
    {
        internalArray = Any::Construct<HashSet<T, KeyByFunction, AllocatorType>>(set), TypeId::ForType<T>();
        elementTypeId = TypeId::ForType<T>();
        serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
        {
            return HypDataHelper<HashSet<T, KeyByFunction, AllocatorType>>::Serialize(array.internalArray.Get<HashSet<T, KeyByFunction, AllocatorType>>(), outData, flags);
        };
        functionTable.size = [](const HypDataArray& array) -> SizeType
        {
            return array.internalArray.Get<HashSet<T, KeyByFunction, AllocatorType>>().Size();
        };
    }

    template <class T, auto KeyByFunction, class AllocatorType>
    explicit HypDataArray(HashSet<T, KeyByFunction, AllocatorType>&& set)
        : HypDataArray()
    {
        internalArray = Any::Construct<HashSet<T, KeyByFunction, AllocatorType>>(std::move(set)), TypeId::ForType<T>();
        elementTypeId = TypeId::ForType<T>();
        serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
        {
            return HypDataHelper<HashSet<T, KeyByFunction, AllocatorType>>::Serialize(array.internalArray.Get<HashSet<T, KeyByFunction, AllocatorType>>(), outData, flags);
        };
        functionTable.size = [](const HypDataArray& array) -> SizeType
        {
            return array.internalArray.Get<HashSet<T, KeyByFunction, AllocatorType>>().Size();
        };
    }

    template <class K, class V, class AllocatorType>
    explicit HypDataArray(const HashMap<K, V, AllocatorType>& map)
        : HypDataArray()
    {
        internalArray = Any::Construct<HashMap<K, V, AllocatorType>>(map), TypeId::ForType<KeyValuePair<K, V>>();
        elementTypeId = TypeId::ForType<KeyValuePair<K, V>>();
        serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
        {
            return HypDataHelper<HashMap<K, V, AllocatorType>>::Serialize(array.internalArray.Get<HashMap<K, V, AllocatorType>>(), outData, flags);
        };
        functionTable.size = [](const HypDataArray& array) -> SizeType
        {
            return array.internalArray.Get<HashMap<K, V, AllocatorType>>().Size();
        };
    }

    template <class K, class V, class AllocatorType>
    explicit HypDataArray(HashMap<K, V, AllocatorType>&& map)
        : HypDataArray()
    {
        internalArray = Any::Construct<HashMap<K, V, AllocatorType>>(std::move(map)), TypeId::ForType<KeyValuePair<K, V>>();
        elementTypeId = TypeId::ForType<KeyValuePair<K, V>>();
        serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
        {
            return HypDataHelper<HashMap<K, V, AllocatorType>>::Serialize(array.internalArray.Get<HashMap<K, V, AllocatorType>>(), outData, flags);
        };
        functionTable.size = [](const HypDataArray& array) -> SizeType
        {
            return array.internalArray.Get<HashMap<K, V, AllocatorType>>().Size();
        };
    }

    template <class T>
    explicit HypDataArray(const LinkedList<T>& list)
        : HypDataArray()
    {
        internalArray = Any::Construct<LinkedList<T>>(list), TypeId::ForType<T>();
        elementTypeId = TypeId::ForType<T>();
        serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
        {
            return HypDataHelper<LinkedList<T>>::Serialize(array.internalArray.Get<LinkedList<T>>(), outData, flags);
        };
        functionTable.pushBack = [](HypDataArray& array, HypData&& value) -> AnyRef
        {
            auto& list = array.internalArray.Get<LinkedList<T>>();
            return AnyRef(list.PushBack(std::move(value.Get<T>())));
        };
        functionTable.size = [](const HypDataArray& array) -> SizeType
        {
            return array.internalArray.Get<LinkedList<T>>().Size();
        };
    }

    template <class T>
    explicit HypDataArray(LinkedList<T>&& list)
        : HypDataArray()
    {
        internalArray = Any::Construct<LinkedList<T>>(std::move(list)), TypeId::ForType<T>();
        elementTypeId = TypeId::ForType<T>();
        serializeFunction = [](const HypDataArray& array, FBOMData& outData, EnumFlags<FBOMDataFlags> flags)
        {
            return HypDataHelper<LinkedList<T>>::Serialize(array.internalArray.Get<LinkedList<T>>(), outData, flags);
        };
        functionTable.pushBack = [](HypDataArray& array, HypData&& value) -> AnyRef
        {
            auto& list = array.internalArray.Get<LinkedList<T>>();
            return AnyRef(list.PushBack(std::move(value.Get<T>())));
        };
        functionTable.size = [](const HypDataArray& array) -> SizeType
        {
            return array.internalArray.Get<LinkedList<T>>().Size();
        };
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return internalArray.HasValue();
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
        HYP_CORE_ASSERT(CanPushBack(), "Cannot push to HypDataArray (internal type: %s)", LookupTypeName(internalArray.GetTypeId()));

        return functionTable.pushBack(*this, std::move(value));
    }

    HYP_FORCE_INLINE bool CanGetElementByIndex() const
    {
        return functionTable.elementAt != nullptr;
    }

    HYP_FORCE_INLINE AnyRef ElementAt(SizeType index)
    {
        HYP_CORE_ASSERT(IsValid());
        HYP_CORE_ASSERT(CanGetElementByIndex(), "Cannot get element by index from HypDataArray (internal type: %s)", LookupTypeName(internalArray.GetTypeId()));
        HYP_CORE_ASSERT(index < Size(), "Index out of bounds when accessing HypDataArray (index: %llu, size: %llu)", index, Size());

        return functionTable.elementAt(*this, index);
    }

    HYP_FORCE_INLINE ConstAnyRef ElementAt(SizeType index) const
    {
        HYP_CORE_ASSERT(IsValid());
        HYP_CORE_ASSERT(CanGetElementByIndex(), "Cannot get element by index from HypDataArray (internal type: %s)", LookupTypeName(internalArray.GetTypeId()));
        HYP_CORE_ASSERT(index < Size(), "Index out of bounds when accessing HypDataArray (index: %llu, size: %llu)", index, Size());

        return functionTable.elementAt(*const_cast<HypDataArray*>(this), index);
    }
};

#pragma endregion HypDataArray

template <class T>
constexpr bool isHypData = std::is_same_v<NormalizedType<T>, HypData>;

template <class T>
struct DefaultHypDataSerializeFunction
{
    static inline FBOMResult Serialize(const HypData& data, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        if (FBOMResult err = HypDataHelper<NormalizedType<T>>::Serialize(data.Get<NormalizedType<T>>(), out, flags))
        {
            return err;
        }

        return {};
    }
};

#pragma region HypDataMarshalHelper

struct HypDataMarshalHelper
{
    static HYP_API FBOMResult NoMarshalRegistered(ANSIStringView typeName);

    template <class T>
    static inline FBOMResult Serialize(const T& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        using Normalized = NormalizedType<T>;

        HYP_SCOPE;

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(TypeId::ForType<Normalized>());

        if (!marshal)
        {
            return NoMarshalRegistered(TypeName<Normalized>());
        }

        FBOMObject object;

        if (FBOMResult err = marshal->Serialize(ConstAnyRef(value), object))
        {
            return err;
        }

        outData = FBOMData::FromObject(std::move(object));

        return FBOMResult::FBOM_OK;
    }

    template <class T>
    static inline FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(TypeId::ForType<T>());

        if (!marshal)
        {
            return NoMarshalRegistered(TypeName<T>());
        }

        if (!data)
        {
            return { FBOMResult::FBOM_ERR, "Cannot deserialize unset value" };
        }

        FBOMObject object;

        if (FBOMResult err = data.ReadObject(context, object))
        {
            return err;
        }

        if (FBOMResult err = marshal->Deserialize(context, object, out))
        {
            return err;
        }

        return FBOMResult::FBOM_OK;
    }
};

#pragma endregion HypDataMarshalHelper

template <class T>
struct HypDataPlaceholderSerializedType;

template <class T>
struct HypDataHelperDecl<T, std::enable_if_t<std::is_fundamental_v<T>>>
{
};

template <class T>
struct HypDataHelper<T, std::enable_if_t<std::is_fundamental_v<T>>>
{
    using StorageType = T;
    using ConvertibleFrom = Tuple<
        int8,
        int16,
        int32,
        int64,
        uint8,
        uint16,
        uint32,
        uint64,
        char,
        float,
        double,
        bool>;

    HYP_FORCE_INLINE bool Is(T value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<OtherT, T>>>
    HYP_FORCE_INLINE bool Is(OtherT value) const
    {
        return std::is_fundamental_v<OtherT>;
    }

    HYP_FORCE_INLINE constexpr T Get(T value) const
    {
        return value;
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<OtherT, T>>>
    HYP_FORCE_INLINE constexpr T Get(OtherT value) const
    {
        return static_cast<T>(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, T value) const
    {
        hypData.Set_Internal(value);
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(T value, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        out = FBOMData(value, flags);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        T value;

        if (FBOMResult err = data.Read(&value))
        {
            return err;
        }

        out = HypData(value);

        return FBOMResult::FBOM_OK;
    }
};

#ifndef HYP_WINDOWS

template <>
struct HypDataHelperDecl<SizeType, std::enable_if_t<!std::is_same_v<SizeType, uint64>>>
{
};

template <>
struct HypDataHelper<SizeType, std::enable_if_t<!std::is_same_v<SizeType, uint64>>> : HypDataHelper<uint64>
{
    using StorageType = uint64;
    using ConvertibleFrom = Tuple<
        int8,
        int16,
        int32,
        int64,
        uint8,
        uint16,
        uint32,
        uint64,
        char,
        float,
        double,
        bool>;

    HYP_FORCE_INLINE bool Is(SizeType value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<OtherT, SizeType>>>
    HYP_FORCE_INLINE bool Is(OtherT value) const
    {
        return std::is_fundamental_v<OtherT>;
    }

    HYP_FORCE_INLINE constexpr SizeType Get(SizeType value) const
    {
        return value;
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<OtherT, SizeType>>>
    HYP_FORCE_INLINE constexpr SizeType Get(OtherT value) const
    {
        return static_cast<SizeType>(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, SizeType value) const
    {
        hypData.Set_Internal(static_cast<uint64>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(SizeType value, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        out = FBOMData(static_cast<uint64>(value), flags);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        uint64 value;

        if (FBOMResult err = data.Read(&value))
        {
            return err;
        }

        out = HypData(static_cast<SizeType>(value));

        return FBOMResult::FBOM_OK;
    }
};

#endif

template <class T>
struct HypDataHelperDecl<T, std::enable_if_t<std::is_enum_v<T>>>
{
};

template <class T>
struct HypDataHelper<T, std::enable_if_t<std::is_enum_v<T>>> : HypDataHelper<std::underlying_type_t<T>>
{
    HYP_FORCE_INLINE bool Is(T value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE constexpr bool Is(std::underlying_type_t<T>) const
    {
        return true;
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<OtherT, T> && !std::is_same_v<OtherT, std::underlying_type_t<T>>>>
    HYP_FORCE_INLINE constexpr bool Is(OtherT value) const
    {
        return std::is_fundamental_v<OtherT>;
    }

    HYP_FORCE_INLINE constexpr T Get(T value) const
    {
        return value;
    }

    HYP_FORCE_INLINE constexpr T Get(std::underlying_type<T> value) const
    {
        return static_cast<T>(value);
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<OtherT, T> && !std::is_same_v<OtherT, std::underlying_type<T>>>>
    HYP_FORCE_INLINE constexpr T Get(OtherT value) const
    {
        return static_cast<T>(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, T value) const
    {
        HypDataHelper<std::underlying_type_t<T>>::Set(hypData, static_cast<std::underlying_type_t<T>>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(T value, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        out = FBOMData(static_cast<std::underlying_type_t<T>>(value), flags);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        std::underlying_type_t<T> value;

        if (FBOMResult err = data.Read(&value))
        {
            return err;
        }

        out = HypData(value);

        return FBOMResult::FBOM_OK;
    }
};

/* void pointer specialization - only meant for runtime, non-serializable. */
template <>
struct HypDataHelperDecl<void*>
{
};

template <>
struct HypDataHelper<void*>
{
    using StorageType = void*;
    using ConvertibleFrom = Tuple<AnyRef, AnyHandle, RC<void>>;

    HYP_FORCE_INLINE bool Is(void* value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE bool Is(const AnyRef& value) const
    {
        return true;
    }

    HYP_FORCE_INLINE bool Is(const AnyHandle& value) const
    {
        return true;
    }

    HYP_FORCE_INLINE bool Is(const RC<void>& value) const
    {
        return true;
    }

    HYP_FORCE_INLINE constexpr void* Get(void* value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void* Get(const AnyRef& value) const
    {
        return value.GetPointer();
    }

    HYP_FORCE_INLINE void* Get(const AnyHandle& value) const
    {
        return value.ToRef().GetPointer();
    }

    HYP_FORCE_INLINE void* Get(const RC<void>& value) const
    {
        return value.Get();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, void* value) const
    {
        hypData.Set_Internal(value);
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(void* value, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        return { FBOMResult::FBOM_ERR, "Cannot serialize a user pointer!" };
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        return { FBOMResult::FBOM_ERR, "Cannot deserialize a user pointer!" };
    }
};

template <class T>
struct HypDataHelperDecl<EnumFlags<T>>
{
};

template <class T>
struct HypDataHelper<EnumFlags<T>> : HypDataHelper<typename EnumFlags<T>::UnderlyingType>
{
    using ConvertibleFrom = Tuple<typename EnumFlags<T>::UnderlyingType>;

    HYP_FORCE_INLINE bool Is(typename EnumFlags<T>::UnderlyingType value) const
    {
        return true;
    }

    HYP_FORCE_INLINE constexpr EnumFlags<T> Get(EnumFlags<T> value) const
    {
        return value;
    }

    HYP_FORCE_INLINE constexpr EnumFlags<T> Get(typename EnumFlags<T>::UnderlyingType value) const
    {
        return static_cast<EnumFlags<T>>(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, EnumFlags<T> value) const
    {
        HypDataHelper<typename EnumFlags<T>::UnderlyingType>::Set(hypData, static_cast<typename EnumFlags<T>::UnderlyingType>(value));
    }
};

template <>
struct HypDataHelperDecl<ObjIdBase>
{
};

template <>
struct HypDataHelper<ObjIdBase>
{
    using StorageType = ObjIdBase;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const ObjIdBase& value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE constexpr ObjIdBase& Get(ObjIdBase& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE const ObjIdBase& Get(const ObjIdBase& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const ObjIdBase& value) const
    {
        hypData.Set_Internal(value);
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(ObjIdBase value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        outData = FBOMData::FromStruct<ObjIdBase>(value, flags);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        ObjIdBase value;

        if (FBOMResult err = data.ReadStruct<ObjIdBase>(&value))
        {
            return err;
        }

        out = HypData(value);

        return FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<ObjId<T>>
{
};

template <class T>
struct HypDataHelper<ObjId<T>> : HypDataHelper<ObjIdBase>
{
    using ConvertibleFrom = Tuple<AnyHandle>;

    HYP_FORCE_INLINE bool Is(const ObjIdBase& value) const
    {
        return true; // can't do anything more to check as ObjIdBase doesn't hold type info.
    }

    HYP_FORCE_INLINE bool Is(const AnyHandle& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE ObjId<T> Get(ObjIdBase value) const
    {
        return ObjId<T>(value);
    }

    HYP_FORCE_INLINE ObjId<T> Get(const AnyHandle& value) const
    {
        return ObjId<T>(value.Id());
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const ObjId<T>& value) const
    {
        HypDataHelper<ObjIdBase>::Set(hypData, static_cast<const ObjIdBase&>(value));
    }
};

template <>
struct HypDataHelperDecl<HypClassRef>
{
};

template <>
struct HypDataHelper<HypClassRef>
{
    using StorageType = HypClassRef;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const HypClassRef& value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE HypClassRef& Get(HypClassRef& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE const HypClassRef& Get(const HypClassRef& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const HypClassRef& value) const
    {
        hypData.Set_Internal(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, HypClassRef&& value) const
    {
        hypData.Set_Internal(std::move(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(HypClassRef value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        return { FBOMResult::FBOM_ERR, "Cannot serialize HypClassRef!" };
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        return { FBOMResult::FBOM_ERR, "Cannot deserialize HypClassRef!" };
    }
};

template <>
struct HypDataHelperDecl<AnyHandle>
{
};

template <>
struct HypDataHelper<AnyHandle>
{
    using StorageType = AnyHandle;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const AnyHandle& value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE AnyHandle& Get(AnyHandle& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE const AnyHandle& Get(const AnyHandle& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const AnyHandle& value) const
    {
        hypData.Set_Internal(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, AnyHandle&& value) const
    {
        hypData.Set_Internal(std::move(value));
    }

    static FBOMResult Serialize(const AnyHandle& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        if (!value.IsValid())
        {
            // unset
            outData = FBOMData();

            return FBOMResult::FBOM_OK;
        }

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(value.GetTypeId());

        if (!marshal)
        {
            return FBOMResult { FBOMResult::FBOM_ERR, "No marshal defined for handle type" };
        }

        FBOMObject object;

        if (FBOMResult err = marshal->Serialize(value.ToRef(), object))
        {
            return err;
        }

        outData = FBOMData::FromObject(std::move(object));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        if (!data)
        {
            out = HypData(AnyHandle {});

            return FBOMResult::FBOM_OK;
        }

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(data.GetType().GetNativeTypeId());

        if (!marshal)
        {
            return FBOMResult { FBOMResult::FBOM_ERR, "No marshal defined for handle type" };
        }

        FBOMObject object;

        if (FBOMResult err = data.ReadObject(context, object))
        {
            return err;
        }

        if (FBOMResult err = marshal->Deserialize(context, object, out))
        {
            return err;
        }

        return FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<Handle<T>>
{
};

template <class T>
struct HypDataHelper<Handle<T>> : HypDataHelper<AnyHandle>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const AnyHandle& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE Handle<T>& Get(AnyHandle& value) const
    {
        return *reinterpret_cast<Handle<T>*>(&value);
    }

    HYP_FORCE_INLINE const Handle<T>& Get(const AnyHandle& value) const
    {
        return *reinterpret_cast<const Handle<T>*>(&value);
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const Handle<T>& value) const
    {
        HypDataHelper<AnyHandle>::Set(hypData, AnyHandle(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, Handle<T>&& value) const
    {
        HypDataHelper<AnyHandle>::Set(hypData, AnyHandle(std::move(value)));
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(TypeId::ForType<T>());

        if (!marshal)
        {
            return FBOMResult { FBOMResult::FBOM_ERR, "No marshal defined for handle type" };
        }

        if (!data)
        {
            out = HypData(Handle<T> {});

            return FBOMResult::FBOM_OK;
        }

        FBOMObject object;

        if (FBOMResult err = data.ReadObject(context, object))
        {
            return err;
        }

        if (FBOMResult err = marshal->Deserialize(context, object, out))
        {
            return err;
        }

        return FBOMResult::FBOM_OK;
    }
};

template <>
struct HypDataHelperDecl<RC<void>>
{
};

template <>
struct HypDataHelper<RC<void>>
{
    using StorageType = RC<void>;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const RC<void>& value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE RC<void>& Get(RC<void>& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE const RC<void>& Get(const RC<void>& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const RC<void>& value) const
    {
        hypData.Set_Internal(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, RC<void>&& value) const
    {
        hypData.Set_Internal(std::move(value));
    }

    static FBOMResult Serialize(const RC<void>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(value.GetTypeId());

        if (!marshal)
        {
            return FBOMResult { FBOMResult::FBOM_ERR, "No marshal registered for type" };
        }

        if (!value)
        {
            // unset
            outData = FBOMData();

            return FBOMResult::FBOM_OK;
        }

        FBOMObject object;

        if (FBOMResult err = marshal->Serialize(value.ToRef(), object))
        {
            return err;
        }

        outData = FBOMData::FromObject(std::move(object));

        return FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<RC<T>, std::enable_if_t<!std::is_void_v<T>>>
{
};

template <class T>
struct HypDataHelper<RC<T>, std::enable_if_t<!std::is_void_v<T>>> : HypDataHelper<RC<void>>
{
    HYP_FORCE_INLINE bool Is(const RC<void>& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE RC<T>& Get(RC<void>& value) const
    {
        return *reinterpret_cast<RC<T>*>(&value);
    }

    HYP_FORCE_INLINE const RC<T>& Get(const RC<void>& value) const
    {
        return *reinterpret_cast<const RC<T>*>(&value);
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const RC<T>& value) const
    {
        HypDataHelper<RC<void>>::Set(hypData, value);
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, RC<T>&& value) const
    {
        HypDataHelper<RC<void>>::Set(hypData, std::move(value));
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(TypeId::ForType<T>());

        if (!marshal)
        {
            return FBOMResult { FBOMResult::FBOM_ERR, "No marshal defined for type" };
        }

        if (!data)
        {
            out = HypData(RC<T> {});

            return FBOMResult::FBOM_OK;
        }

        FBOMObject object;

        if (FBOMResult err = data.ReadObject(context, object))
        {
            return err;
        }

        if (FBOMResult err = marshal->Deserialize(context, object, out))
        {
            return err;
        }

        return FBOMResult::FBOM_OK;
    }
};

template <>
struct HypDataHelperDecl<AnyRef>
{
};

template <>
struct HypDataHelper<AnyRef>
{
    using StorageType = AnyRef;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const AnyRef& value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE AnyRef& Get(AnyRef& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE const AnyRef& Get(const AnyRef& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const AnyRef& value) const
    {
        hypData.Set_Internal(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, AnyRef&& value) const
    {
        hypData.Set_Internal(std::move(value));
    }

    static FBOMResult Serialize(const AnyRef& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(value.GetTypeId());

        if (!marshal)
        {
            return FBOMResult { FBOMResult::FBOM_ERR, "No marshal registered for type" };
        }

        if (!value.HasValue())
        {
            // unset
            outData = FBOMData();

            return FBOMResult::FBOM_OK;
        }

        FBOMObject object;

        if (FBOMResult err = marshal->Serialize(value, object))
        {
            return err;
        }

        outData = FBOMData::FromObject(std::move(object));

        return FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<T*, std::enable_if_t<!isConstPointer<T*> && !std::is_same_v<T*, void*>>>
{
};

template <class T>
struct HypDataHelper<T*, std::enable_if_t<!isConstPointer<T*> && !std::is_same_v<T*, void*>>> : HypDataHelper<AnyRef>
{
    using ConvertibleFrom = Tuple<AnyHandle, RC<void>>;

    HYP_FORCE_INLINE bool Is(const AnyRef& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE bool Is(const AnyHandle& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE bool Is(const RC<void>& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE T* Get(const AnyRef& value) const
    {
        HYP_CORE_ASSERT(value.Is<T>());

        return static_cast<T*>(value.GetPointer());
    }

    HYP_FORCE_INLINE T* Get(const AnyHandle& value) const
    {
        HYP_CORE_ASSERT(value.Is<T>());

        return value.TryGet<T>();
    }

    HYP_FORCE_INLINE T* Get(const RC<void>& value) const
    {
        HYP_CORE_ASSERT(value.Is<T>());

        return value.CastUnsafe_Internal<T>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, T* value) const
    {
        HypDataHelper<AnyRef>::Set(hypData, AnyRef(TypeId::ForType<T>(), value));
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(TypeId::ForType<T>());

        if (!marshal)
        {
            return FBOMResult { FBOMResult::FBOM_ERR, "No marshal defined for type" };
        }

        if (!data)
        {
            out = HypData(static_cast<T*>(nullptr));

            return FBOMResult::FBOM_OK;
        }

        FBOMObject object;

        if (FBOMResult err = data.ReadObject(context, object))
        {
            return err;
        }

        if (FBOMResult err = marshal->Deserialize(context, object, out))
        {
            return err;
        }

        return FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<const T*, std::enable_if_t<!std::is_same_v<T*, void*>>>
{
};

template <class T>
struct HypDataHelper<const T*, std::enable_if_t<!std::is_same_v<T*, void*>>> : HypDataHelper<T*>
{
    HYP_FORCE_INLINE const T* Get(const ConstAnyRef& value) const
    {
        return HypDataHelper<T*>::Get(AnyRef(value.GetTypeId(), const_cast<void*>(value.GetPointer())));
    }

    HYP_FORCE_INLINE const T* Get(const AnyRef& value) const
    {
        return HypDataHelper<T*>::Get(value);
    }

    HYP_FORCE_INLINE const T* Get(const AnyHandle& value) const
    {
        return HypDataHelper<T*>::Get(value);
    }

    HYP_FORCE_INLINE const T* Get(const RC<void>& value) const
    {
        return HypDataHelper<T*>::Get(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const T* value) const
    {
        HypDataHelper<T*>::Set(hypData, const_cast<T*>(value));
    }
};

template <>
struct HypDataHelperDecl<Any>
{
};

template <>
struct HypDataHelper<Any>
{
    using StorageType = Any;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE Any& Get(Any& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE const Any& Get(const Any& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, Any&& value) const
    {
        hypData.Set_Internal(std::move(value));
    }

    static FBOMResult Serialize(const Any& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(value.GetTypeId());

        if (!marshal)
        {
            return FBOMResult { FBOMResult::FBOM_ERR, "No marshal registered for type" };
        }

        if (!value.HasValue())
        {
            // unset
            outData = FBOMData();

            return FBOMResult::FBOM_OK;
        }

        FBOMObject object;

        if (FBOMResult err = marshal->Serialize(value.ToRef(), object))
        {
            return err;
        }

        outData = FBOMData::FromObject(std::move(object));

        return FBOMResult::FBOM_OK;
    }
};

template <>
struct HypDataHelperDecl<HypDataArray>
{
};

template <>
struct HypDataHelper<HypDataArray> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<HypDataArray>();
    }

    HYP_FORCE_INLINE HypDataArray& Get(const Any& value) const
    {
        return value.Get<HypDataArray>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const HypDataArray& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<HypDataArray>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, HypDataArray&& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<HypDataArray>(std::move(value)));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const HypDataArray& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        HypDataArray::SerializeFunction serializeFunction = value.serializeFunction;
        if (!serializeFunction)
        {
            return { FBOMResult::FBOM_ERR, "Cannot serialize HypDataArray without a serialize function!" };
        }

        if (FBOMResult err = serializeFunction(value, outData, flags))
        {
            return err;
        }

        return FBOMResult::FBOM_OK;
    }
};

template <>
struct HypDataHelperDecl<HypData_UserData128>
{
};

template <>
struct HypDataHelper<HypData_UserData128>
{
    using StorageType = HypData_UserData128;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const HypData_UserData128& value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE HypData_UserData128& Get(HypData_UserData128& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE const HypData_UserData128& Get(const HypData_UserData128& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const HypData_UserData128& value) const
    {
        hypData.Set_Internal(value);
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const HypData_UserData128& value, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        return { FBOMResult::FBOM_ERR, "Cannot serialize user data!" };
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        return { FBOMResult::FBOM_ERR, "Cannot deserialize user data!" };
    }
};

template <int StringType>
struct HypDataHelperDecl<containers::String<StringType>>
{
};

template <int StringType>
struct HypDataHelper<containers::String<StringType>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<containers::String<StringType>>();
    }

    HYP_FORCE_INLINE containers::String<StringType>& Get(Any& value) const
    {
        return value.Get<containers::String<StringType>>();
    }

    HYP_FORCE_INLINE const containers::String<StringType>& Get(const Any& value) const
    {
        return value.Get<containers::String<StringType>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const containers::String<StringType>& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<containers::String<StringType>>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, containers::String<StringType>&& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<containers::String<StringType>>(std::move(value)));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const containers::String<StringType>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData::FromString(value);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        containers::String<StringType> result;

        if (FBOMResult err = data.ReadString(result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

template <int StringType>
struct HypDataHelperDecl<utilities::StringView<StringType>>
{
};

template <int StringType>
struct HypDataHelper<utilities::StringView<StringType>> : HypDataHelper<containers::String<StringType>>
{
    using ConvertibleFrom = Tuple<containers::String<StringType>>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return HypDataHelper<containers::String<StringType>>::Is(value);
    }

    HYP_FORCE_INLINE utilities::StringView<StringType> Get(const Any& value) const
    {
        return HypDataHelper<containers::String<StringType>>::Get(value);
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const utilities::StringView<StringType>& value) const
    {
        HypDataHelper<containers::String<StringType>>::Set(hypData, value);
    }
};

template <>
struct HypDataHelperDecl<const char*>
{
};

template <>
struct HypDataHelper<const char*> : HypDataHelper<String>
{
    using ConvertibleFrom = Tuple<String>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return HypDataHelper<String>::Is(value);
    }

    HYP_FORCE_INLINE const char* Get(const Any& value) const
    {
        return HypDataHelper<String>::Get(value).Data();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const char* value) const
    {
        HypDataHelper<String>::Set(hypData, String(value));
    }
};

template <>
struct HypDataHelperDecl<FilePath>
{
};

template <>
struct HypDataHelper<FilePath> : HypDataHelper<String>
{
    HYP_FORCE_INLINE FilePath Get(const Any& value) const
    {
        return value.Get<String>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const FilePath& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<String>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, FilePath&& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<String>(std::move(value)));
    }
};

template <>
struct HypDataHelperDecl<Name>
{
};

template <>
struct HypDataHelper<Name>
{
    using StorageType = Name;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Name&) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();

        return true;
    }

    HYP_FORCE_INLINE constexpr Name& Get(Name& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE constexpr const Name& Get(const Name& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const Name& value) const
    {
        hypData.Set_Internal(value);
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const Name& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData::FromString(ANSIString(value.LookupString()));

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        ANSIString str;

        if (FBOMResult err = data.ReadString(str))
        {
            return err;
        }

        out = HypData(CreateNameFromDynamicString(str));

        return { FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<WeakName>
{
};

template <>
struct HypDataHelper<WeakName>
{
    using StorageType = Name;
    using ConvertibleFrom = Tuple<Name>;

    HYP_FORCE_INLINE constexpr bool Is(const WeakName&) const
    {
        return true;
    }

    HYP_FORCE_INLINE constexpr WeakName& Get(WeakName& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE constexpr const WeakName& Get(const WeakName& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE constexpr bool Is(const Name&) const
    {
        return true;
    }

    HYP_FORCE_INLINE WeakName& Get(Name& value) const
    {
        return *reinterpret_cast<WeakName*>(&value);
    }

    HYP_FORCE_INLINE const WeakName& Get(const Name& value) const
    {
        return *reinterpret_cast<const WeakName*>(&value);
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const WeakName& value) const
    {
        hypData.Set_Internal(*reinterpret_cast<const Name*>(&value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const WeakName& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        return HypDataHelper<Name>::Serialize(*reinterpret_cast<const Name*>(&value), outData, flags);
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HypData nameData;

        if (FBOMResult err = HypDataHelper<Name>::Deserialize(context, data, nameData))
        {
            return err;
        }

        out = HypData(WeakName(nameData.Get<Name>()));

        return { FBOMResult::FBOM_OK };
    }
};

template <class T, class AllocatorType>
struct HypDataHelperDecl<Array<T, AllocatorType>, std::enable_if_t<!std::is_const_v<T> && !isHypData<T>>>
{
};

template <class T, class AllocatorType>
struct HypDataHelper<Array<T, AllocatorType>, std::enable_if_t<!std::is_const_v<T> && !isHypData<T>>> : HypDataHelper<HypDataArray>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        if (const HypDataArray* arr = value.TryGet<HypDataArray>())
        {
            return arr->internalArray.Is<Array<T, AllocatorType>>();
        }

        return false;
    }

    HYP_FORCE_INLINE Array<T, AllocatorType>& Get(const Any& value) const
    {
        return value.Get<HypDataArray>().internalArray.Get<Array<T, AllocatorType>>();
    }

    HYP_FORCE_INLINE bool Is(const HypDataArray& value) const
    {
        return value.internalArray.Is<Array<T, AllocatorType>>();
    }

    HYP_FORCE_INLINE Array<T, AllocatorType>& Get(const HypDataArray& value) const
    {
        return value.internalArray.Get<Array<T, AllocatorType>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const Array<T, AllocatorType>& value) const
    {
        HypDataHelper<HypDataArray>::Set(hypData, HypDataArray(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, Array<T, AllocatorType>&& value) const
    {
        HypDataHelper<HypDataArray>::Set(hypData, HypDataArray(std::move(value)));
    }

    static FBOMResult Serialize(const Array<T, AllocatorType>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const SizeType size = value.Size();

        if (size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            outData = FBOMData::FromArray(FBOMArray(HypDataPlaceholderSerializedType<T>::Get()));

            return FBOMResult::FBOM_OK;
        }

        Array<FBOMData> elements;
        elements.Resize(size);

        for (SizeType i = 0; i < size; i++)
        {
            if (FBOMResult err = HypDataHelper<T>::Serialize(value[i], elements[i]))
            {
                return err;
            }
        }

        outData = FBOMData::FromArray(FBOMArray(elements[0].GetType(), std::move(elements)));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        FBOMArray array { FBOMUnset() };

        if (FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        const SizeType size = array.Size();

        Array<T, AllocatorType> result;
        result.Reserve(size);

        for (SizeType i = 0; i < size; i++)
        {
            HypData element;

            if (FBOMResult err = HypDataHelper<T>::Deserialize(context, array.GetElement(i), element))
            {
                return err;
            }

            result.PushBack(std::move(element.Get<T>()));
        }

        HypDataHelper<Array<T, AllocatorType>> {}.Set(out, std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

template <class T, SizeType Size>
struct HypDataHelperDecl<FixedArray<T, Size>>
{
};

template <class T, SizeType Size>
struct HypDataHelper<FixedArray<T, Size>, std::enable_if_t<!std::is_const_v<T>>> : HypDataHelper<HypDataArray>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        if (const HypDataArray* arr = value.TryGet<HypDataArray>())
        {
            return arr->internalArray.Is<FixedArray<T, Size>>();
        }

        return false;
    }

    HYP_FORCE_INLINE FixedArray<T, Size>& Get(const Any& value) const
    {
        return value.Get<HypDataArray>().internalArray.Get<FixedArray<T, Size>>();
    }

    HYP_FORCE_INLINE bool Is(const HypDataArray& value) const
    {
        return value.internalArray.Is<FixedArray<T, Size>>();
    }

    HYP_FORCE_INLINE FixedArray<T, Size>& Get(const HypDataArray& value) const
    {
        return value.internalArray.Get<FixedArray<T, Size>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const FixedArray<T, Size>& value) const
    {
        HypDataHelper<HypDataArray>::Set(hypData, HypDataArray(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, FixedArray<T, Size>&& value) const
    {
        HypDataHelper<HypDataArray>::Set(hypData, HypDataArray(std::move(value)));
    }

    static FBOMResult Serialize(const FixedArray<T, Size>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        if (Size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            outData = FBOMData::FromArray(FBOMArray(HypDataPlaceholderSerializedType<T>::Get()));

            return FBOMResult::FBOM_OK;
        }

        Array<FBOMData> elements;
        elements.Resize(Size);

        for (SizeType i = 0; i < Size; i++)
        {
            if (FBOMResult err = HypDataHelper<T>::Serialize(value[i], elements[i]))
            {
                return err;
            }
        }

        outData = FBOMData::FromArray(FBOMArray(elements[0].GetType(), std::move(elements)));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        FBOMArray array { FBOMUnset() };

        if (FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        if (Size != array.Size())
        {
            return { FBOMResult::FBOM_ERR, "Failed to deserialize array - size does not match expected size" };
        }

        FixedArray<T, Size> result;

        for (SizeType i = 0; i < Size; i++)
        {
            HypData element;

            if (FBOMResult err = HypDataHelper<T>::Deserialize(context, array.GetElement(i), element))
            {
                return err;
            }

            result[i] = std::move(element.Get<T>());
        }

        HypDataHelper<FixedArray<T, Size>> {}.Set(out, std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

template <class T, SizeType Size>
struct HypDataHelperDecl<T[Size]>
{
};

template <class T, SizeType Size>
struct HypDataHelper<T[Size], std::enable_if_t<!std::is_const_v<T>>> : HypDataHelper<FixedArray<T, Size>>
{
    using ConvertibleFrom = Tuple<FixedArray<T, Size>>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return HypDataHelper<FixedArray<T, Size>>::Is(value);
    }

    HYP_FORCE_INLINE FixedArray<T, Size>& Get(const Any& value) const
    {
        return HypDataHelper<FixedArray<T, Size>>::Get(value);
    }

    HYP_FORCE_INLINE bool Is(const HypDataArray& value) const
    {
        return value.internalArray.Is<FixedArray<T, Size>>();
    }

    HYP_FORCE_INLINE FixedArray<T, Size>& Get(const HypDataArray& value) const
    {
        return value.internalArray.Get<FixedArray<T, Size>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const T (&value)[Size]) const
    {
        HypDataHelper<FixedArray<T, Size>>::Set(hypData, MakeFixedArray(value));
    }

    static FBOMResult Serialize(const T (&value)[Size], FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        if (Size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            outData = FBOMData::FromArray(FBOMArray(HypDataPlaceholderSerializedType<T>::Get()));

            return FBOMResult::FBOM_OK;
        }

        Array<FBOMData> elements;
        elements.Resize(Size);

        for (SizeType i = 0; i < Size; i++)
        {
            if (FBOMResult err = HypDataHelper<T>::Serialize(value[i], elements[i]))
            {
                return err;
            }
        }

        outData = FBOMData::FromArray(FBOMArray(elements[0].GetType(), std::move(elements)));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        FBOMArray array { FBOMUnset() };

        if (FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        if (Size != array.Size())
        {
            return { FBOMResult::FBOM_ERR, "Failed to deserialize array - size does not match expected size" };
        }

        FixedArray<T, Size> result;

        for (SizeType i = 0; i < Size; i++)
        {
            HypData element;

            if (FBOMResult err = HypDataHelper<T>::Deserialize(context, array.GetElement(i), element))
            {
                return err;
            }

            result[i] = std::move(element.Get<T>());
        }

        HypDataHelper<FixedArray<T, Size>> {}.Set(out, std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

template <class K, class V>
struct HypDataHelperDecl<Pair<K, V>>
{
};

template <class K, class V>
struct HypDataHelper<Pair<K, V>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Pair<K, V>>();
    }

    HYP_FORCE_INLINE Pair<K, V>& Get(const Any& value) const
    {
        return value.Get<Pair<K, V>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const Pair<K, V>& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<Pair<K, V>>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, Pair<K, V>&& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<Pair<K, V>>(std::move(value)));
    }

    static FBOMResult Serialize(const Pair<K, V>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        FBOMData firstData;
        FBOMData secondData;

        if (FBOMResult err = HypDataHelper<K>::Serialize(value.first, firstData))
        {
            return err;
        }

        if (FBOMResult err = HypDataHelper<V>::Serialize(value.second, secondData))
        {
            return err;
        }

        FBOMObject object(FBOMObjectType(TypeWrapper<Pair<K, V>> {}, FBOMTypeFlags::DEFAULT, FBOMBaseObjectType()));
        object.SetProperty("Key", std::move(firstData));
        object.SetProperty("Value", std::move(secondData));

        outData = FBOMData::FromObject(std::move(object));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        FBOMObject object;

        if (FBOMResult err = data.ReadObject(context, object))
        {
            return err;
        }

        HypData first;
        HypData second;

        if (FBOMResult err = HypDataHelper<K>::Deserialize(context, object.GetProperty("Key"), first))
        {
            return err;
        }

        if (FBOMResult err = HypDataHelper<V>::Deserialize(context, object.GetProperty("Value"), second))
        {
            return err;
        }

        HypDataHelper<Pair<K, V>> {}.Set(out, Pair<K, V> { first.Get<K>(), second.Get<V>() });

        return { FBOMResult::FBOM_OK };
    }
};

template <class K, class V>
struct HypDataHelperDecl<HashMap<K, V>>
{
};

template <class K, class V>
struct HypDataHelper<HashMap<K, V>> : HypDataHelper<HypDataArray>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        if (const HypDataArray* array = value.TryGet<HypDataArray>())
        {
            return array->internalArray.Is<HashMap<K, V>>();
        }

        return false;
    }

    HYP_FORCE_INLINE HashMap<K, V>& Get(const Any& value) const
    {
        return value.Get<HypDataArray>().internalArray.Get<HashMap<K, V>>();
    }

    HYP_FORCE_INLINE bool Is(const HypDataArray& value) const
    {
        return value.internalArray.Is<HashMap<K, V>>();
    }

    HYP_FORCE_INLINE HashMap<K, V>& Get(const HypDataArray& value) const
    {
        return value.internalArray.Get<HashMap<K, V>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const HashMap<K, V>& value) const
    {
        HypDataHelper<HypDataArray>::Set(hypData, HypDataArray(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, HashMap<K, V>&& value) const
    {
        HypDataHelper<HypDataArray>::Set(hypData, HypDataArray(std::move(value)));
    }

    static FBOMResult Serialize(const HashMap<K, V>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const SizeType size = value.Size();

        if (size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            outData = FBOMData::FromArray(FBOMArray(HypDataPlaceholderSerializedType<Pair<K, V>>::Get()));

            return FBOMResult::FBOM_OK;
        }

        Array<FBOMData> elements;
        elements.Reserve(size);

        uint32 elementIndex = 0;

        for (const Pair<K, V>& pair : value)
        {
            FBOMData& element = elements.EmplaceBack();

            if (FBOMResult err = HypDataHelper<Pair<K, V>>::Serialize(pair, element))
            {
                return err;
            }
        }

        outData = FBOMData::FromArray(FBOMArray(elements[0].GetType(), std::move(elements)));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        FBOMArray array { FBOMUnset() };

        if (FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        const SizeType size = array.Size();

        HashMap<K, V> result;

        for (SizeType i = 0; i < size; i++)
        {
            HypData element;

            if (FBOMResult err = HypDataHelper<Pair<K, V>>::Deserialize(context, array.GetElement(i), element))
            {
                return err;
            }

            result.Insert(std::move(element.Get<Pair<K, V>>()));
        }

        HypDataHelper<HashMap<K, V>> {}.Set(out, std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

template <class ValueType, auto KeyByFunction>
struct HypDataHelperDecl<HashSet<ValueType, KeyByFunction>>
{
};

template <class ValueType, auto KeyByFunction>
struct HypDataHelper<HashSet<ValueType, KeyByFunction>> : HypDataHelper<HypDataArray>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        if (const HypDataArray* array = value.TryGet<HypDataArray>())
        {
            return array->internalArray.Is<HashSet<ValueType, KeyByFunction>>();
        }

        return false;
    }

    HYP_FORCE_INLINE HashSet<ValueType, KeyByFunction>& Get(const Any& value) const
    {
        return value.Get<HypDataArray>().internalArray.Get<HashSet<ValueType, KeyByFunction>>();
    }

    HYP_FORCE_INLINE bool Is(const HypDataArray& value) const
    {
        return value.internalArray.Is<HashSet<ValueType, KeyByFunction>>();
    }

    HYP_FORCE_INLINE HashSet<ValueType, KeyByFunction>& Get(const HypDataArray& value) const
    {
        return value.internalArray.Get<HashSet<ValueType, KeyByFunction>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const HashSet<ValueType, KeyByFunction>& value) const
    {
        HypDataHelper<HypDataArray>::Set(hypData, HypDataArray(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, HashSet<ValueType, KeyByFunction>&& value) const
    {
        HypDataHelper<HypDataArray>::Set(hypData, HypDataArray(std::move(value)));
    }

    static FBOMResult Serialize(const HashSet<ValueType, KeyByFunction>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const SizeType size = value.Size();

        if (size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            outData = FBOMData::FromArray(FBOMArray(HypDataPlaceholderSerializedType<ValueType>::Get()));

            return FBOMResult::FBOM_OK;
        }

        Array<FBOMData> elements;
        elements.Reserve(size);

        uint32 elementIndex = 0;

        for (const ValueType& value : value)
        {
            FBOMData& element = elements.EmplaceBack();

            if (FBOMResult err = HypDataHelper<ValueType>::Serialize(value, element))
            {
                return err;
            }
        }

        outData = FBOMData::FromArray(FBOMArray(elements[0].GetType(), std::move(elements)));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        FBOMArray array { FBOMUnset() };

        if (FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        const SizeType size = array.Size();

        HashSet<ValueType, KeyByFunction> result;

        for (SizeType i = 0; i < size; i++)
        {
            HypData element;

            if (FBOMResult err = HypDataHelper<ValueType>::Deserialize(context, array.GetElement(i), element))
            {
                return err;
            }

            result.Insert(std::move(element.Get<ValueType>()));
        }

        HypDataHelper<HashSet<ValueType, KeyByFunction>> {}.Set(out, std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

template <class T>
struct HypDataHelperDecl<LinkedList<T>>
{
};

template <class T>
struct HypDataHelper<LinkedList<T>> : HypDataHelper<HypDataArray>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        if (const HypDataArray* array = value.TryGet<HypDataArray>())
        {
            return array->internalArray.Is<LinkedList<T>>();
        }

        return false;
    }

    HYP_FORCE_INLINE LinkedList<T>& Get(const Any& value) const
    {
        return value.Get<HypDataArray>().internalArray.Get<LinkedList<T>>();
    }

    HYP_FORCE_INLINE bool Is(const HypDataArray& value) const
    {
        return value.internalArray.Is<LinkedList<T>>();
    }

    HYP_FORCE_INLINE LinkedList<T>& Get(const HypDataArray& value) const
    {
        return value.internalArray.Get<LinkedList<T>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const LinkedList<T>& value) const
    {
        HypDataHelper<HypDataArray>::Set(hypData, HypDataArray(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, LinkedList<T>&& value) const
    {
        HypDataHelper<HypDataArray>::Set(hypData, HypDataArray(std::move(value)));
    }

    static FBOMResult Serialize(const LinkedList<T>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const SizeType size = value.Size();

        if (size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            outData = FBOMData::FromArray(FBOMArray(HypDataPlaceholderSerializedType<T>::Get()));

            return FBOMResult::FBOM_OK;
        }

        Array<FBOMData> elements;
        elements.Reserve(size);

        uint32 elementIndex = 0;

        for (const T& value : value)
        {
            FBOMData& element = elements.EmplaceBack();

            if (FBOMResult err = HypDataHelper<T>::Serialize(value, element))
            {
                return err;
            }
        }

        outData = FBOMData::FromArray(FBOMArray(elements[0].GetType(), std::move(elements)));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        FBOMArray array { FBOMUnset() };

        if (FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        const SizeType size = array.Size();

        LinkedList<T> result;

        for (SizeType i = 0; i < size; i++)
        {
            HypData element;

            if (FBOMResult err = HypDataHelper<T>::Deserialize(context, array.GetElement(i), element))
            {
                return err;
            }

            result.PushBack(std::move(element.Get<T>()));
        }

        HypDataHelper<LinkedList<T>> {}.Set(out, std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

// fwd decl for math types
namespace math {
template <class T>
struct Vec2;

template <class T>
struct Vec3;

template <class T>
struct Vec4;

} // namespace math

template <class T>
struct HypDataHelperDecl<math::Vec2<T>>
{
};

template <class T>
struct HypDataHelper<math::Vec2<T>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<math::Vec2<T>>();
    }

    HYP_FORCE_INLINE math::Vec2<T>& Get(const Any& value) const
    {
        return value.Get<math::Vec2<T>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const math::Vec2<T>& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<math::Vec2<T>>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const math::Vec2<T>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData(value);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        math::Vec2<T> result;

        if (FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

template <class T>
struct HypDataHelperDecl<math::Vec3<T>>
{
};

template <class T>
struct HypDataHelper<math::Vec3<T>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<math::Vec3<T>>();
    }

    HYP_FORCE_INLINE math::Vec3<T>& Get(const Any& value) const
    {
        return value.Get<math::Vec3<T>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const math::Vec3<T>& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<math::Vec3<T>>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const math::Vec3<T>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData(value);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        math::Vec3<T> result;

        if (FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

template <class T>
struct HypDataHelperDecl<math::Vec4<T>>
{
};

template <class T>
struct HypDataHelper<math::Vec4<T>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<math::Vec4<T>>();
    }

    HYP_FORCE_INLINE math::Vec4<T>& Get(const Any& value) const
    {
        return value.Get<math::Vec4<T>>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const math::Vec4<T>& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<math::Vec4<T>>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const math::Vec4<T>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData(value);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        math::Vec4<T> result;

        if (FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<Matrix3>
{
};

template <>
struct HypDataHelper<Matrix3> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Matrix3>();
    }

    HYP_FORCE_INLINE Matrix3& Get(const Any& value) const
    {
        return value.Get<Matrix3>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const Matrix3& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<Matrix3>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const Matrix3& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData(value);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        Matrix3 result;

        if (FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<Matrix4>
{
};

template <>
struct HypDataHelper<Matrix4> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Matrix4>();
    }

    HYP_FORCE_INLINE Matrix4& Get(const Any& value) const
    {
        return value.Get<Matrix4>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const Matrix4& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<Matrix4>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const Matrix4& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData(value);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        Matrix4 result;

        if (FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<Quaternion>
{
};

template <>
struct HypDataHelper<Quaternion> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Quaternion>();
    }

    HYP_FORCE_INLINE Quaternion& Get(const Any& value) const
    {
        return value.Get<Quaternion>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const Quaternion& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<Quaternion>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const Quaternion& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData(value);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        Quaternion result;

        if (FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<Uuid>
{
};

template <>
struct HypDataHelper<Uuid> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Uuid>();
    }

    HYP_FORCE_INLINE Uuid& Get(const Any& value) const
    {
        return value.Get<Uuid>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const Uuid& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<Uuid>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const Uuid& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData::FromStruct<Uuid>(value, flags);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        Uuid result;

        if (FBOMResult err = data.ReadStruct<Uuid>(&result))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<ByteBuffer>
{
};

template <>
struct HypDataHelper<ByteBuffer> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<ByteBuffer>();
    }

    HYP_FORCE_INLINE ByteBuffer& Get(const Any& value) const
    {
        return value.Get<ByteBuffer>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const ByteBuffer& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<ByteBuffer>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, ByteBuffer&& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<ByteBuffer>(std::move(value)));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const ByteBuffer& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData::FromByteBuffer(value, flags);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        ByteBuffer byteBuffer;

        if (FBOMResult err = data.ReadByteBuffer(byteBuffer))
        {
            return err;
        }

        out = HypData(std::move(byteBuffer));

        return { FBOMResult::FBOM_OK };
    }
};

template <class... Types>
struct HypDataHelperDecl<Variant<Types...>>
{
};

template <class... Types>
struct HypDataHelper<Variant<Types...>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<Types...>;

    template <class T>
    static FBOMResult VariantElementSerializeHelper(const Variant<Types...>& variant, FBOMData& outData)
    {
        return HypDataHelper<T>::Serialize(variant.template Get<T>(), outData);
    }

    template <class T>
    static FBOMResult VariantElementDeserializeHelper(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HypData tmp;
        if (FBOMResult err = HypDataHelper<T>::Deserialize(context, data, tmp))
        {
            return err;
        }

        out = HypData(Variant<Types...>(std::move(tmp).template Get<T>()));

        return FBOMResult::FBOM_OK;
    }

    static constexpr std::add_pointer_t<FBOMResult(const Variant<Types...>&, FBOMData&)> elementSerializeFunctions[] = { &VariantElementSerializeHelper<Types>... };
    static constexpr std::add_pointer_t<FBOMResult(FBOMLoadContext&, const FBOMData&, HypData&)> elementDeserializeFunctions[] = { &VariantElementDeserializeHelper<Types>... };

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Variant<Types...>>();
    }

    HYP_FORCE_INLINE Variant<Types...>& Get(Any& value) const // temp
    {
        return value.Get<Variant<Types...>>();
    }

    HYP_FORCE_INLINE const Variant<Types...>& Get(const Any& value) const
    {
        return value.Get<Variant<Types...>>();
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<NormalizedType<T>, Variant<Types...>> && std::disjunction_v<std::is_same<NormalizedType<T>, Types>...>>>
    HYP_FORCE_INLINE constexpr bool Is(const T& value) const
    {
        return true;
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<NormalizedType<T>, Variant<Types...>> && std::disjunction_v<std::is_same<NormalizedType<T>, Types>...>>>
    HYP_FORCE_INLINE Variant<Types...> Get(const T& value) const
    {
        return Variant<Types...>(value);
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<NormalizedType<T>, Variant<Types...>> && std::disjunction_v<std::is_same<NormalizedType<T>, Types>...>>>
    HYP_FORCE_INLINE Variant<Types...> Get(T&& value) const
    {
        return Variant<Types...>(std::forward<T>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const Variant<Types...>& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<Variant<Types...>>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, Variant<Types...>&& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<Variant<Types...>>(std::move(value)));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const Variant<Types...>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const int typeIndex = value.GetTypeIndex();

        if (typeIndex == Variant<Types...>::invalidTypeIndex)
        {
            outData = FBOMData();

            return FBOMResult::FBOM_OK;
        }

        return elementSerializeFunctions[typeIndex](value, outData);
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        if (data.IsUnset())
        {
            out = HypData(Variant<Types...>());

            return FBOMResult::FBOM_OK;
        }

        int foundTypeIndex = Variant<Types...>::invalidTypeIndex;
        int currentTypeIndex = 0;

        for (TypeId typeId : Variant<Types...>::typeIds)
        {
            if (data.GetType().GetNativeTypeId() == typeId
                || IsA(GetClass(data.GetType().GetNativeTypeId()), GetClass(typeId)))
            {
                foundTypeIndex = currentTypeIndex;

                break;
            }

            ++currentTypeIndex;
        }

        if (foundTypeIndex == Variant<Types...>::invalidTypeIndex)
        {
            return { FBOMResult::FBOM_ERR, "Cannot deserialize variant - type not found" };
        }

        return elementDeserializeFunctions[foundTypeIndex](context, data, out);
    }
};

#if 0
template <class T>
struct HypDataHelper<T, std::enable_if_t<!HypData::canStoreDirectly<T> && !implementationExists<HypDataHelperDecl<T>>>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<T*, AnyRef, AnyHandle, RC<void>>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE bool Is(T* value) const
    {
        // Dereferencing a null pointer would be bad - so we'll just pretend it's not the type
        return value != nullptr;
    }

    HYP_FORCE_INLINE bool Is(const AnyRef& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE bool Is(const AnyHandle& value) const
    {
        if constexpr (std::is_base_of_v<HypObjectBase, T>)
        {
            return value.Is<T>();
        }
        else
        {
            return false;
        }
    }

    HYP_FORCE_INLINE bool Is(const RC<void>& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE T& Get(const Any& value) const
    {
        return value.Get<T>();
    }

    HYP_FORCE_INLINE T& Get(T* value) const
    {
        return *value;
    }

    HYP_FORCE_INLINE T& Get(const AnyRef& value) const
    {
        return value.Get<T>();
    }

    HYP_FORCE_INLINE T& Get(const AnyHandle& value) const
    {
        if constexpr (std::is_base_of_v<HypObjectBase, T>)
        {
            return *value.Cast<T>().Get();
        }
        else
        {
            HYP_UNREACHABLE();
        }
    }

    HYP_FORCE_INLINE T& Get(const RC<void>& value) const
    {
        return *value.CastUnsafe_Internal<T>();
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, const T& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<T>(value));
    }

    HYP_FORCE_INLINE void Set(HypData& hypData, T&& value) const
    {
        HypDataHelper<Any>::Set(hypData, Any::Construct<T>(std::move(value)));
    }

    static FBOMResult Serialize(const T& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(TypeId::ForType<NormalizedType<T>>());

        if (!marshal)
        {
            return FBOMResult { FBOMResult::FBOM_ERR, "No marshal registered for type" };
        }

        FBOMObject object;

        if (FBOMResult err = marshal->Serialize(ConstAnyRef(value), object))
        {
            return err;
        }

        outData = FBOMData::FromObject(std::move(object));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, HypData& out)
    {
        HYP_SCOPE;

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(TypeId::ForType<T>());

        if (!marshal)
        {
            return FBOMResult { FBOMResult::FBOM_ERR, "No marshal defined for type" };
        }

        if (!data)
        {
            out = HypData(T {});

            return FBOMResult::FBOM_OK;
        }

        FBOMObject object;

        if (FBOMResult err = data.ReadObject(context, object))
        {
            return err;
        }

        if (FBOMResult err = marshal->Deserialize(context, object, out))
        {
            return err;
        }

        return FBOMResult::FBOM_OK;
    }
};
#endif

#pragma region HypDataTypeChecker implementation

template <class T>
struct HypDataTypeChecker
{
    HYP_FORCE_INLINE bool operator()(const HypData::VariantType& value) const
    {
        constexpr bool shouldSkipAdditionalIsCheck = std::is_same_v<T, typename HypDataHelper<T>::StorageType>;

        static_assert(HypData::canStoreDirectly<typename HypDataHelper<T>::StorageType>, "StorageType must be a type that can be stored directly in the HypData variant without allocating memory dynamically");

        return value.Is<typename HypDataHelper<T>::StorageType>()
            && (shouldSkipAdditionalIsCheck || HypDataHelper<T> {}.Is(value.Get<typename HypDataHelper<T>::StorageType>()));
    }
};

template <class T, class... ConvertibleFrom>
struct HypDataTypeChecker_Tuple<T, Tuple<ConvertibleFrom...>>
{
    HYP_FORCE_INLINE bool operator()(const HypData::VariantType& value) const
    {
        return HypDataTypeChecker<T> {}(value) || (HypDataTypeChecker<ConvertibleFrom> {}(value) || ...);
    }
};

#pragma endregion HypDataTypeChecker implementation

#pragma region Helpers

template <class T>
struct HypDataPlaceholderSerializedType
{
    static inline FBOMType Get()
    {
        static thread_local bool s_isInit = false;
        static thread_local FBOMType s_placeholderType = FBOMPlaceholderType();

        if (!s_isInit)
        {
            s_isInit = true;

            FBOMData placeholderData;

            if (FBOMResult err = HypDataHelper<T>::Serialize(T {}, placeholderData))
            {
                HYP_FAIL("Failed to serialize placeholder data for type %s: %s", TypeName<T>().Data(), *err.message);
            }

            s_placeholderType = placeholderData.GetType();
        }

        return s_placeholderType;
    }
};

template <class T>
struct HypDataPlaceholderSerializedType<Handle<T>>
{
    static inline FBOMType Get()
    {
        static thread_local bool s_isInit = false;
        static thread_local FBOMType s_placeholderType = FBOMPlaceholderType();

        if (!s_isInit)
        {
            s_isInit = true;

            const HypClass* hypClass = GetClass(TypeId::ForType<T>());
            HYP_CORE_ASSERT(hypClass, "HypClass for type %s is not registered", TypeName<T>().Data());

            s_placeholderType = FBOMObjectType(hypClass);
        }

        return s_placeholderType;
    }
};

#pragma endregion Helpers

#pragma region HypDataGetter implementation

// template <class ReturnType, class T>
// struct HypDataGetter
// {
//     HYP_FORCE_INLINE bool operator()(HypData::VariantType &value, ValueStorage<ReturnType> &outStorage) const
//     {
//         if constexpr (HypData::canStoreDirectly<typename HypDataHelper<T>::StorageType>) {
//             return HypDataHelper<NormalizedType<T>>{}.Get(value.Get<typename HypDataHelper<T>::StorageType>(), outStorage);
//         } else {
//             return HypDataHelper<NormalizedType<T>>{}.Get(value.Get<Any>().Get<typename HypDataHelper<T>::StorageType>(), outStorage);
//         }
//     }

//     HYP_FORCE_INLINE bool operator()(const HypData::VariantType &value, ValueStorage<const ReturnType> &outStorage) const
//     {
//         if constexpr (HypData::canStoreDirectly<typename HypDataHelper<T>::StorageType>) {
//             return HypDataHelper<NormalizedType<T>>{}.Get(value.Get<typename HypDataHelper<T>::StorageType>(), outStorage);
//         } else {
//             return HypDataHelper<NormalizedType<T>>{}.Get(value.Get<Any>().Get<typename HypDataHelper<T>::StorageType>(), outStorage);
//         }
//     }
// };

template <class VariantType, class ReturnType, class... Types, SizeType... Indices>
HYP_FORCE_INLINE bool HypDataGetter_Tuple_Impl(VariantType&& value, Optional<ReturnType>& outValue, std::index_sequence<Indices...>)
{
    const auto getForTypeIndex = [&value]<SizeType SelectedTypeIndex>(Optional<ReturnType>& outValue, std::integral_constant<SizeType, SelectedTypeIndex>) -> bool
    {
        using SelectedType = typename TupleElement<SelectedTypeIndex, Types...>::Type;
        using StorageType = typename HypDataHelper<SelectedType>::StorageType;

        static_assert(HypData::canStoreDirectly<typename HypDataHelper<NormalizedType<ReturnType>>::StorageType>);

        if constexpr (std::is_same_v<NormalizedType<ReturnType>, StorageType>)
        {
            outValue.Set(value.template Get<StorageType>());
        }
        else
        {
            decltype(auto) internalValue = value.template Get<StorageType>();

            if (!HypDataHelper<NormalizedType<ReturnType>> {}.Is(internalValue))
            {
                return false;
            }

            outValue.Set(HypDataHelper<NormalizedType<ReturnType>> {}.Get(std::forward<decltype(internalValue)>(internalValue)));
        }

        return true;
    };

    return ((HypDataTypeChecker<Types> {}(value) && getForTypeIndex(outValue, std::integral_constant<SizeType, Indices> {})) || ...);
}

template <class ReturnType, class T, class... ConvertibleFrom>
struct HypDataGetter_Tuple<ReturnType, T, Tuple<ConvertibleFrom...>>
{
    HYP_FORCE_INLINE bool operator()(HypData::VariantType& value, Optional<ReturnType>& outValue) const
    {
        return HypDataGetter_Tuple_Impl<HypData::VariantType&, ReturnType, T, ConvertibleFrom...>(value, outValue, std::index_sequence_for<T, ConvertibleFrom...> {});
    }

    HYP_FORCE_INLINE bool operator()(const HypData::VariantType& value, Optional<ReturnType>& outValue) const
    {
        return HypDataGetter_Tuple_Impl<const HypData::VariantType&, ReturnType, T, ConvertibleFrom...>(value, outValue, std::index_sequence_for<T, ConvertibleFrom...> {});
    }
};

#pragma endregion HypDataGetter implementation

static_assert(sizeof(HypData) == 32, "sizeof(HypData) != 32 bytes");

} // namespace hyperion
