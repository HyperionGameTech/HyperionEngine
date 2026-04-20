/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/BoxedValueFwd.hpp>

#include <Core/reflection/ObjId.hpp>
#include <Core/reflection/Handle.hpp>
#include <Core/reflection/ObjectFwd.hpp>
#include <Core/reflection/GenericArrayWrapper.hpp>
#include <Core/reflection/TypeInfoFwd.hpp>

#include <Core/containers/Array.hpp>

#include <Core/filesystem/FilePath.hpp>

#include <Core/utilities/Variant.hpp>
#include <Core/utilities/Optional.hpp>
#include <Core/utilities/StringView.hpp>
#include <Core/utilities/Pair.hpp>
#include <Core/utilities/Tuple.hpp>
#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/Float16.hpp>
#include <Core/utilities/Result.hpp>
#include <Core/utilities/Uuid.hpp>

#include <Core/memory/Any.hpp>
#include <Core/memory/RefCountedPtr.hpp>
#include <Core/memory/ByteBuffer.hpp>

#include <Core/profiling/ProfileScope.hpp>

#include <Core/Types.hpp>

#include <type_traits>

namespace Hyperion {

namespace filesystem {
class FilePath;
} // namespace filesystem

using filesystem::FilePath;

HYP_API extern const Class* GetClass(const TypeId& typeId);
HYP_API extern bool IsA(const Class* cls, const Class* instanceClass);

template <class T, class T2 = void>
struct BoxedValueHelper;

template <class T, class T2 = void>
struct BoxedValueHelperDecl;

template <class T, class ConvertibleFromTuple>
struct BoxedValue_Is;

template <class ReturnType, class T, class ConvertibleFromTuple>
struct BoxedValue_Get;

template <class T, bool IsConst>
struct GetReturnTypeHelper
{
    using Type = decltype(std::declval<BoxedValueHelper<T>>().Get(std::declval<std::conditional_t<IsConst, const typename BoxedValueHelper<T>::StorageType&, typename BoxedValueHelper<T>::StorageType&>>()));
};

template <>
struct GetReturnTypeHelper<BoxedValue, false>
{
    using Type = BoxedValue&;
};

template <>
struct GetReturnTypeHelper<BoxedValue, true>
{
    using Type = const BoxedValue&;
};

HYP_API extern BoxedValueSerializeFunction GetBoxedValueSerializeFunction(TypeId typeId);
HYP_API extern void RegisterBoxedValueSerializeFunction(TypeId typeId, BoxedValueSerializeFunction func);

struct GenericArrayWrapper;

#ifdef HYP_SCRIPT
enum class GCIndex : uint32;

static constexpr GCIndex INVALID_GC_INDEX = GCIndex(0);
static constexpr GCIndex GARBAGE_GC_INDEX = GCIndex(1u << 30);

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
    struct InlineData
    {
        uint64 data[2];

        HYP_FORCE_INLINE bool operator==(const InlineData& other) const
        {
            return Memory::Compare(this, &other, sizeof(InlineData)) == 0;
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

    union
    {
#ifdef HYP_SCRIPT
        // HypScript only - object metadata
        struct
        {
            GCIndex gcIndex : 31;   // index into the pool of tracked objects
            uint8 isStaticInit : 1; // static data pool / stack data - will be 1 if init
        };
#endif
        uint32 dummy;
    } extData;

    BoxedValue()
    {
        extData = {};
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<NormalizedType<T>, BoxedValue>>>
    explicit BoxedValue(T&& value)
        : BoxedValue()
    {
        BoxedValueHelper<NormalizedType<T>> {}.Set(*this, std::forward<T>(value));
    }

    BoxedValue(const BoxedValue& other)
        : value(other.value)
    {
        extData = other.extData;
    }

    BoxedValue& operator=(const BoxedValue& other)
    {
        if (&other == this)
        {
            return *this;
        }

        value = other.value;
        extData = other.extData;

        return *this;
    }

    BoxedValue(BoxedValue&& other) noexcept
        : value(std::move(other.value))
    {
        extData = other.extData;
        other.extData = {};
    }

    BoxedValue& operator=(BoxedValue&& other) noexcept
    {
        if (&other == this)
        {
            return *this;
        }

        value = std::move(other.value);

        extData = other.extData;
        other.extData = {};

        return *this;
    }

    ~BoxedValue()
    {
#if HYP_DEBUG_MODE
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
                return BoxedValue_Is<T, Tuple<>> {}(value, /* checkReference */ true);
            }

            return BoxedValue_Is<T, typename BoxedValueHelper<T>::ConvertibleFrom> {}(value, /* checkReference */ true);
        }
    }

