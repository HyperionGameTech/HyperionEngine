/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/BoxedValueFwd.hpp>

#include <core/reflection/ObjId.hpp>
#include <core/reflection/Handle.hpp>
#include <core/reflection/ObjectFwd.hpp>
#include <core/reflection/GenericArrayWrapper.hpp>
#include <core/reflection/TypeInfoFwd.hpp>

#include <core/containers/Array.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/utilities/Variant.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/Pair.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Float16.hpp>
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

namespace Hyperion {

namespace filesystem {
class FilePath;
} // namespace filesystem

using filesystem::FilePath;

HYP_API extern const Class* GetClass(const TypeId& typeId);
HYP_API extern bool IsA(const Class* cls, const Class* instanceClass);

template <class T, class T2 = void>
struct HypDataHelper;

template <class T, class T2 = void>
struct HypDataHelperDecl;

template <class T, class ConvertibleFromTuple>
struct HypData_Is;

template <class ReturnType, class T, class ConvertibleFromTuple>
struct HypData_Get;

template <class T, bool IsConst>
struct HypDataGetReturnTypeHelper
{
    using Type = decltype(std::declval<HypDataHelper<T>>().Get(std::declval<std::conditional_t<IsConst, const typename HypDataHelper<T>::StorageType&, typename HypDataHelper<T>::StorageType&>>()));
};

template <>
struct HypDataGetReturnTypeHelper<BoxedValue, false>
{
    using Type = BoxedValue&;
};

template <>
struct HypDataGetReturnTypeHelper<BoxedValue, true>
{
    using Type = const BoxedValue&;
};

using HypDataSerializeFunction = FBOMResult (*)(const BoxedValue& data, FBOMData& out, EnumFlags<FBOMDataFlags> flags);

extern HYP_API HypDataSerializeFunction GetHypDataSerializeFunction(TypeId typeId);
extern HYP_API void RegisterHypDataSerializeFunction(TypeId typeId, HypDataSerializeFunction func);
extern HYP_API void SetHypDataFromReference(BoxedValue& boxed, AnyRef ref);

struct GenericArrayWrapper;

#ifdef HYP_SCRIPT
enum class GCIndex : uint32;

static constexpr GCIndex INVALID_GC_INDEX = GCIndex(0);
static constexpr GCIndex GARBAGE_GC_INDEX = GCIndex(~0u);

// max 31 bits for index - this is the highest valid index
static constexpr GCIndex MAX_GC_INDEX = GCIndex((1u << 31) - 1);
#endif

/*! \brief A type-safe union that can store multiple different types of run-time data, abstracting away internal engine structures such as Handle<T>, RC<T>, etc.
 *  Providing a unified way of accessing the data via Get<T>() and TryGet<T>() methods.
 *  \note Used in serialization, reflection, scripting, and other systems where data needs to be stored in a generic way.
 */
struct BoxedValue
{
    /*! \brief A struct that can hold up to 16 bytes of user data.
     *  Useful for storing small amounts of data directly in BoxedValue without heap allocation.
     *  \note This is primarily for internal use and should be used with care to avoid alignment issues.
     */
    struct alignas(std::max_align_t) InlineData
    {
        uint64 data[2];

        HYP_FORCE_INLINE bool operator==(const InlineData& other) const
        {
            return Memory::MemCmp(this, &other, sizeof(InlineData)) == 0;
        }

        HYP_FORCE_INLINE bool operator!=(const InlineData& other) const
        {
            return !operator==(other);
        }
    };

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
        Float16,
        Name,
        ObjIdBase,
        ClassRef,
        Handle<ObjectBase>,
        RC<void>,
        AnyRef,
        Any,
        InlineData>;

    template <class T>
    static constexpr bool canStoreDirectly =
        /* Fundamental types - can be stored inline */
        std::is_same_v<T, int8> || std::is_same_v<T, int16> | std::is_same_v<T, int32> | std::is_same_v<T, int64> || std::is_same_v<T, uint8> || std::is_same_v<T, uint16> || std::is_same_v<T, uint32> || std::is_same_v<T, uint64>

        || std::is_same_v<T, char> || std::is_same_v<T, float> || std::is_same_v<T, double> || std::is_same_v<T, bool> || std::is_same_v<T, void*>

        || std::is_same_v<T, Float16>

        /*! Name is 32 bits and can be stored inline */
        || std::is_same_v<T, Name>

        /*! All ObjId<T> are stored as ObjIdBase */
        || std::is_base_of_v<ObjIdBase, T>

        || std::is_same_v<T, ClassRef>

        /*! Handle<T> gets stored as Handle<ObjectBase>, which holds TypeId for conversion */
        || std::is_base_of_v<HandleBase, T> || std::is_same_v<T, Handle<ObjectBase>> || std::is_base_of_v<ObjectBase, T>

        /*! RC<T> gets stored as RC<void> and can be converted back */
        || std::is_base_of_v<typename RC<void>::RefCountedPtrBase, T>

        /*! Pointers are stored as AnyRef which holds TypeId for conversion */
        || std::is_same_v<T, AnyRef> || std::is_pointer_v<T>

        || std::is_same_v<T, Any> || std::is_same_v<T, InlineData>;

    VariantType value;

#ifdef HYP_SCRIPT
    union
    {
        // HypScript only - object metadata
        struct
        {
            GCIndex gcIndex : 31;   // index into the pool of tracked objects
            uint8 isStaticInit : 1; // static data pool / stack data - will be 1 if init
        };
    } extData;
#else
    char extData[1]; // preserved for backwards compatibility
#endif