    template <class T>
    auto Get() -> typename GetReturnTypeHelper<T, false>::Type
    {
        if constexpr (std::is_same_v<T, BoxedValue>)
        {
            return *this;
        }
        else
        {
#if HYP_DEBUG_MODE
            HYP_CORE_ASSERT(Is<T>(), "Expected %s, got %s", TypeName<T>().Data(), *TypeInfo_GetName(*GetTypeInfo()));
#endif

            using ReturnType = typename GetReturnTypeHelper<T, false>::Type;

            Optional<ReturnType> resultValue;
            BoxedValue_Get<ReturnType, T, typename BoxedValueHelper<T>::ConvertibleFrom> {}(value, resultValue);

#if HYP_DEBUG_MODE
            HYP_CORE_ASSERT(resultValue.HasValue(),
                "Failed to invoke BoxedValue Get method with T = %s - Mismatched types or T could not be converted to the held type (%s)",
                TypeName<T>().Data(),
                *TypeInfo_GetName(*GetTypeInfo()));
#endif

            return *resultValue;
        }
    }

    template <class T>
    auto Get() const -> typename GetReturnTypeHelper<T, true>::Type
    {
        if constexpr (std::is_same_v<T, BoxedValue>)
        {
            return *this;
        }
        else
        {
#if HYP_DEBUG_MODE
            HYP_CORE_ASSERT(Is<T>(), "Expected %s, got %s", TypeName<T>().Data(), *TypeInfo_GetName(*GetTypeInfo()));
#endif

            using ReturnType = typename GetReturnTypeHelper<T, true>::Type;

            Optional<ReturnType> resultValue;
            BoxedValue_Get<ReturnType, T, typename BoxedValueHelper<T>::ConvertibleFrom> {}(value, resultValue);

#if HYP_DEBUG_MODE
            HYP_CORE_ASSERT(resultValue.HasValue(),
                "Failed to invoke BoxedValue Get method with T = %s - Mismatched types or T could not be converted to the held type (%s)",
                TypeName<T>().Data(),
                *TypeInfo_GetName(*GetTypeInfo()));
#endif

            return *resultValue;
        }
    }

    template <class T>
    auto TryGet() -> Optional<typename GetReturnTypeHelper<T, false>::Type>
    {
        if constexpr (std::is_same_v<T, BoxedValue>)
        {
            return *this;
        }
        else
        {
            using ReturnType = typename GetReturnTypeHelper<T, false>::Type;

            Optional<ReturnType> resultValue;
            BoxedValue_Get<ReturnType, T, typename BoxedValueHelper<T>::ConvertibleFrom> {}(value, resultValue);

            return resultValue;
        }
    }

    template <class T>
    auto TryGet() const -> Optional<typename GetReturnTypeHelper<T, true>::Type>
    {
        if constexpr (std::is_same_v<T, BoxedValue>)
        {
            return *this;
        }
        else
        {
            using ReturnType = typename GetReturnTypeHelper<T, true>::Type;

            Optional<ReturnType> resultValue;
            BoxedValue_Get<ReturnType, T, typename BoxedValueHelper<T>::ConvertibleFrom> {}(value, resultValue);

            return resultValue;
        }
    }