    BoxedValue()
    {
        Memory::MemSet(&extData, 0, sizeof(extData));
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<NormalizedType<T>, BoxedValue>>>
    explicit BoxedValue(T&& value)
        : BoxedValue()
    {
#if 0
        if constexpr (std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>)
        {
            SetHypDataFromReference(*this, AnyRef(&value));
        }
        else
        {
            HypDataHelper<NormalizedType<T>> {}.Set(*this, std::forward<T>(value));
        }
#else
        HypDataHelper<NormalizedType<T>> {}.Set(*this, std::forward<T>(value));
#endif
    }

    BoxedValue(const BoxedValue& other)
        : value(other.value)
    {
        Memory::MemCpy(&extData, &other.extData, sizeof(extData));
    }

    BoxedValue& operator=(const BoxedValue& other)
    {
        if (&other == this)
        {
            return *this;
        }

        value = other.value;

        Memory::MemCpy(&extData, &other.extData, sizeof(extData));

        return *this;
    }

    BoxedValue(BoxedValue&& other) noexcept
        : value(std::move(other.value))
    {
        Memory::MemCpy(&extData, &other.extData, sizeof(extData));
        Memory::MemSet(&other.extData, 0, sizeof(extData));
    }

    BoxedValue& operator=(BoxedValue&& other) noexcept
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

    ~BoxedValue()
    {
#ifdef HYP_DEBUG_MODE
#ifdef HYP_SCRIPT
        HYP_CORE_ASSERT(extData.gcIndex == INVALID_GC_INDEX, "BoxedValue being destroyed while still registered with the GC (index = %u)", uint32(extData.gcIndex));
        extData.gcIndex = GARBAGE_GC_INDEX;
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
        return Is<GenericArrayWrapper>();
    }

    HYP_FORCE_INLINE TypeId GetTypeId() const
    {
        return ToRef().GetTypeId();
    }

    HYP_FORCE_INLINE const TypeInfo* GetTypeInfo() const
    {
        return ToRef().GetTypeInfo();
    }

    HYP_FORCE_INLINE void Reset()
    {
        value.Reset();
    }

    AnyRef ToRef()
    {
        if (!IsValid())
        {
            return AnyRef();
        }

        if (Handle<ObjectBase>* anyHandlePtr = value.TryGet<Handle<ObjectBase>>())
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

        return AnyRef(value.GetCurrentTypeInfo(), value.GetPointer());
    }

    HYP_FORCE_INLINE AnyRef ToRef() const
    {
        return const_cast<BoxedValue*>(this)->ToRef();
    }

    template <class T>
    bool Is(bool strict = false) const
    {
        if constexpr (std::is_same_v<T, BoxedValue>)
        {
            return true;
        }
        else
        {
            if (strict)
            {
                return HypData_Is<T, Tuple<>> {}(value, /* checkReference */ true);
            }

            return HypData_Is<T, typename HypDataHelper<T>::ConvertibleFrom> {}(value, /* checkReference */ true);
        }
    }

    template <class T>
    auto Get() -> typename HypDataGetReturnTypeHelper<T, false>::Type
    {
        if constexpr (std::is_same_v<T, BoxedValue>)
        {
            return *this;
        }
        else
        {
#ifdef HYP_DEBUG_MODE
            HYP_CORE_ASSERT(Is<T>(), "Expected %s, got %s", TypeName<T>().Data(), *TypeInfo_GetName(*GetTypeInfo()));
#endif

            using ReturnType = typename HypDataGetReturnTypeHelper<T, false>::Type;

            Optional<ReturnType> resultValue;
            HypData_Get<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> {}(value, resultValue);

#ifdef HYP_DEBUG_MODE
            HYP_CORE_ASSERT(resultValue.HasValue(),
                "Failed to invoke BoxedValue Get method with T = %s - Mismatched types or T could not be converted to the held type (%s)",
                TypeName<T>().Data(),
                *TypeInfo_GetName(*GetTypeInfo()));
#endif

            return *resultValue;
        }
    }

    template <class T>
    auto Get() const -> typename HypDataGetReturnTypeHelper<T, true>::Type
    {
        if constexpr (std::is_same_v<T, BoxedValue>)
        {
            return *this;
        }
        else
        {
#ifdef HYP_DEBUG_MODE
            HYP_CORE_ASSERT(Is<T>(), "Expected %s, got %s", TypeName<T>().Data(), *TypeInfo_GetName(*GetTypeInfo()));
#endif

            using ReturnType = typename HypDataGetReturnTypeHelper<T, true>::Type;

            Optional<ReturnType> resultValue;
            HypData_Get<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> {}(value, resultValue);

#ifdef HYP_DEBUG_MODE
            HYP_CORE_ASSERT(resultValue.HasValue(),
                "Failed to invoke BoxedValue Get method with T = %s - Mismatched types or T could not be converted to the held type (%s)",
                TypeName<T>().Data(),
                *TypeInfo_GetName(*GetTypeInfo()));
#endif

            return *resultValue;
        }
    }

    template <class T>
    auto TryGet() -> Optional<typename HypDataGetReturnTypeHelper<T, false>::Type>
    {
        if constexpr (std::is_same_v<T, BoxedValue>)
        {
            return *this;
        }
        else
        {
            using ReturnType = typename HypDataGetReturnTypeHelper<T, false>::Type;

            Optional<ReturnType> resultValue;
            HypData_Get<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> {}(value, resultValue);

            return resultValue;
        }
    }

    template <class T>
    auto TryGet() const -> Optional<typename HypDataGetReturnTypeHelper<T, true>::Type>
    {
        if constexpr (std::is_same_v<T, BoxedValue>)
        {
            return *this;
        }
        else
        {
            using ReturnType = typename HypDataGetReturnTypeHelper<T, true>::Type;

            Optional<ReturnType> resultValue;
            HypData_Get<ReturnType, T, typename HypDataHelper<T>::ConvertibleFrom> {}(value, resultValue);

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
        BoxedValue outData;

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
                    [](const BoxedValue& data, FBOMData& out, EnumFlags<FBOMDataFlags> flags) -> FBOMResult
                    {
                        return HypDataHelper<NormalizedType<T>>::Serialize(data.Get<NormalizedType<T>>(), out, flags);
                    });
            }
        } s_initializeSerializeFunction;