    template <class T>
    void Set_Internal(T&& value)
    {
        this->value.Set<NormalizedType<T>>(std::forward<T>(value));
    }
};

template <class T>
struct BoxedValueHelperDecl<T, std::enable_if_t<std::is_fundamental_v<T>>>
{
};

template <class T>
struct BoxedValueHelper<T, std::enable_if_t<std::is_fundamental_v<T>>>
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
};

#if !HYP_WINDOWS

template <class T>
struct BoxedValueHelperDecl<T, std::enable_if_t<std::is_same_v<size_t, T> && !std::is_same_v<size_t, uint64>>>
{
};

template <class T>
struct BoxedValueHelper<T, std::enable_if_t<std::is_same_v<size_t, T> && !std::is_same_v<size_t, uint64>>> : BoxedValueHelper<uint64>
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

    HYP_FORCE_INLINE bool Is(size_t value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<OtherT, size_t>>>
    HYP_FORCE_INLINE bool Is(OtherT value) const
    {
        return std::is_fundamental_v<OtherT>;
    }

    HYP_FORCE_INLINE constexpr size_t Get(size_t value) const
    {
        return value;
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<OtherT, size_t>>>
    HYP_FORCE_INLINE constexpr size_t Get(OtherT value) const
    {
        return static_cast<size_t>(value);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, size_t value) const
    {
        boxed.Set_Internal(static_cast<uint64>(value));
    }
};

#endif

template <>
struct BoxedValueHelperDecl<Float16>
{
};

template <>
struct BoxedValueHelper<Float16> : BoxedValueHelper<uint16>
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
};

template <class T>
struct BoxedValueHelperDecl<T, std::enable_if_t<std::is_enum_v<T>>>
{
};

template <class T>
struct BoxedValueHelper<T, std::enable_if_t<std::is_enum_v<T>>> : BoxedValueHelper<std::underlying_type_t<T>>
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
        BoxedValueHelper<std::underlying_type_t<T>>::Set(boxed, static_cast<std::underlying_type_t<T>>(value));
    }
};

template <class T>
struct BoxedValueHelperDecl<EnumFlags<T>>
{
};

template <class T>
struct BoxedValueHelper<EnumFlags<T>> : BoxedValueHelper<T>
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
        BoxedValueHelper<typename EnumFlags<T>::UnderlyingType>::Set(boxed, static_cast<typename EnumFlags<T>::UnderlyingType>(value));
    }
};

/* void pointer specialization - only meant for runtime, non-serializable. */
template <>
struct BoxedValueHelperDecl<void*>
{
};

template <>
struct BoxedValueHelper<void*>
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
};

/// ObjIdBase specialization - stores as ObjIdBase internally, ObjId<T> converts to/from this.

template <>
struct BoxedValueHelperDecl<ObjIdBase>
{
};

template <>
struct BoxedValueHelper<ObjIdBase>
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
};

/// ObjId<T> specialization - stores as ObjIdBase internally, converts to/from ObjIdBase and Handle<ObjectBase>.

template <class T>
struct BoxedValueHelperDecl<ObjId<T>>
{
};

template <class T>
struct BoxedValueHelper<ObjId<T>> : BoxedValueHelper<ObjIdBase>
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
        BoxedValueHelper<ObjIdBase>::Set(boxed, static_cast<const ObjIdBase&>(value));
    }
};

/// ClassRef specialization - stores as ClassRef internally, not serializable.

template <>
struct BoxedValueHelperDecl<ClassRef>
{
};

template <>
struct BoxedValueHelper<ClassRef>
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
};

/// Handle<ObjectBase> specialization - stores as Handle<ObjectBase> internally, serializable

template <>
struct BoxedValueHelperDecl<Handle<ObjectBase>>
{
};

template <>
struct BoxedValueHelper<Handle<ObjectBase>>
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
};

/// Handle<T> specialization - stores as Handle<ObjectBase> internally, converts to/from Handle<ObjectBase>

template <class T>
struct BoxedValueHelperDecl<Handle<T>, std::enable_if_t<!std::is_same_v<T, ObjectBase>>>
{
};