        this->value.Set<NormalizedType<T>>(std::forward<T>(value));
    }
};

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
        bool,
        Float16>;

    // Fundamental types

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

    // Float16 Conversion

    HYP_FORCE_INLINE constexpr bool Is(Float16 value) const
    {
        return true;
    }

    HYP_FORCE_INLINE constexpr T Get(Float16 value) const
    {
        return static_cast<T>(float(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, T value) const
    {
        boxed.Set_Internal(value);
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(T value, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        out = FBOMData(value, flags);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        T value;

        if (FBOMResult err = data.Read(&value))
        {
            return err;
        }

        out = BoxedValue(value);

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, SizeType value) const
    {
        boxed.Set_Internal(static_cast<uint64>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(SizeType value, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        out = FBOMData(static_cast<uint64>(value), flags);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        uint64 value;

        if (FBOMResult err = data.Read(&value))
        {
            return err;
        }

        out = BoxedValue(static_cast<SizeType>(value));

        return FBOMResult::FBOM_OK;
    }
};

#endif

template <>
struct HypDataHelperDecl<Float16>
{
};

template <>
struct HypDataHelper<Float16> : HypDataHelper<uint16>
{
    using StorageType = Float16;
    using ConvertibleFrom = Tuple<
        float,
        double>;

    HYP_FORCE_INLINE bool Is(Float16 value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<OtherT, Float16>>>
    HYP_FORCE_INLINE bool Is(OtherT value) const
    {
        return std::is_floating_point_v<OtherT>;
    }

    HYP_FORCE_INLINE constexpr Float16 Get(Float16 value) const
    {
        return value;
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<OtherT, Float16>>>
    HYP_FORCE_INLINE constexpr Float16 Get(OtherT value) const
    {
        return Float16(float(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, Float16 value) const
    {
        boxed.Set_Internal(value);
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(Float16 value, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        out = FBOMData(value.value, flags);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        uint16 value;

        if (FBOMResult err = data.Read(&value))
        {
            return err;
        }

        out = BoxedValue(Float16(value));

        return FBOMResult::FBOM_OK;
    }
};

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

    HYP_FORCE_INLINE constexpr T Get(T value) const
    {
        return value;
    }

    HYP_FORCE_INLINE constexpr T Get(std::underlying_type_t<T> value) const
    {
        return T(value);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, T value) const
    {
        HypDataHelper<std::underlying_type_t<T>>::Set(boxed, static_cast<std::underlying_type_t<T>>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(T value, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        out = FBOMData(static_cast<std::underlying_type_t<T>>(value), flags);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        std::underlying_type_t<T> value;

        if (FBOMResult err = data.Read(&value))
        {
            return err;
        }

        out = BoxedValue(value);

        return FBOMResult::FBOM_OK;
    }
};

template <class T>
struct HypDataHelperDecl<EnumFlags<T>>
{
};

template <class T>
struct HypDataHelper<EnumFlags<T>> : HypDataHelper<T>
{
    HYP_FORCE_INLINE bool Is(EnumFlags<T> value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE constexpr bool Is(std::underlying_type_t<T>) const
    {
        return true;
    }

    HYP_FORCE_INLINE constexpr EnumFlags<T> Get(EnumFlags<T> value) const
    {
        return value;
    }

    HYP_FORCE_INLINE constexpr EnumFlags<T> Get(std::underlying_type_t<T> value) const
    {
        return EnumFlags<T>(value);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, EnumFlags<T> value) const
    {
        HypDataHelper<typename EnumFlags<T>::UnderlyingType>::Set(boxed, static_cast<typename EnumFlags<T>::UnderlyingType>(value));
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
    using ConvertibleFrom = Tuple<AnyRef, Handle<ObjectBase>, RC<void>>;

    HYP_FORCE_INLINE bool Is(void* value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE bool Is(const AnyRef& value) const
    {
        return true;
    }

    HYP_FORCE_INLINE bool Is(const Handle<ObjectBase>& value) const
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

    HYP_FORCE_INLINE void* Get(const Handle<ObjectBase>& value) const
    {
        return value.ToRef().GetPointer();
    }

    HYP_FORCE_INLINE void* Get(const RC<void>& value) const
    {
        return value.Get();
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, void* value) const
    {
        boxed.Set_Internal(value);
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(void* value, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        return { FBOMResult::FBOM_ERR, "Cannot serialize a user pointer!" };
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        return { FBOMResult::FBOM_ERR, "Cannot deserialize a user pointer!" };
    }
};

/// ObjIdBase specialization - stores as ObjIdBase internally, ObjId<T> converts to/from this.

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const ObjIdBase& value) const
    {
        boxed.Set_Internal(value);
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(ObjIdBase value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        outData = FBOMData::FromStruct<ObjIdBase>(value, flags);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        ObjIdBase value;

        if (FBOMResult err = data.ReadStruct<ObjIdBase>(&value))
        {
            return err;
        }

        out = BoxedValue(value);

        return FBOMResult::FBOM_OK;
    }
};

/// ObjId<T> specialization - stores as ObjIdBase internally, converts to/from ObjIdBase and Handle<ObjectBase>.

template <class T>
struct HypDataHelperDecl<ObjId<T>>
{
};

template <class T>
struct HypDataHelper<ObjId<T>> : HypDataHelper<ObjIdBase>
{
    using ConvertibleFrom = Tuple<Handle<ObjectBase>>;

    HYP_FORCE_INLINE bool Is(const ObjIdBase& value) const
    {
        return IsA(GetClass(TypeId::ForType<T>()), GetClass(value.GetTypeId()));
    }

    HYP_FORCE_INLINE bool Is(const Handle<ObjectBase>& value) const
    {
        // allow null handles through
        return !value || IsA(GetClass(TypeId::ForType<T>()), GetClass(value.GetTypeId()));
    }

    HYP_FORCE_INLINE ObjId<T> Get(ObjIdBase value) const
    {
        return ObjId<T>(value);
    }

    HYP_FORCE_INLINE ObjId<T> Get(const Handle<ObjectBase>& value) const
    {
        return ObjId<T>(value.Id());
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const ObjId<T>& value) const
    {
        HypDataHelper<ObjIdBase>::Set(boxed, static_cast<const ObjIdBase&>(value));
    }
};

/// ClassRef specialization - stores as ClassRef internally, not serializable.

template <>
struct HypDataHelperDecl<ClassRef>
{
};

template <>
struct HypDataHelper<ClassRef>
{
    using StorageType = ClassRef;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const ClassRef& value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE ClassRef& Get(ClassRef& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE const ClassRef& Get(const ClassRef& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const ClassRef& value) const
    {
        boxed.Set_Internal(value);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, ClassRef&& value) const
    {
        boxed.Set_Internal(std::move(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(ClassRef value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        return { FBOMResult::FBOM_ERR, "Cannot serialize ClassRef!" };
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        return { FBOMResult::FBOM_ERR, "Cannot deserialize ClassRef!" };
    }
};

/// Handle<ObjectBase> specialization - stores as Handle<ObjectBase> internally, serializable

template <>
struct HypDataHelperDecl<Handle<ObjectBase>>
{
};

template <>
struct HypDataHelper<Handle<ObjectBase>>
{
    using StorageType = Handle<ObjectBase>;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Handle<ObjectBase>& value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE Handle<ObjectBase>& Get(Handle<ObjectBase>& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE const Handle<ObjectBase>& Get(const Handle<ObjectBase>& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const Handle<ObjectBase>& value) const
    {
        boxed.Set_Internal(value);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, Handle<ObjectBase>&& value) const
    {
        boxed.Set_Internal(std::move(value));
    }

    static FBOMResult Serialize(const Handle<ObjectBase>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
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

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        if (!data)
        {
            out = BoxedValue(Handle<ObjectBase> {});

            return FBOMResult::FBOM_OK;
        }

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(data.GetType().GetNativeTypeId());

        if (!marshal)
        {
            return FBOMResult { FBOMResult::FBOM_ERR, "No marshal defined for handle type" };
        }

        FBOMObject object;

        if (FBOMResult err = data.ReadObject(context, object, /* deserializeObject */ false))
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

/// Handle<T> specialization - stores as Handle<ObjectBase> internally, converts to/from Handle<ObjectBase>

template <class T>
struct HypDataHelperDecl<Handle<T>, std::enable_if_t<!std::is_same_v<T, ObjectBase>>>
{
};

template <class T>
struct HypDataHelper<Handle<T>> : HypDataHelper<Handle<ObjectBase>, std::enable_if_t<!std::is_same_v<T, ObjectBase>>>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Handle<ObjectBase>& value) const
    {
        // Allow null handles through
        return !value || IsA(GetClass(TypeId::ForType<T>()), GetClass(value.GetTypeId()));
    }

    HYP_FORCE_INLINE Handle<T>& Get(Handle<ObjectBase>& value) const
    {
        return reinterpret_cast<Handle<T>&>(value);
    }

    HYP_FORCE_INLINE const Handle<T>& Get(const Handle<ObjectBase>& value) const
    {
        return reinterpret_cast<const Handle<T>&>(value);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const Handle<T>& value) const
    {
        HypDataHelper<Handle<ObjectBase>>::Set(boxed, reinterpret_cast<const Handle<ObjectBase>&>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, Handle<T>&& value) const
    {
        HypDataHelper<Handle<ObjectBase>>::Set(boxed, reinterpret_cast<Handle<ObjectBase>&&>(std::move(value)));
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(TypeId::ForType<T>());

        if (!marshal)
        {
            return FBOMResult { FBOMResult::FBOM_ERR, "No marshal defined for handle type" };
        }

        if (!data)
        {
            out = BoxedValue(Handle<T> {});

            return FBOMResult::FBOM_OK;
        }

        FBOMObject object;

        if (FBOMResult err = data.ReadObject(context, object, /* deserializeObject */ false))
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

/// Objects can be stored inline via Handle<ObjectBase> like Handle<T>, and converted to/from Handle<T>

template <class T>
struct HypDataHelperDecl<T, std::enable_if_t<std::is_base_of_v<ObjectBase, T>>>
{
};

template <class T>
struct HypDataHelper<T, std::enable_if_t<std::is_base_of_v<ObjectBase, T>>> : HypDataHelper<Handle<T>>
{
    using ConvertibleFrom = Tuple<AnyRef>;

    HYP_FORCE_INLINE bool Is(const Handle<ObjectBase>& value) const
    {
        return value && HypDataHelper<Handle<T>>::Is(value);
    }

    HYP_FORCE_INLINE T& Get(const Handle<ObjectBase>& value) const
    {
        return *HypDataHelper<Handle<T>>::Get(value);
    }

    HYP_FORCE_INLINE bool Is(const AnyRef& value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE T& Get(const AnyRef& value) const
    {
        HYP_CORE_ASSERT(value.HasValue(), "Tried to get BoxedValue value from null pointer");
        return value.Get<T>();
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const T& value) const
    {
        HypDataHelper<Handle<T>>::Set(boxed, value.HandleFromThis());
    }
};

/// RefCountedPtr void type can be used to hold any other RefCountedPtr type

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const RC<void>& value) const
    {
        boxed.Set_Internal(value);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, RC<void>&& value) const
    {
        boxed.Set_Internal(std::move(value));
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
        // allow null pointers
        return !value || value.Is<T>();
    }

    HYP_FORCE_INLINE RC<T>& Get(RC<void>& value) const
    {
        return *reinterpret_cast<RC<T>*>(&value);
    }

    HYP_FORCE_INLINE const RC<T>& Get(const RC<void>& value) const
    {
        return *reinterpret_cast<const RC<T>*>(&value);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const RC<T>& value) const
    {
        HypDataHelper<RC<void>>::Set(boxed, value);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, RC<T>&& value) const
    {
        HypDataHelper<RC<void>>::Set(boxed, std::move(value));
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(TypeId::ForType<T>());

        if (!marshal)
        {
            return FBOMResult { FBOMResult::FBOM_ERR, "No marshal defined for type" };
        }

        if (!data)
        {
            out = BoxedValue(RC<T> {});

            return FBOMResult::FBOM_OK;
        }

        FBOMObject object;

        if (FBOMResult err = data.ReadObject(context, object, /* deserializeObject */ false))
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

/// AnyRef - type erased reference - @TODO: Add ConstAnyRef support

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const AnyRef& value) const
    {
        boxed.Set_Internal(value);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, AnyRef&& value) const
    {
        boxed.Set_Internal(std::move(value));
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

/// T* - raw pointer (non-owning, non-const) held as AnyRef

template <class T>
struct HypDataHelperDecl<T*, std::enable_if_t<!IsConstPointerV<T*> && !std::is_same_v<T*, void*>>>
{
};

template <class T>
struct HypDataHelper<T*, std::enable_if_t<!IsConstPointerV<T*> && !std::is_same_v<T*, void*>>> : HypDataHelper<AnyRef>
{
    using ConvertibleFrom = Tuple<Handle<ObjectBase>, RC<void>>;

    HYP_FORCE_INLINE bool Is(const AnyRef& value) const
    {
        // allow null pointers
        return !value.HasValue() || value.Is<T>();
    }

    HYP_FORCE_INLINE bool Is(const Handle<ObjectBase>& value) const
    {
        return !value || IsA(GetClass(TypeId::ForType<T>()), GetClass(value.GetTypeId()));
    }

    HYP_FORCE_INLINE bool Is(const RC<void>& value) const
    {
        return !value || value.Is<T>();
    }

    HYP_FORCE_INLINE T* Get(const AnyRef& value) const
    {
        if (!value.HasValue())
        {
            return nullptr;
        }

        HYP_CORE_ASSERT(value.Is<T>());

        return static_cast<T*>(value.GetPointer());
    }

    HYP_FORCE_INLINE T* Get(const Handle<ObjectBase>& value) const
    {
        if constexpr (std::is_base_of_v<ObjectBase, T>)
        {
            return static_cast<T*>(value.Get());
        }

        return nullptr;
    }

    HYP_FORCE_INLINE T* Get(const RC<void>& value) const
    {
        if (!value)
        {
            return nullptr;
        }

        HYP_CORE_ASSERT(value.Is<T>());

        return value.CastUnchecked<T>();
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, T* value) const
    {
        HypDataHelper<AnyRef>::Set(boxed, AnyRef(&TypeOf<T>(), value));
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(TypeId::ForType<T>());

        if (!marshal)
        {
            return FBOMResult { FBOMResult::FBOM_ERR, "No marshal defined for type" };
        }

        if (!data)
        {
            out = BoxedValue(static_cast<T*>(nullptr));

            return FBOMResult::FBOM_OK;
        }

        FBOMObject object;

        if (FBOMResult err = data.ReadObject(context, object, /* deserializeObject */ false))
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

/// const T* - raw pointer (non-owning, const) held as AnyRef

template <class T>
struct HypDataHelperDecl<const T*, std::enable_if_t<!std::is_same_v<T*, void*>>>
{
};

template <class T>
struct HypDataHelper<const T*, std::enable_if_t<!std::is_same_v<T*, void*>>> : HypDataHelper<T*>
{
    HYP_FORCE_INLINE const T* Get(const ConstAnyRef& value) const
    {
        return HypDataHelper<T*>::Get(AnyRef(value.GetTypeInfo(), const_cast<void*>(value.GetPointer())));
    }

    HYP_FORCE_INLINE const T* Get(const AnyRef& value) const
    {
        return HypDataHelper<T*>::Get(value);
    }

    HYP_FORCE_INLINE const T* Get(const Handle<ObjectBase>& value) const
    {
        return HypDataHelper<T*>::Get(value);
    }

    HYP_FORCE_INLINE const T* Get(const RC<void>& value) const
    {
        return HypDataHelper<T*>::Get(value);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const T* value) const
    {
        HypDataHelper<T*>::Set(boxed, const_cast<T*>(value));
    }
};

/// Any - type erased value, allocated on the heap

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, Any&& value) const
    {
        boxed.Set_Internal(std::move(value));
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

/// GenericArrayWrapper - generic array / container type wrapper - @TODO Add HypDataMap for associative containers

template <>
struct HypDataHelperDecl<GenericArrayWrapper>
{
};

template <>
struct HypDataHelper<GenericArrayWrapper> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<GenericArrayWrapper>();
    }

    HYP_FORCE_INLINE GenericArrayWrapper& Get(const Any& value) const
    {
        return value.Get<GenericArrayWrapper>();
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const GenericArrayWrapper& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<GenericArrayWrapper>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, GenericArrayWrapper&& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<GenericArrayWrapper>(std::move(value)));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const GenericArrayWrapper& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        GenericArrayWrapper::SerializeFunction serializeFunction = value.functionTable.serializeFunction;

        if (!serializeFunction)
        {
            return { FBOMResult::FBOM_ERR, "Cannot serialize GenericArrayWrapper without a serialize function!" };
        }

        if (FBOMResult err = serializeFunction(value, outData, flags))
        {
            return err;
        }

        return FBOMResult::FBOM_OK;
    }
};

template <>
struct HypDataHelperDecl<BoxedValue::InlineData>
{
};

template <>
struct HypDataHelper<BoxedValue::InlineData>
{
    using StorageType = BoxedValue::InlineData;
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const BoxedValue::InlineData& value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FORCE_INLINE BoxedValue::InlineData& Get(BoxedValue::InlineData& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE const BoxedValue::InlineData& Get(const BoxedValue::InlineData& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const BoxedValue::InlineData& value) const
    {
        boxed.Set_Internal(value);
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const BoxedValue::InlineData& value, FBOMData& out, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        return { FBOMResult::FBOM_ERR, "Cannot serialize user data!" };
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        return { FBOMResult::FBOM_ERR, "Cannot deserialize user data!" };
    }
};

/// String types

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

    HYP_FORCE_INLINE containers::String<StringType>& Get(const Any& value) const
    {
        return value.Get<containers::String<StringType>>();
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const containers::String<StringType>& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<containers::String<StringType>>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, containers::String<StringType>&& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<containers::String<StringType>>(std::move(value)));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const containers::String<StringType>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData::FromString(value);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        containers::String<StringType> result;

        if (FBOMResult err = data.ReadString(result))
        {
            return err;
        }

        out = BoxedValue(std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

/// StringView types - held as String, memory is owned

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const utilities::StringView<StringType>& value) const
    {
        HypDataHelper<containers::String<StringType>>::Set(boxed, value);
    }
};

/// C String - converted to String, memory is owned

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const char* value) const
    {
        HypDataHelper<String>::Set(boxed, String(value));
    }
};

/// FilePath - stored as String (base class of FilePath)

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const FilePath& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<String>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, FilePath&& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<String>(std::move(value)));
    }
};

/// Name and StringHash - stored as String value

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const Name& value) const
    {
        boxed.Set_Internal(value);
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const Name& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData::FromName(value);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        Name name;

        if (FBOMResult err = data.ReadName(&name))
        {
            return err;
        }

        out = BoxedValue(name);

        return { FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<StringHash>
{
};

template <>
struct HypDataHelper<StringHash>
{
    using StorageType = Name;
    using ConvertibleFrom = Tuple<Name>;

    HYP_FORCE_INLINE constexpr bool Is(const StringHash&) const
    {
        return true;
    }

    HYP_FORCE_INLINE constexpr StringHash& Get(StringHash& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE constexpr const StringHash& Get(const StringHash& value) const
    {
        return value;
    }

    HYP_FORCE_INLINE constexpr bool Is(const Name&) const
    {
        return true;
    }

    HYP_FORCE_INLINE StringHash& Get(Name& value) const
    {
        return *reinterpret_cast<StringHash*>(&value);
    }

    HYP_FORCE_INLINE const StringHash& Get(const Name& value) const
    {
        return *reinterpret_cast<const StringHash*>(&value);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const StringHash& value) const
    {
        boxed.Set_Internal(*reinterpret_cast<const Name*>(&value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const StringHash& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        return HypDataHelper<Name>::Serialize(*reinterpret_cast<const Name*>(&value), outData, flags);
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        BoxedValue nameData;

        if (FBOMResult err = HypDataHelper<Name>::Deserialize(context, data, nameData))
        {
            return err;
        }

        out = BoxedValue(StringHash(nameData.Get<Name>()));

        return { FBOMResult::FBOM_OK };
    }
};

/// Array types

template <class T, class AllocatorType>
struct HypDataHelperDecl<Array<T, AllocatorType>, std::enable_if_t<!std::is_const_v<T>>>
{
};

template <class T, class AllocatorType>
struct HypDataHelper<Array<T, AllocatorType>, std::enable_if_t<!std::is_const_v<T>>> : HypDataHelper<GenericArrayWrapper>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        const TypeId arrayTypeId = TypeId::ForType<Array<T, AllocatorType>>();

        if (const GenericArrayWrapper* arr = value.TryGet<GenericArrayWrapper>())
        {
            return TypeInfo_GetId(*arr->typeInfo) == arrayTypeId;
        }

        return value.GetTypeId() == arrayTypeId;
    }

    HYP_FORCE_INLINE Array<T, AllocatorType>& Get(const Any& value) const
    {
        const TypeId arrayTypeId = TypeId::ForType<Array<T, AllocatorType>>();

        // debug
        HYP_CORE_ASSERT(this->Is(value));

        if (const GenericArrayWrapper* arr = value.TryGet<GenericArrayWrapper>())
        {
            HYP_CORE_ASSERT(TypeInfo_GetId(*arr->typeInfo) == arrayTypeId);

            return *static_cast<Array<T, AllocatorType>*>(arr->pInternalArray);
        }

        HYP_CORE_ASSERT(value.GetTypeId() == arrayTypeId);

        return value.Get<Array<T, AllocatorType>>();
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const Array<T, AllocatorType>& value) const
    {
        HypDataHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, Array<T, AllocatorType>&& value) const
    {
        HypDataHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, std::move(value)));
    }

    static FBOMResult Serialize(const Array<T, AllocatorType>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const SizeType size = value.Size();

        if (size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            outData = FBOMData::FromArray(FBOMArray());

            return FBOMResult::FBOM_OK;
        }

        Array<FBOMData> elements;
        elements.Resize(size);

        for (SizeType i = 0; i < size; i++)
        {
            if constexpr (IsBoxedValueV<T>)
            {
                if (FBOMResult err = value[i].Serialize(elements[i], FBOMDataFlags::NONE))
                {
                    return err;
                }
            }
            else
            {
                if (FBOMResult err = HypDataHelper<T>::Serialize(value[i], elements[i], FBOMDataFlags::NONE))
                {
                    return err;
                }
            }
        }

        outData = FBOMData::FromArray(FBOMArray(std::move(elements)));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        FBOMArray array;

        if (FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        const SizeType size = array.Size();

        Array<T, AllocatorType> result;
        result.Reserve(size);

        for (SizeType i = 0; i < size; i++)
        {
            BoxedValue element;

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

/// FixedArray

template <class T, SizeType Size>
struct HypDataHelperDecl<FixedArray<T, Size>>
{
};

template <class T, SizeType Size>
struct HypDataHelper<FixedArray<T, Size>, std::enable_if_t<!std::is_const_v<T>>> : HypDataHelper<GenericArrayWrapper>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        if (const GenericArrayWrapper* arr = value.TryGet<GenericArrayWrapper>())
        {
            return TypeInfo_GetId(*arr->typeInfo) == TypeId::ForType<FixedArray<T, Size>>();
        }

        return value.GetTypeId() == TypeId::ForType<FixedArray<T, Size>>();
    }

    HYP_FORCE_INLINE FixedArray<T, Size>& Get(const Any& value) const
    {
        if (const GenericArrayWrapper* arr = value.TryGet<GenericArrayWrapper>())
        {
            if (TypeInfo_GetId(*arr->typeInfo) == TypeId::ForType<FixedArray<T, Size>>())
            {
                return *static_cast<FixedArray<T, Size>*>(arr->pInternalArray);
            }
        }
        else if (value.GetTypeId() == TypeId::ForType<FixedArray<T, Size>>())
        {
            return value.Get<FixedArray<T, Size>>();
        }

        HYP_UNREACHABLE();
    }

    HYP_FORCE_INLINE bool Is(const GenericArrayWrapper& value) const
    {
        return TypeInfo_GetId(*value.typeInfo) == TypeId::ForType<FixedArray<T, Size>>();
    }

    HYP_FORCE_INLINE FixedArray<T, Size>& Get(const GenericArrayWrapper& value) const
    {
        return *static_cast<FixedArray<T, Size>*>(value.pInternalArray);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const FixedArray<T, Size>& value) const
    {
        HypDataHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, FixedArray<T, Size>&& value) const
    {
        HypDataHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, std::move(value)));
    }

    static FBOMResult Serialize(const FixedArray<T, Size>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        if (Size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            outData = FBOMData::FromArray(FBOMArray());

            return FBOMResult::FBOM_OK;
        }

        Array<FBOMData> elements;
        elements.Resize(Size);

        for (SizeType i = 0; i < Size; i++)
        {
            if constexpr (IsBoxedValueV<T>)
            {
                if (FBOMResult err = value[i].Serialize(elements[i], FBOMDataFlags::NONE))
                {
                    return err;
                }
            }
            else
            {
                if (FBOMResult err = HypDataHelper<T>::Serialize(value[i], elements[i], FBOMDataFlags::NONE))
                {
                    return err;
                }
            }
        }

        outData = FBOMData::FromArray(FBOMArray(std::move(elements)));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        FBOMArray array;

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
            BoxedValue element;

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

#if 0
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

    HYP_FORCE_INLINE bool Is(const GenericArrayWrapper& value) const
    {
        return value.internalArray.Is<FixedArray<T, Size>>();
    }

    HYP_FORCE_INLINE FixedArray<T, Size>& Get(const GenericArrayWrapper& value) const
    {
        return value.internalArray.Get<FixedArray<T, Size>>();
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const T (&value)[Size]) const
    {
        HypDataHelper<FixedArray<T, Size>>::Set(boxed, MakeFixedArray(value));
    }

    static FBOMResult Serialize(const T (&value)[Size], FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        if (Size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            outData = FBOMData::FromArray(FBOMArray());

            return FBOMResult::FBOM_OK;
        }

        Array<FBOMData> elements;
        elements.Resize(Size);

        for (SizeType i = 0; i < Size; i++)
        {
            if constexpr (IsBoxedValueV<T>)
            {
                if (FBOMResult err = value[i].Serialize(elements[i], FBOMDataFlags::NONE))
                {
                    return err;
                }
            }
            else
            {
                if (FBOMResult err = HypDataHelper<T>::Serialize(value[i], elements[i], FBOMDataFlags::NONE))
                {
                    return err;
                }
            }
        }

        outData = FBOMData::FromArray(FBOMArray(std::move(elements)));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        FBOMArray array;

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
            BoxedValue element;

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
#endif

/// Pair

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const Pair<K, V>& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<Pair<K, V>>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, Pair<K, V>&& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<Pair<K, V>>(std::move(value)));
    }

    static FBOMResult Serialize(const Pair<K, V>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        FBOMData firstData;
        FBOMData secondData;

        if (FBOMResult err = HypDataHelper<K>::Serialize(value.first, firstData, FBOMDataFlags::NONE))
        {
            return err;
        }

        if (FBOMResult err = HypDataHelper<V>::Serialize(value.second, secondData, FBOMDataFlags::NONE))
        {
            return err;
        }

        FBOMObject object(FBOMObjectType(TypeWrapper<Pair<K, V>> {}, FBOMTypeFlags::DEFAULT, FBOMBaseObjectType()));
        object.SetProperty("Key", std::move(firstData));
        object.SetProperty("Value", std::move(secondData));

        outData = FBOMData::FromObject(std::move(object));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        FBOMObject object;

        if (FBOMResult err = data.ReadObject(context, object, /* deserializeObject */ false))
        {
            return err;
        }

        BoxedValue first;
        BoxedValue second;

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

/// HashMap

template <class K, class V>
struct HypDataHelperDecl<HashMap<K, V>>
{
};

template <class K, class V>
struct HypDataHelper<HashMap<K, V>> : HypDataHelper<GenericArrayWrapper>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        if (const GenericArrayWrapper* array = value.TryGet<GenericArrayWrapper>())
        {
            return TypeInfo_GetId(*array->typeInfo) == TypeId::ForType<HashMap<K, V>>();
        }

        return value.GetTypeId() == TypeId::ForType<HashMap<K, V>>();
    }

    HYP_FORCE_INLINE HashMap<K, V>& Get(const Any& value) const
    {
        if (const GenericArrayWrapper* arr = value.TryGet<GenericArrayWrapper>())
        {
            if (TypeInfo_GetId(*arr->typeInfo) == TypeId::ForType<HashMap<K, V>>())
            {
                return *static_cast<HashMap<K, V>*>(arr->pInternalArray);
            }
        }
        else if (value.GetTypeId() == TypeId::ForType<HashMap<K, V>>())
        {
            return value.Get<HashMap<K, V>>();
        }

        HYP_UNREACHABLE();
    }

    HYP_FORCE_INLINE bool Is(const GenericArrayWrapper& value) const
    {
        return TypeInfo_GetId(*value.typeInfo) == TypeId::ForType<HashMap<K, V>>();
    }

    HYP_FORCE_INLINE HashMap<K, V>& Get(const GenericArrayWrapper& value) const
    {
        return *static_cast<HashMap<K, V>*>(value.pInternalArray);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const HashMap<K, V>& value) const
    {
        HypDataHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, HashMap<K, V>&& value) const
    {
        HypDataHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, std::move(value)));
    }

    static FBOMResult Serialize(const HashMap<K, V>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const SizeType size = value.Size();

        if (size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            outData = FBOMData::FromArray(FBOMArray());

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

        outData = FBOMData::FromArray(FBOMArray(std::move(elements)));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        FBOMArray array;

        if (FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        const SizeType size = array.Size();

        HashMap<K, V> result;

        for (SizeType i = 0; i < size; i++)
        {
            BoxedValue element;

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

/// FlatMap

template <class K, class V>
struct HypDataHelperDecl<FlatMap<K, V>>
{
};

template <class K, class V>
struct HypDataHelper<FlatMap<K, V>> : HypDataHelper<GenericArrayWrapper>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        if (const GenericArrayWrapper* array = value.TryGet<GenericArrayWrapper>())
        {
            return TypeInfo_GetId(*array->typeInfo) == TypeId::ForType<FlatMap<K, V>>();
        }

        return value.GetTypeId() == TypeId::ForType<FlatMap<K, V>>();
    }

    HYP_FORCE_INLINE FlatMap<K, V>& Get(const Any& value) const
    {
        if (const GenericArrayWrapper* arr = value.TryGet<GenericArrayWrapper>())
        {
            if (TypeInfo_GetId(*arr->typeInfo) == TypeId::ForType<FlatMap<K, V>>())
            {
                return *static_cast<FlatMap<K, V>*>(arr->pInternalArray);
            }
        }
        else if (value.GetTypeId() == TypeId::ForType<FlatMap<K, V>>())
        {
            return value.Get<FlatMap<K, V>>();
        }

        HYP_UNREACHABLE();
    }

    HYP_FORCE_INLINE bool Is(const GenericArrayWrapper& value) const
    {
        return TypeInfo_GetId(*value.typeInfo) == TypeId::ForType<FlatMap<K, V>>();
    }

    HYP_FORCE_INLINE FlatMap<K, V>& Get(const GenericArrayWrapper& value) const
    {
        return *static_cast<FlatMap<K, V>*>(value.pInternalArray);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const FlatMap<K, V>& value) const
    {
        HypDataHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, FlatMap<K, V>&& value) const
    {
        HypDataHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, std::move(value)));
    }

    static FBOMResult Serialize(const FlatMap<K, V>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const SizeType size = value.Size();

        if (size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            outData = FBOMData::FromArray(FBOMArray());

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

        outData = FBOMData::FromArray(FBOMArray(std::move(elements)));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        FBOMArray array;

        if (FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        const SizeType size = array.Size();

        FlatMap<K, V> result;

        for (SizeType i = 0; i < size; i++)
        {
            BoxedValue element;

            if (FBOMResult err = HypDataHelper<Pair<K, V>>::Deserialize(context, array.GetElement(i), element))
            {
                return err;
            }

            result.Insert(std::move(element.Get<Pair<K, V>>()));
        }

        HypDataHelper<FlatMap<K, V>> {}.Set(out, std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

/// HashSet

template <class ValueType, auto KeyByFunction>
struct HypDataHelperDecl<HashSet<ValueType, KeyByFunction>>
{
};

template <class ValueType, auto KeyByFunction>
struct HypDataHelper<HashSet<ValueType, KeyByFunction>> : HypDataHelper<GenericArrayWrapper>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        if (const GenericArrayWrapper* array = value.TryGet<GenericArrayWrapper>())
        {
            return TypeInfo_GetId(*array->typeInfo) == TypeId::ForType<HashSet<ValueType, KeyByFunction>>();
        }

        return value.GetTypeId() == TypeId::ForType<HashSet<ValueType, KeyByFunction>>();
    }

    HYP_FORCE_INLINE HashSet<ValueType, KeyByFunction>& Get(const Any& value) const
    {
        if (const GenericArrayWrapper* arr = value.TryGet<GenericArrayWrapper>())
        {
            if (TypeInfo_GetId(*arr->typeInfo) == TypeId::ForType<HashSet<ValueType, KeyByFunction>>())
            {
                return *static_cast<HashSet<ValueType, KeyByFunction>*>(arr->pInternalArray);
            }
        }
        else if (value.GetTypeId() == TypeId::ForType<HashSet<ValueType, KeyByFunction>>())
        {
            return value.Get<HashSet<ValueType, KeyByFunction>>();
        }

        HYP_UNREACHABLE();
    }

    HYP_FORCE_INLINE bool Is(const GenericArrayWrapper& value) const
    {
        return TypeInfo_GetId(*value.typeInfo) == TypeId::ForType<HashSet<ValueType, KeyByFunction>>();
    }

    HYP_FORCE_INLINE HashSet<ValueType, KeyByFunction>& Get(const GenericArrayWrapper& value) const
    {
        return *static_cast<HashSet<ValueType, KeyByFunction>*>(value.pInternalArray);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const HashSet<ValueType, KeyByFunction>& value) const
    {
        HypDataHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, HashSet<ValueType, KeyByFunction>&& value) const
    {
        HypDataHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, std::move(value)));
    }

    static FBOMResult Serialize(const HashSet<ValueType, KeyByFunction>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const SizeType size = value.Size();

        if (size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            outData = FBOMData::FromArray(FBOMArray());

            return FBOMResult::FBOM_OK;
        }

        Array<FBOMData> elements;
        elements.Reserve(size);

        uint32 elementIndex = 0;

        for (const ValueType& value : value)
        {
            FBOMData& element = elements.EmplaceBack();

            if (FBOMResult err = HypDataHelper<ValueType>::Serialize(value, element, FBOMDataFlags::NONE))
            {
                return err;
            }
        }

        outData = FBOMData::FromArray(FBOMArray(std::move(elements)));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        FBOMArray array;

        if (FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        const SizeType size = array.Size();

        HashSet<ValueType, KeyByFunction> result;

        for (SizeType i = 0; i < size; i++)
        {
            BoxedValue element;

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

// FlatSet

template <class T>
struct HypDataHelperDecl<FlatSet<T>>
{
};

template <class T>
struct HypDataHelper<FlatSet<T>> : HypDataHelper<GenericArrayWrapper>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        if (const GenericArrayWrapper* array = value.TryGet<GenericArrayWrapper>())
        {
            return TypeInfo_GetId(*array->typeInfo) == TypeId::ForType<FlatSet<T>>();
        }

        return value.GetTypeId() == TypeId::ForType<FlatSet<T>>();
    }

    HYP_FORCE_INLINE FlatSet<T>& Get(const Any& value) const
    {
        if (const GenericArrayWrapper* arr = value.TryGet<GenericArrayWrapper>())
        {
            if (TypeInfo_GetId(*arr->typeInfo) == TypeId::ForType<FlatSet<T>>())
            {
                return *static_cast<FlatSet<T>*>(arr->pInternalArray);
            }
        }
        else if (value.GetTypeId() == TypeId::ForType<FlatSet<T>>())
        {
            return value.Get<FlatSet<T>>();
        }

        HYP_UNREACHABLE();
    }

    HYP_FORCE_INLINE bool Is(const GenericArrayWrapper& value) const
    {
        return TypeInfo_GetId(*value.typeInfo) == TypeId::ForType<FlatSet<T>>();
    }

    HYP_FORCE_INLINE FlatSet<T>& Get(const GenericArrayWrapper& value) const
    {
        return *static_cast<FlatSet<T>*>(value.pInternalArray);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const FlatSet<T>& value) const
    {
        HypDataHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, FlatSet<T>&& value) const
    {
        HypDataHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, std::move(value)));
    }

    static FBOMResult Serialize(const FlatSet<T>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const SizeType size = value.Size();

        if (size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            outData = FBOMData::FromArray(FBOMArray());

            return FBOMResult::FBOM_OK;
        }

        Array<FBOMData> elements;
        elements.Reserve(size);

        uint32 elementIndex = 0;

        for (const T& value : value)
        {
            FBOMData& element = elements.EmplaceBack();

            if (FBOMResult err = HypDataHelper<T>::Serialize(value, element, FBOMDataFlags::NONE))
            {
                return err;
            }
        }

        outData = FBOMData::FromArray(FBOMArray(std::move(elements)));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        FBOMArray array;

        if (FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        const SizeType size = array.Size();

        FlatSet<T> result;

        for (SizeType i = 0; i < size; i++)
        {
            BoxedValue element;

            if (FBOMResult err = HypDataHelper<T>::Deserialize(context, array.GetElement(i), element))
            {
                return err;
            }

            result.Insert(std::move(element.Get<T>()));
        }

        HypDataHelper<FlatSet<T>> {}.Set(out, std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

/// LinkedList

template <class T>
struct HypDataHelperDecl<LinkedList<T>>
{
};

template <class T>
struct HypDataHelper<LinkedList<T>> : HypDataHelper<GenericArrayWrapper>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        if (const GenericArrayWrapper* array = value.TryGet<GenericArrayWrapper>())
        {
            return TypeInfo_GetId(*array->typeInfo) == TypeId::ForType<LinkedList<T>>();
        }

        return value.GetTypeId() == TypeId::ForType<LinkedList<T>>();
    }

    HYP_FORCE_INLINE LinkedList<T>& Get(const Any& value) const
    {
        if (const GenericArrayWrapper* arr = value.TryGet<GenericArrayWrapper>())
        {
            if (TypeInfo_GetId(*arr->typeInfo) == TypeId::ForType<LinkedList<T>>())
            {
                return *static_cast<LinkedList<T>*>(arr->pInternalArray);
            }
        }
        else if (value.GetTypeId() == TypeId::ForType<LinkedList<T>>())
        {
            return value.Get<LinkedList<T>>();
        }

        HYP_UNREACHABLE();
    }

    HYP_FORCE_INLINE bool Is(const GenericArrayWrapper& value) const
    {
        return TypeInfo_GetId(*value.typeInfo) == TypeId::ForType<LinkedList<T>>();
    }

    HYP_FORCE_INLINE LinkedList<T>& Get(const GenericArrayWrapper& value) const
    {
        return *static_cast<LinkedList<T>*>(value.pInternalArray);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const LinkedList<T>& value) const
    {
        HypDataHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, LinkedList<T>&& value) const
    {
        HypDataHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, std::move(value)));
    }

    static FBOMResult Serialize(const LinkedList<T>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const SizeType size = value.Size();

        if (size == 0)
        {
            // If size is empty, serialize a placeholder value to get the element type
            outData = FBOMData::FromArray(FBOMArray());

            return FBOMResult::FBOM_OK;
        }

        Array<FBOMData> elements;
        elements.Reserve(size);

        uint32 elementIndex = 0;

        for (const T& value : value)
        {
            FBOMData& element = elements.EmplaceBack();

            if constexpr (IsBoxedValueV<T>)
            {
                if (FBOMResult err = value.Serialize(element, FBOMDataFlags::NONE))
                {
                    return err;
                }
            }
            else
            {
                if (FBOMResult err = HypDataHelper<T>::Serialize(value, element, FBOMDataFlags::NONE))
                {
                    return err;
                }
            }
        }

        outData = FBOMData::FromArray(FBOMArray(std::move(elements)));

        return FBOMResult::FBOM_OK;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        FBOMArray array;

        if (FBOMResult err = data.ReadArray(context, array))
        {
            return err;
        }

        const SizeType size = array.Size();

        LinkedList<T> result;

        for (SizeType i = 0; i < size; i++)
        {
            BoxedValue element;

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

/// Matrix and Vector types

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const math::Vec2<T>& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<math::Vec2<T>>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const math::Vec2<T>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData(value);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        math::Vec2<T> result;

        if (FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = BoxedValue(std::move(result));

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const math::Vec3<T>& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<math::Vec3<T>>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const math::Vec3<T>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData(value);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        math::Vec3<T> result;

        if (FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = BoxedValue(std::move(result));

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const math::Vec4<T>& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<math::Vec4<T>>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const math::Vec4<T>& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData(value);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        math::Vec4<T> result;

        if (FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = BoxedValue(std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<Mat3f>
{
};

template <>
struct HypDataHelper<Mat3f> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Mat3f>();
    }

    HYP_FORCE_INLINE Mat3f& Get(const Any& value) const
    {
        return value.Get<Mat3f>();
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const Mat3f& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<Mat3f>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const Mat3f& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData(value);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        Mat3f result;

        if (FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = BoxedValue(std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

template <>
struct HypDataHelperDecl<Mat4f>
{
};

template <>
struct HypDataHelper<Mat4f> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Mat4f>();
    }

    HYP_FORCE_INLINE Mat4f& Get(const Any& value) const
    {
        return value.Get<Mat4f>();
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const Mat4f& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<Mat4f>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const Mat4f& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData(value);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        Mat4f result;

        if (FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = BoxedValue(std::move(result));

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const Quaternion& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<Quaternion>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const Quaternion& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData(value);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        Quaternion result;

        if (FBOMResult err = data.Read(&result))
        {
            return err;
        }

        out = BoxedValue(std::move(result));

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const Uuid& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<Uuid>(value));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const Uuid& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData::FromStruct<Uuid>(value, flags);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        Uuid result;

        if (FBOMResult err = data.ReadStruct<Uuid>(&result))
        {
            return err;
        }

        out = BoxedValue(std::move(result));

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

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const ByteBuffer& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<ByteBuffer>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, ByteBuffer&& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<ByteBuffer>(std::move(value)));
    }

    HYP_FORCE_INLINE static FBOMResult Serialize(const ByteBuffer& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        outData = FBOMData::FromByteBuffer(value, flags);

        return FBOMResult::FBOM_OK;
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        ByteBuffer byteBuffer;

        if (FBOMResult err = data.ReadByteBuffer(byteBuffer))
        {
            return err;
        }

        out = BoxedValue(std::move(byteBuffer));

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
    using ConvertibleFrom = Tuple<>;

    template <class T>
    static FBOMResult VariantElementSerializeHelper(const Variant<Types...>& variant, FBOMData& outData)
    {
        return HypDataHelper<T>::Serialize(variant.template Get<T>(), outData);
    }

    template <class T>
    static FBOMResult VariantElementDeserializeHelper(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        BoxedValue tmp;
        if (FBOMResult err = HypDataHelper<T>::Deserialize(context, data, tmp))
        {
            return err;
        }

        out = BoxedValue(Variant<Types...>(std::move(tmp).template Get<T>()));

        return FBOMResult::FBOM_OK;
    }

    static constexpr std::add_pointer_t<FBOMResult(const Variant<Types...>&, FBOMData&)> ElementSerializeFunctions[] = { &VariantElementSerializeHelper<Types>... };
    static constexpr std::add_pointer_t<FBOMResult(FBOMLoadContext&, const FBOMData&, BoxedValue&)> ElementDeserializeFunctions[] = { &VariantElementDeserializeHelper<Types>... };

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Variant<Types...>>();
    }

    HYP_FORCE_INLINE Variant<Types...>& Get(const Any& value) const
    {
        return value.Get<Variant<Types...>>();
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<NormalizedType<T>, Variant<Types...>> && std::disjunction_v<std::is_same<NormalizedType<T>, Types>...>>>
    HYP_FORCE_INLINE constexpr bool Is(const T& value) const
    {
        return true;
    }

#if 0
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
#endif

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const Variant<Types...>& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<Variant<Types...>>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, Variant<Types...>&& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<Variant<Types...>>(std::move(value)));
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

        return ElementSerializeFunctions[typeIndex](value, outData);
    }

    HYP_FORCE_INLINE static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        if (data.IsUnset())
        {
            out = BoxedValue(Variant<Types...>());

            return FBOMResult::FBOM_OK;
        }

        int foundTypeIndex = Variant<Types...>::invalidTypeIndex;
        int currTypeIndex = 0;

        StaticForEach<Tuple<Types...>>([&]<class T>(TypeWrapper<T>)
            {
                if (foundTypeIndex != Variant<Types...>::invalidTypeIndex)
                {
                    // already found
                    return;
                }

                // check if same TypeIds, or if a HypDataHelper for T has ConvertibleFrom that is the same type id
                if (data.GetType().GetNativeTypeId() == TypeId::ForType<T>())
                {
                    foundTypeIndex = currTypeIndex;
                    return;
                }

                // Also check any types listed in HypDataHelper<T>::ConvertibleFrom
                bool matchedConvertible = false;
                StaticForEach<typename HypDataHelper<T>::ConvertibleFrom>([&]<class FromT>(TypeWrapper<FromT>)
                    {
                        if (matchedConvertible || foundTypeIndex != Variant<Types...>::invalidTypeIndex)
                        {
                            return;
                        }

                        if (data.GetType().GetNativeTypeId() == TypeId::ForType<FromT>())
                        {
                            foundTypeIndex = currTypeIndex;
                            matchedConvertible = true;
                        }
                    });

                if (matchedConvertible)
                {
                    return;
                }

                currTypeIndex++;
            });

        if (foundTypeIndex == Variant<Types...>::invalidTypeIndex)
        {
            return { FBOMResult::FBOM_ERR, "Cannot deserialize variant - type not found" };
        }

        return ElementDeserializeFunctions[foundTypeIndex](context, data, out);
    }
};

#if 1
template <class T>
struct HypDataHelper<T, std::enable_if_t<!BoxedValue::canStoreDirectly<T> && !ImplementationExistsV<HypDataHelperDecl<T>>>> : HypDataHelper<Any>
{
    using ConvertibleFrom = Tuple<T*, AnyRef, Handle<ObjectBase>, RC<void>>;

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

    HYP_FORCE_INLINE bool Is(const Handle<ObjectBase>& value) const
    {
        if constexpr (std::is_base_of_v<ObjectBase, T>)
        {
            // Dereferencing a null pointer would be bad - so we'll just pretend it's not the type
            return value && IsA(GetClass(TypeId::ForType<T>()), GetClass(value.GetTypeId()));
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

    HYP_FORCE_INLINE T& Get(const Handle<ObjectBase>& value) const
    {
        if constexpr (std::is_base_of_v<ObjectBase, T>)
        {
            return *static_cast<T*>(value.Get());
        }
        else
        {
            HYP_UNREACHABLE();
        }
    }

    HYP_FORCE_INLINE T& Get(const RC<void>& value) const
    {
        return *value.CastUnchecked<T>();
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const T& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<T>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, T&& value) const
    {
        HypDataHelper<Any>::Set(boxed, Any::Construct<T>(std::move(value)));
    }

    static FBOMResult Serialize(const T& value, FBOMData& outData, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        HYP_SCOPE;

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(TypeId::ForType<NormalizedType<T>>());

        if (!marshal)
        {
            HYP_BREAKPOINT;
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

    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMData& data, BoxedValue& out)
    {
        HYP_SCOPE;

        const FBOMMarshalerBase* marshal = FBOM::GetInstance().GetMarshal(TypeId::ForType<T>());

        if (!marshal)
        {
            return FBOMResult { FBOMResult::FBOM_ERR, "No marshal defined for type" };
        }

        if (!data)
        {
            out = BoxedValue(T {});

            return FBOMResult::FBOM_OK;
        }

        FBOMObject object;

        if (FBOMResult err = data.ReadObject(context, object, /* deserializeObject */ false))
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

#include <core/reflection/GenericArrayWrapper.inc>

#pragma region HypData_Is implementation

template <class To, class From = To>
static inline bool HypData_Is_Impl(const BoxedValue::VariantType& value)
{
    static_assert(BoxedValue::canStoreDirectly<typename HypDataHelper<From>::StorageType>, "StorageType must be a type that can be stored directly in the BoxedValue variant without allocating memory dynamically");

    constexpr bool ShouldDoAdditionalCheck = !std::is_same_v<To, typename HypDataHelper<From>::StorageType>;

    return value.Is<typename HypDataHelper<From>::StorageType>()
        && (!ShouldDoAdditionalCheck || HypDataHelper<To> {}.Is(value.GetUnchecked<typename HypDataHelper<From>::StorageType>()));
}

template <class T, class... ConvertibleFrom>
struct HypData_Is<T, Tuple<ConvertibleFrom...>>
{
    bool operator()(const BoxedValue::VariantType& value, bool checkReference) const
    {
        return (HypData_Is_Impl<T>(value) || (HypData_Is_Impl<T, ConvertibleFrom>(value) || ...))
            || (checkReference && value.Is<AnyRef>() && value.GetUnchecked<AnyRef>().template Is<T>());
    }
};

#pragma endregion HypData_Is implementation

#pragma region HypData_Get implementation

template <class VariantType, class ReturnType, class... Types, SizeType... Indices>
bool HypData_Get_Impl(VariantType&& value, Optional<ReturnType>& outValue, std::index_sequence<Indices...>)
{
    const auto getForTypeIndex = [&value]<SizeType SelectedTypeIndex>(Optional<ReturnType>& outValue, std::integral_constant<SizeType, SelectedTypeIndex>) -> bool
    {
        using SelectedType = typename TupleElement<SelectedTypeIndex, Types...>::Type;
        using StorageType = typename HypDataHelper<SelectedType>::StorageType;

        static_assert(BoxedValue::canStoreDirectly<typename HypDataHelper<NormalizedType<ReturnType>>::StorageType>);

        if (!value.template Is<StorageType>())
        {
            return false;
        }

        if constexpr (std::is_same_v<NormalizedType<ReturnType>, StorageType>)
        {
            outValue.Set(value.template Get<StorageType>());
        }
        else
        {
            decltype(auto) internalValue = value.template Get<StorageType>();

            if (!(HypDataHelper<NormalizedType<ReturnType>> {}.Is(internalValue)))
            {
                return false;
            }

            outValue.Set(HypDataHelper<NormalizedType<ReturnType>> {}.Get(internalValue));
        }

        return true;
    };

    using FirstType = typename TupleElement<0, Types...>::Type;

    return (getForTypeIndex(outValue, std::integral_constant<SizeType, Indices> {}) || ...)
        || (value.template Is<AnyRef>() && ((value.template GetUnchecked<AnyRef>().template Is<FirstType>() && (outValue.Set(value.template GetUnchecked<AnyRef>().template GetUnchecked<FirstType>()), true))));
}

template <class ReturnType, class T, class... ConvertibleFrom>
struct HypData_Get<ReturnType, T, Tuple<ConvertibleFrom...>>
{
    HYP_FORCE_INLINE bool operator()(BoxedValue::VariantType& value, Optional<ReturnType>& outValue) const
    {
        return HypData_Get_Impl<BoxedValue::VariantType&, ReturnType, T, ConvertibleFrom...>(value, outValue, std::index_sequence_for<T, ConvertibleFrom...> {});
    }

    HYP_FORCE_INLINE bool operator()(const BoxedValue::VariantType& value, Optional<ReturnType>& outValue) const
    {
        return HypData_Get_Impl<const BoxedValue::VariantType&, ReturnType, T, ConvertibleFrom...>(value, outValue, std::index_sequence_for<T, ConvertibleFrom...> {});
    }
};

#pragma endregion HypData_Get implementation

static_assert(sizeof(BoxedValue) == 32, "sizeof(BoxedValue) != 32 bytes");

} // namespace Hyperion