template <class T>
struct BoxedValueHelper<Handle<T>> : BoxedValueHelper<Handle<ObjectBase>, std::enable_if_t<!std::is_same_v<T, ObjectBase>>>
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
        BoxedValueHelper<Handle<ObjectBase>>::Set(boxed, reinterpret_cast<const Handle<ObjectBase>&>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, Handle<T>&& value) const
    {
        BoxedValueHelper<Handle<ObjectBase>>::Set(boxed, reinterpret_cast<Handle<ObjectBase>&&>(std::move(value)));
    }
};

#if 1

/// Objects can be stored inline via Handle<ObjectBase> like Handle<T>, and converted to/from Handle<T>

template <class T>
struct BoxedValueHelperDecl<T, std::enable_if_t<std::is_base_of_v<ObjectBase, T>>>
{
};

template <class T>
struct BoxedValueHelper<T, std::enable_if_t<std::is_base_of_v<ObjectBase, T>>> : BoxedValueHelper<Handle<T>>
{
    using ConvertibleFrom = Tuple<AnyRef>;

    HYP_FORCE_INLINE bool Is(const Handle<ObjectBase>& value) const
    {
        return value && BoxedValueHelper<Handle<T>>::Is(value);
    }

    HYP_FORCE_INLINE T& Get(const Handle<ObjectBase>& value) const
    {
        return *BoxedValueHelper<Handle<T>>::Get(value);
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
        BoxedValueHelper<Handle<T>>::Set(boxed, value.HandleFromThis());
    }
};

#endif

/// RefCountedPtr void type can be used to hold any other RefCountedPtr type

template <>
struct BoxedValueHelperDecl<RC<void>>
{
};

template <>
struct BoxedValueHelper<RC<void>>
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
};

template <class T>
struct BoxedValueHelperDecl<RC<T>, std::enable_if_t<!std::is_void_v<T>>>
{
};

template <class T>
struct BoxedValueHelper<RC<T>, std::enable_if_t<!std::is_void_v<T>>> : BoxedValueHelper<RC<void>>
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
        BoxedValueHelper<RC<void>>::Set(boxed, value);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, RC<T>&& value) const
    {
        BoxedValueHelper<RC<void>>::Set(boxed, std::move(value));
    }
};

/// AnyRef - type erased reference - @TODO: Add ConstAnyRef support

template <>
struct BoxedValueHelperDecl<AnyRef>
{
};

template <>
struct BoxedValueHelper<AnyRef>
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
};

/// T* - raw pointer (non-owning, non-const) held as AnyRef

template <class T>
struct BoxedValueHelperDecl<T*, std::enable_if_t<!is_const_pointer_v<T*> && !std::is_same_v<T*, void*>>>
{
};

template <class T>
struct BoxedValueHelper<T*, std::enable_if_t<!is_const_pointer_v<T*> && !std::is_same_v<T*, void*>>> : BoxedValueHelper<AnyRef>
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
        BoxedValueHelper<AnyRef>::Set(boxed, AnyRef(&TypeOf<T>(), value));
    }
};

/// const T* - raw pointer (non-owning, const) held as AnyRef

template <class T>
struct BoxedValueHelperDecl<const T*, std::enable_if_t<!std::is_same_v<T*, void*>>>
{
};

template <class T>
struct BoxedValueHelper<const T*, std::enable_if_t<!std::is_same_v<T*, void*>>> : BoxedValueHelper<T*>
{
    HYP_FORCE_INLINE const T* Get(const ConstAnyRef& value) const
    {
        return BoxedValueHelper<T*>::Get(AnyRef(value.GetTypeInfo(), const_cast<void*>(value.GetPointer())));
    }

    HYP_FORCE_INLINE const T* Get(const AnyRef& value) const
    {
        return BoxedValueHelper<T*>::Get(value);
    }

    HYP_FORCE_INLINE const T* Get(const Handle<ObjectBase>& value) const
    {
        return BoxedValueHelper<T*>::Get(value);
    }

    HYP_FORCE_INLINE const T* Get(const RC<void>& value) const
    {
        return BoxedValueHelper<T*>::Get(value);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const T* value) const
    {
        BoxedValueHelper<T*>::Set(boxed, const_cast<T*>(value));
    }
};

/// Any - type erased value, allocated on the heap

template <>
struct BoxedValueHelperDecl<Any>
{
};

template <>
struct BoxedValueHelper<Any>
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
};

template <>
struct BoxedValueHelperDecl<GenericArrayWrapper>
{
};

template <>
struct BoxedValueHelper<GenericArrayWrapper> : BoxedValueHelper<Any>
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
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<GenericArrayWrapper>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, GenericArrayWrapper&& value) const
    {
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<GenericArrayWrapper>(std::move(value)));
    }
};

template <>
struct BoxedValueHelperDecl<BoxedValue::InlineData>
{
};

template <>
struct BoxedValueHelper<BoxedValue::InlineData>
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
};

/// String types

template <int StringType>
struct BoxedValueHelperDecl<containers::String<StringType>>
{
};

template <int StringType>
struct BoxedValueHelper<containers::String<StringType>> : BoxedValueHelper<Any>
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
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<containers::String<StringType>>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, containers::String<StringType>&& value) const
    {
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<containers::String<StringType>>(std::move(value)));
    }
};

/// StringView types - held as String, memory is owned

template <int StringType>
struct BoxedValueHelperDecl<utilities::StringView<StringType>>
{
};

template <int StringType>
struct BoxedValueHelper<utilities::StringView<StringType>> : BoxedValueHelper<containers::String<StringType>>
{
    using ConvertibleFrom = Tuple<containers::String<StringType>>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return BoxedValueHelper<containers::String<StringType>>::Is(value);
    }

    HYP_FORCE_INLINE utilities::StringView<StringType> Get(const Any& value) const
    {
        return BoxedValueHelper<containers::String<StringType>>::Get(value);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const utilities::StringView<StringType>& value) const
    {
        BoxedValueHelper<containers::String<StringType>>::Set(boxed, value);
    }
};

/// C String - converted to String, memory is owned

template <>
struct BoxedValueHelperDecl<const char*>
{
};

template <>
struct BoxedValueHelper<const char*> : BoxedValueHelper<String>
{
    using ConvertibleFrom = Tuple<String>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return BoxedValueHelper<String>::Is(value);
    }

    HYP_FORCE_INLINE const char* Get(const Any& value) const
    {
        return BoxedValueHelper<String>::Get(value).Data();
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const char* value) const
    {
        BoxedValueHelper<String>::Set(boxed, String(value));
    }
};

/// FilePath - stored as String (base class of FilePath)

template <>
struct BoxedValueHelperDecl<FilePath>
{
};

template <>
struct BoxedValueHelper<FilePath> : BoxedValueHelper<String>
{
    HYP_FORCE_INLINE FilePath Get(const Any& value) const
    {
        return value.Get<String>();
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const FilePath& value) const
    {
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<String>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, FilePath&& value) const
    {
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<String>(std::move(value)));
    }
};

/// Name and StringHash - stored as String value

template <>
struct BoxedValueHelperDecl<Name>
{
};

template <>
struct BoxedValueHelper<Name>
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
};

template <>
struct BoxedValueHelperDecl<StringHash>
{
};

template <>
struct BoxedValueHelper<StringHash>
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
};

/// Array types

template <class T, class AllocatorType>
struct BoxedValueHelperDecl<Array<T, AllocatorType>, std::enable_if_t<!std::is_const_v<T>>>
{
};

template <class T, class AllocatorType>
struct BoxedValueHelper<Array<T, AllocatorType>, std::enable_if_t<!std::is_const_v<T>>> : BoxedValueHelper<GenericArrayWrapper>
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
        BoxedValueHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, Array<T, AllocatorType>&& value) const
    {
        BoxedValueHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, std::move(value)));
    }
};

/// FixedArray

template <class T, size_t Size>
struct BoxedValueHelperDecl<FixedArray<T, Size>>
{
};

template <class T, size_t Size>
struct BoxedValueHelper<FixedArray<T, Size>, std::enable_if_t<!std::is_const_v<T>>> : BoxedValueHelper<GenericArrayWrapper>
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
        BoxedValueHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, FixedArray<T, Size>&& value) const
    {
        BoxedValueHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, std::move(value)));
    }
};

#if 0
template <class T, size_t Size>
struct BoxedValueHelperDecl<T[Size]>
{
};

template <class T, size_t Size>
struct BoxedValueHelper<T[Size], std::enable_if_t<!std::is_const_v<T>>> : BoxedValueHelper<FixedArray<T, Size>>
{
    using ConvertibleFrom = Tuple<FixedArray<T, Size>>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return BoxedValueHelper<FixedArray<T, Size>>::Is(value);
    }

    HYP_FORCE_INLINE FixedArray<T, Size>& Get(const Any& value) const
    {
        return BoxedValueHelper<FixedArray<T, Size>>::Get(value);
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
        BoxedValueHelper<FixedArray<T, Size>>::Set(boxed, MakeFixedArray(value));
    }
};
#endif

/// Pair

template <class K, class V>
struct BoxedValueHelperDecl<Pair<K, V>>
{
};

template <class K, class V>
struct BoxedValueHelper<Pair<K, V>> : BoxedValueHelper<Any>
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
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<Pair<K, V>>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, Pair<K, V>&& value) const
    {
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<Pair<K, V>>(std::move(value)));
    }
};

/// HashMap

template <class K, class V>
struct BoxedValueHelperDecl<HashMap<K, V>>
{
};

template <class K, class V>
struct BoxedValueHelper<HashMap<K, V>> : BoxedValueHelper<GenericArrayWrapper>
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
        BoxedValueHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, HashMap<K, V>&& value) const
    {
        BoxedValueHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, std::move(value)));
    }
};

/// FlatMap

template <class K, class V>
struct BoxedValueHelperDecl<FlatMap<K, V>>
{
};

template <class K, class V>
struct BoxedValueHelper<FlatMap<K, V>> : BoxedValueHelper<GenericArrayWrapper>
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
        BoxedValueHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, FlatMap<K, V>&& value) const
    {
        BoxedValueHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, std::move(value)));
    }
};

/// HashSet

template <class ValueType>
struct BoxedValueHelperDecl<HashSet<ValueType>>
{
};

template <class ValueType>
struct BoxedValueHelper<HashSet<ValueType>> : BoxedValueHelper<GenericArrayWrapper>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        if (const GenericArrayWrapper* array = value.TryGet<GenericArrayWrapper>())
        {
            return TypeInfo_GetId(*array->typeInfo) == TypeId::ForType<HashSet<ValueType>>();
        }

        return value.GetTypeId() == TypeId::ForType<HashSet<ValueType>>();
    }

    HYP_FORCE_INLINE HashSet<ValueType>& Get(const Any& value) const
    {
        if (const GenericArrayWrapper* arr = value.TryGet<GenericArrayWrapper>())
        {
            if (TypeInfo_GetId(*arr->typeInfo) == TypeId::ForType<HashSet<ValueType>>())
            {
                return *static_cast<HashSet<ValueType>*>(arr->pInternalArray);
            }
        }
        else if (value.GetTypeId() == TypeId::ForType<HashSet<ValueType>>())
        {
            return value.Get<HashSet<ValueType>>();
        }

        HYP_UNREACHABLE();
    }

    HYP_FORCE_INLINE bool Is(const GenericArrayWrapper& value) const
    {
        return TypeInfo_GetId(*value.typeInfo) == TypeId::ForType<HashSet<ValueType>>();
    }

    HYP_FORCE_INLINE HashSet<ValueType>& Get(const GenericArrayWrapper& value) const
    {
        return *static_cast<HashSet<ValueType>*>(value.pInternalArray);
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const HashSet<ValueType>& value) const
    {
        BoxedValueHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, HashSet<ValueType>&& value) const
    {
        BoxedValueHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, std::move(value)));
    }
};

// FlatSet

template <class T>
struct BoxedValueHelperDecl<FlatSet<T>>
{
};

template <class T>
struct BoxedValueHelper<FlatSet<T>> : BoxedValueHelper<GenericArrayWrapper>
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
        BoxedValueHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, FlatSet<T>&& value) const
    {
        BoxedValueHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, std::move(value)));
    }
};

/// LinkedList

template <class T>
struct BoxedValueHelperDecl<LinkedList<T>>
{
};

template <class T>
struct BoxedValueHelper<LinkedList<T>> : BoxedValueHelper<GenericArrayWrapper>
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
        BoxedValueHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, LinkedList<T>&& value) const
    {
        BoxedValueHelper<GenericArrayWrapper>::Set(boxed, GenericArrayWrapper(GenericArrayWrapper::AS_COPY, std::move(value)));
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
struct BoxedValueHelperDecl<math::Vec2<T>>
{
};

template <class T>
struct BoxedValueHelper<math::Vec2<T>> : BoxedValueHelper<Any>
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
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<math::Vec2<T>>(value));
    }
};

template <class T>
struct BoxedValueHelperDecl<math::Vec3<T>>
{
};

template <class T>
struct BoxedValueHelper<math::Vec3<T>> : BoxedValueHelper<Any>
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
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<math::Vec3<T>>(value));
    }
};

template <class T>
struct BoxedValueHelperDecl<math::Vec4<T>>
{
};

template <class T>
struct BoxedValueHelper<math::Vec4<T>> : BoxedValueHelper<Any>
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
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<math::Vec4<T>>(value));
    }
};

template <>
struct BoxedValueHelperDecl<Mat3f>
{
};

template <>
struct BoxedValueHelper<Mat3f> : BoxedValueHelper<Any>
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
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<Mat3f>(value));
    }
};

template <>
struct BoxedValueHelperDecl<Mat4f>
{
};

template <>
struct BoxedValueHelper<Mat4f> : BoxedValueHelper<Any>
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
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<Mat4f>(value));
    }
};

template <>
struct BoxedValueHelperDecl<Quat4f>
{
};

template <>
struct BoxedValueHelper<Quat4f> : BoxedValueHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<Quat4f>();
    }

    HYP_FORCE_INLINE Quat4f& Get(const Any& value) const
    {
        return value.Get<Quat4f>();
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const Quat4f& value) const
    {
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<Quat4f>(value));
    }
};

template <>
struct BoxedValueHelperDecl<UUID>
{
};

template <>
struct BoxedValueHelper<UUID> : BoxedValueHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE bool Is(const Any& value) const
    {
        return value.Is<UUID>();
    }

    HYP_FORCE_INLINE UUID& Get(const Any& value) const
    {
        return value.Get<UUID>();
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const UUID& value) const
    {
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<UUID>(value));
    }
};

template <>
struct BoxedValueHelperDecl<ByteBuffer>
{
};

template <>
struct BoxedValueHelper<ByteBuffer> : BoxedValueHelper<Any>
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
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<ByteBuffer>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, ByteBuffer&& value) const
    {
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<ByteBuffer>(std::move(value)));
    }
};

template <class... Types>
struct BoxedValueHelperDecl<Variant<Types...>>
{
};

template <class... Types>
struct BoxedValueHelper<Variant<Types...>> : BoxedValueHelper<Any>
{
    using ConvertibleFrom = Tuple<>;

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, const Variant<Types...>& value) const
    {
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<Variant<Types...>>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, Variant<Types...>&& value) const
    {
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<Variant<Types...>>(std::move(value)));
    }
};

#if 1
template <class T>
struct BoxedValueHelper<T, std::enable_if_t<!BoxedValue::canStoreDirectly<T> && !implementation_exists_v<BoxedValueHelperDecl<T>>>> : BoxedValueHelper<Any>
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
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<T>(value));
    }

    HYP_FORCE_INLINE void Set(BoxedValue& boxed, T&& value) const
    {
        BoxedValueHelper<Any>::Set(boxed, Any::Construct<T>(std::move(value)));
    }
};
#endif

#include <Core/reflection/GenericArrayWrapper.inc>

#pragma region BoxedValue_Is implementation

template <class To, class From = To>
static inline bool BoxedValue_Is_Impl(const BoxedValue::VariantType& value)
{
    static_assert(BoxedValue::canStoreDirectly<typename BoxedValueHelper<From>::StorageType>, "StorageType must be a type that can be stored directly in the BoxedValue variant without allocating memory dynamically");

    constexpr bool ShouldDoAdditionalCheck = !std::is_same_v<To, typename BoxedValueHelper<From>::StorageType>;

    return value.Is<typename BoxedValueHelper<From>::StorageType>()
        && (!ShouldDoAdditionalCheck || BoxedValueHelper<To> {}.Is(value.GetUnchecked<typename BoxedValueHelper<From>::StorageType>()));
}

template <class T, class... ConvertibleFrom>
struct BoxedValue_Is<T, Tuple<ConvertibleFrom...>>
{
    bool operator()(const BoxedValue::VariantType& value, bool checkReference) const
    {
        return (BoxedValue_Is_Impl<T>(value) || (BoxedValue_Is_Impl<T, ConvertibleFrom>(value) || ...))
            || (checkReference && value.Is<AnyRef>() && value.GetUnchecked<AnyRef>().template Is<T>());
    }
};

#pragma endregion BoxedValue_Is implementation

#pragma region BoxedValue_Get implementation

template <class VariantType, class ReturnType, class... Types, size_t... Indices>
bool BoxedValue_Get_Impl(VariantType&& value, Optional<ReturnType>& outValue, std::index_sequence<Indices...>)
{
    const auto getForTypeIndex = [&value]<size_t SelectedTypeIndex>(Optional<ReturnType>& outValue, std::integral_constant<size_t, SelectedTypeIndex>) -> bool
    {
        using SelectedType = typename TupleElement<SelectedTypeIndex, Types...>::Type;
        using StorageType = typename BoxedValueHelper<SelectedType>::StorageType;

        static_assert(BoxedValue::canStoreDirectly<typename BoxedValueHelper<NormalizedType<ReturnType>>::StorageType>);

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

            if (!(BoxedValueHelper<NormalizedType<ReturnType>> {}.Is(internalValue)))
            {
                return false;
            }

            outValue.Set(BoxedValueHelper<NormalizedType<ReturnType>> {}.Get(internalValue));
        }

        return true;
    };

    using FirstType = typename TupleElement<0, Types...>::Type;

    return (getForTypeIndex(outValue, std::integral_constant<size_t, Indices> {}) || ...)
        || (value.template Is<AnyRef>() && ((value.template GetUnchecked<AnyRef>().template Is<FirstType>() && (outValue.Set(value.template GetUnchecked<AnyRef>().template GetUnchecked<FirstType>()), true))));
}

template <class ReturnType, class T, class... ConvertibleFrom>
struct BoxedValue_Get<ReturnType, T, Tuple<ConvertibleFrom...>>
{
    HYP_FORCE_INLINE bool operator()(BoxedValue::VariantType& value, Optional<ReturnType>& outValue) const
    {
        return BoxedValue_Get_Impl<BoxedValue::VariantType&, ReturnType, T, ConvertibleFrom...>(value, outValue, std::index_sequence_for<T, ConvertibleFrom...> {});
    }

    HYP_FORCE_INLINE bool operator()(const BoxedValue::VariantType& value, Optional<ReturnType>& outValue) const
    {
        return BoxedValue_Get_Impl<const BoxedValue::VariantType&, ReturnType, T, ConvertibleFrom...>(value, outValue, std::index_sequence_for<T, ConvertibleFrom...> {});
    }
};

#pragma endregion BoxedValue_Get implementation

static_assert(sizeof(BoxedValue) == 32 || always_fail_v<std::integral_constant<size_t, sizeof(BoxedValue)>>);

} // namespace Hyperion
