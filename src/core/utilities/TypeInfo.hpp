/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/TypeId.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <core/memory/AnyRef.hpp>
#include <core/memory/Any.hpp>

#include <core/Name.hpp>
#include <core/Types.hpp>
#include <core/Traits.hpp>
#include <core/HashCode.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/String.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <type_traits>

namespace hyperion {

enum class TypeInfoFlags : uint32
{
    NONE = 0x0,
    POD_TYPE = 0x1,
    CLASS_TYPE = 0x2,
    STRUCT_TYPE = 0x4,
    CLASS_OR_STRUCT_TYPE = CLASS_TYPE | STRUCT_TYPE,
    ENUM_TYPE = 0x8,
    ENUM_FLAGS_TYPE = 0x10,
    FUNDAMENTAL_TYPE = 0x20,
    INTEGRAL_TYPE = 0x40,
    FLOAT_TYPE = 0x80,

    // Container types
    ARRAY_TYPE = 0x2000,
    LINKEDLIST_TYPE = 0x8000,
    STRING_TYPE = 0x10000,
    MAP_TYPE = 0x20000,
    SET_TYPE = 0x40000,
    VARIANT_TYPE = 0x100000,

    // Vector types
    VEC2_TYPE = 0x200000,
    VEC3_TYPE = 0x400000,
    VEC4_TYPE = 0x800000,
    VECTOR_TYPE = VEC2_TYPE | VEC3_TYPE | VEC4_TYPE,

    // Matrix types
    MAT3_TYPE = 0x1000000,
    MAT4_TYPE = 0x2000000,
    MATRIX_TYPE = MAT3_TYPE | MAT4_TYPE
};

HYP_MAKE_ENUM_FLAGS(TypeInfoFlags)

class HypClass;
struct HypData;
struct GenericArrayWrapper;

struct Float16;

HYP_API extern const HypClass* GetClass(TypeId typeId);
HYP_API extern bool HypClassRegistry_IsInitialized();
namespace containers {

template <int TStringType>
class String;

template <class T>
class LinkedList;

template <class Key, class Value, class NodeAllocatorType>
class HashMap;

template <class Value, auto KeyBy, class NodeAllocatorType>
class HashSet;

template <class Key, class Value>
class FlatMap;

template <class T>
class FlatSet;

template <class Key, class Value>
class ArrayMap;

} // namespace containers

namespace math {

template <class T>
struct Vec2;

template <class T>
struct Vec3;

template <class T>
struct Vec4;

} // namespace math

struct Mat3f;
struct Mat4f;

namespace utilities {

template <class... Types>
struct Variant;

} // namespace utilities

namespace filesystem {
class FilePath;
} // namespace filesystem

namespace utilities {

struct TypeInfo;

// Forward-declare the free helper so BuildVariantTypeArray can use it without
// needing the full TypeInfo definition yet.
template <class T>
const TypeInfo& TypeInfo_ForType();

// Helper to build a FixedArray of TypeInfo* for variant alternative types.
// Forward-declared here; the definition is placed after `TypeInfo` and
// `TypeInfo_ForType` are fully defined, to avoid referencing incomplete types
// during template definition/instantiation.
template <class NormalizedT, std::size_t... Indices>
FixedArray<const TypeInfo*, sizeof...(Indices)> BuildVariantTypeArray(std::index_sequence<Indices...>);

/*! \brief Get cached type attributes for a given TypeId
 *  \param typeId The TypeId to get attributes for
 *  \param typeSize The size of the type
 *  \param typeAlignment The alignment of the type
 *  \return Pointer to TypeInfo if found, nullptr otherwise */
HYP_API extern TypeInfo* TypeInfo_FetchFromCache(TypeId typeId, uint16 typeSize, uint16 typeAlignment);

/*! \brief Allocate a TypeInfo instance from the pool
 *  \param typeId The TypeId of the type
 *  \param typeSize The size of the type
 *  \param typeAlignment The alignment of the type
 *  \param pGuard Optional pointer to construct a Mutex::Guard into, to lock the type attribute cache mutex
 *   If nullptr, it is assumed the caller already holds exclusive access to the cache mutex
 *  \return Pointer to newly allocated TypeInfo instance */
HYP_API extern TypeInfo* TypeInfo_Alloc(
    TypeId typeId, uint16 typeSize, uint16 typeAlignment,
    Mutex::Guard* pGuard);

/*! \brief Initialize the TypeInfo system, must be called before any other TypeInfo functions */
HYP_API extern void TypeInfo_Initialize();

/*! \brief Free all allocated TypeInfo instances and clear the cache */
HYP_API extern void TypeInfo_Shutdown();

class ITypeInfoHandler
{
public:
    enum Type
    {
        TYPE_NONE,
        TYPE_ARRAY,
        TYPE_LINKEDLIST,
        TYPE_MAP,
        TYPE_STRING,
        TYPE_VECTOR,
        TYPE_MATRIX
    };

    virtual ~ITypeInfoHandler() = default;

    virtual ITypeInfoHandler* Clone() const = 0;
    virtual Type GetHandlerType() const = 0;

    virtual bool CreateInstance(HypData& outInstance) const = 0;
};

class ITypeInfoArrayHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoArrayHandler() = default;

    virtual ITypeInfoHandler* Clone() const override = 0;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_ARRAY;
    }

    virtual bool CreateInstance(HypData& outInstance) const override = 0;

    virtual bool GetElementAt(const HypData& instance, SizeType index, HypData& outValue) const = 0;
    virtual bool SetElementAt(const HypData& instance, SizeType index, HypData&& value) const = 0;

    virtual SizeType GetSize(const HypData& instance) const = 0;

    virtual void Resize(const HypData& instance, SizeType newSize) const = 0;
};

class ITypeInfoLinkedListHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoLinkedListHandler() = default;

    virtual ITypeInfoHandler* Clone() const override = 0;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_LINKEDLIST;
    }

    virtual bool CreateInstance(HypData& outInstance) const override = 0;

    virtual AnyRef GetElementAt(const HypData& instance, SizeType index) const = 0;
    virtual void SetElementAt(const HypData& instance, SizeType index, const HypData& value) const = 0;

    virtual SizeType GetSize(const HypData& instance) const = 0;

    virtual void Resize(const HypData& instance, SizeType newSize) const = 0;
};

class ITypeInfoMapHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoMapHandler() = default;

    virtual ITypeInfoHandler* Clone() const override = 0;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_MAP;
    }

    virtual bool CreateInstance(HypData& outInstance) const override = 0;

    virtual AnyRef GetValueAt(const HypData& instance, const HypData& key) const = 0;
    virtual void SetValueAt(const HypData& instance, const HypData& key, const HypData& value) const = 0;

    virtual bool ContainsKey(const HypData& instance, const HypData& key) const = 0;
    virtual bool RemoveKey(const HypData& instance, const HypData& key) const = 0;

    virtual SizeType GetSize(const HypData& instance) const = 0;
};

class ITypeInfoStringHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoStringHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_STRING;
    }

    virtual bool CreateInstance(HypData& outInstance) const override = 0;

    virtual ITypeInfoHandler* Clone() const override = 0;

    virtual String GetValue(const HypData& instance) const = 0;
    virtual void SetValue(const HypData& instance, const UTF8StringView& str) const = 0;
};

class ITypeInfoVectorHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoVectorHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_VECTOR;
    }

    virtual bool CreateInstance(HypData& outInstance) const override = 0;

    virtual ITypeInfoHandler* Clone() const override = 0;

    virtual int GetNumComponents() const = 0;

    virtual AnyRef GetComponent(const HypData& instance, int index) const = 0;
    virtual void SetComponent(const HypData& instance, int index, const HypData& value) const = 0;
};

class ITypeInfoMatrixHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoMatrixHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_MATRIX;
    }

    virtual bool CreateInstance(HypData& outInstance) const override = 0;

    virtual ITypeInfoHandler* Clone() const override = 0;

    virtual int GetNumRows() const = 0;
    virtual int GetNumColumns() const = 0;

    virtual AnyRef GetElement(const HypData& instance, int row, int column) const = 0;
    virtual void SetElement(const HypData& instance, int row, int column, const HypData& value) const = 0;
};

/*! \brief Additional type information for containers and complex types */
struct HYP_API TypeInfoEx
{
    enum DataType
    {
        DT_NONE = 0,
        DT_TYPE_INFO = 1
    };

    /*! \brief Tagged union holding either:
     *  - const TypeInfo* for container element types (single type)
     *  - const HypClass* for types with HypClass reflection info */
    union
    {
        const TypeInfo* typeInfo;
    } data;

    DataType dataType : 2;

    ITypeInfoHandler* handler = nullptr;
    TypeInfoEx* next = nullptr;

    TypeInfoEx()
        : dataType(DT_NONE)
    {
        data.typeInfo = nullptr;
    }

    TypeInfoEx(const TypeInfoEx& other);
    TypeInfoEx& operator=(const TypeInfoEx& other);

    TypeInfoEx(TypeInfoEx&& other) noexcept;
    TypeInfoEx& operator=(TypeInfoEx&& other) noexcept;

    ~TypeInfoEx();

    /*! \brief Get element type pointer if this holds a TypeInfo* */
    HYP_FORCE_INLINE const TypeInfo* GetElementType() const
    {
        if (dataType == DT_TYPE_INFO)
        {
            return data.typeInfo;
        }

        return nullptr;
    }

    HashCode GetHashCode() const;
};

template <class T, class HypDataType = HypData, class EnableIf = void>
struct TypeInfoImpl;

template <class T, class HypDataType>
struct TypeInfoImpl<T, HypDataType, std::enable_if_t<std::is_same_v<T, GenericArrayWrapper>>>
{
    void operator()(TypeInfo& result) const;
};

template <class ArrayType, class HypDataType>
struct TypeInfoImpl<ArrayType, HypDataType, std::enable_if_t<IsArray<ArrayType>::value>>
{
    void operator()(TypeInfo& result) const;
};

template <class T, SizeType Size, class HypDataType>
struct TypeInfoImpl<containers::FixedArray<T, Size>, HypDataType>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class HypDataType>
struct TypeInfoImpl<containers::LinkedList<T>, HypDataType>
{
    void operator()(TypeInfo& result) const;
};

template <int TStringType, class HypDataType>
struct TypeInfoImpl<containers::String<TStringType>, HypDataType>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class HypDataType>
struct TypeInfoImpl<T, HypDataType, std::enable_if_t<std::is_same_v<T, filesystem::FilePath>>>
{
    void operator()(TypeInfo& result) const;
};

template <class Key, class Value, class NodeAllocatorType, class HypDataType>
struct TypeInfoImpl<containers::HashMap<Key, Value, NodeAllocatorType>, HypDataType>
{
    void operator()(TypeInfo& result) const;
};

template <class Value, auto KeyBy, class NodeAllocatorType, class HypDataType>
struct TypeInfoImpl<containers::HashSet<Value, KeyBy, NodeAllocatorType>, HypDataType>
{
    void operator()(TypeInfo& result) const;
};

template <class Key, class Value, class HypDataType>
struct TypeInfoImpl<containers::FlatMap<Key, Value>, HypDataType>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class HypDataType>
struct TypeInfoImpl<containers::FlatSet<T>, HypDataType>
{
    void operator()(TypeInfo& result) const;
};

template <class Key, class Value, class HypDataType>
struct TypeInfoImpl<containers::ArrayMap<Key, Value>, HypDataType>
{
    void operator()(TypeInfo& result) const;
};

template <class... Types, class HypDataType>
struct TypeInfoImpl<utilities::Variant<Types...>, HypDataType>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class HypDataType>
struct TypeInfoImpl<math::Vec2<T>, HypDataType>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class HypDataType>
struct TypeInfoImpl<math::Vec3<T>, HypDataType>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class HypDataType>
struct TypeInfoImpl<math::Vec4<T>, HypDataType>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class HypDataType>
struct TypeInfoImpl<T, HypDataType, std::enable_if_t<std::is_same_v<T, Mat3f>>>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class HypDataType>
struct TypeInfoImpl<T, HypDataType, std::enable_if_t<std::is_same_v<T, Mat4f>>>
{
    void operator()(TypeInfo& result) const;
};

struct TypeInfo
{
    /*! \brief Get a default TypeInfo instance representing void type */
    HYP_API static const TypeInfo& Void();

    TypeId id = TypeId::Void();
    Name name = Name::Invalid();
    uint16 size = 0;
    uint16 alignment = 0;
    EnumFlags<TypeInfoFlags> flags = TypeInfoFlags::NONE;
    TypeInfoEx extendedInfo;

    TypeInfo() = default;

    TypeInfo(const TypeInfo& other) = default;
    TypeInfo& operator=(const TypeInfo& other) = default;

    TypeInfo(TypeInfo&& other) noexcept = default;
    TypeInfo& operator=(TypeInfo&& other) noexcept = default;

    ~TypeInfo() = default;

    HYP_FORCE_INLINE constexpr operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE constexpr bool operator!() const
    {
        return !IsValid();
    }

    HYP_FORCE_INLINE bool operator==(const TypeInfo& other) const
    {
        return id == other.id
            && name == other.name
            && size == other.size
            && alignment == other.alignment
            && flags == other.flags
            && extendedInfo.GetHashCode() == other.extendedInfo.GetHashCode();
    }

    HYP_FORCE_INLINE bool operator!=(const TypeInfo& other) const
    {
        return !(*this == other);
    }

    HYP_FORCE_INLINE constexpr bool IsValid() const
    {
        return id != TypeId::Void();
    }

    HYP_API static const TypeInfo& ForHypClass(const HypClass* hypClass);

    template <class T>
    static const TypeInfo& ForType()
    {
        using NormalizedT = NormalizedType<T>;

        const TypeId typeId = TypeId::ForType<NormalizedT>();

        if constexpr (std::is_void_v<NormalizedT>)
        {
            return Void();
        }
        else
        {
            static_assert(sizeof(NormalizedT) <= UINT16_MAX, "Cannot use TypeInfo::ForType<T>() with types larger than 65535 bytes");
            static_assert(alignof(NormalizedT) <= UINT16_MAX, "Cannot use TypeInfo::ForType<T>() with types with alignment larger than 65535 bytes");

            if (TypeInfo* cached = TypeInfo_FetchFromCache(typeId, uint16(sizeof(NormalizedT)), uint16(alignof(NormalizedT))))
            {
                return *cached;
            }

            TypeInfo result;
            result.id = typeId;
            result.name = CreateNameFromStaticString(HashedName<TypeNameHelper<NormalizedT, true>::value>());
            result.size = uint16(sizeof(NormalizedT));
            result.alignment = uint16(alignof(NormalizedT));
            result.flags = TypeInfoFlags::NONE;

            if constexpr (std::is_class_v<NormalizedT>)
            {
                result.flags |= TypeInfoFlags::CLASS_TYPE;
            }

            if constexpr (IsPodTypeV<NormalizedT>)
            {
                result.flags |= TypeInfoFlags::POD_TYPE;
            }

            if constexpr (std::is_enum_v<NormalizedT>)
            {
                result.flags |= TypeInfoFlags::ENUM_TYPE;

                result.extendedInfo.data.typeInfo = &ForType<typename std::underlying_type_t<NormalizedT>>();

                result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
                result.extendedInfo.handler = nullptr;
            }

            if constexpr (IsEnumFlagsV<NormalizedT>)
            {
                result.flags |= TypeInfoFlags::ENUM_FLAGS_TYPE;

                result.extendedInfo.data.typeInfo = &ForType<typename NormalizedT::EnumType>();

                result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
                result.extendedInfo.handler = nullptr;
            }

            if constexpr (std::is_fundamental_v<NormalizedT>)
            {
                result.flags |= TypeInfoFlags::FUNDAMENTAL_TYPE;
            }

            if constexpr (std::is_integral_v<NormalizedT>)
            {
                result.flags |= TypeInfoFlags::INTEGRAL_TYPE;
            }

            if constexpr (std::is_floating_point_v<NormalizedT> || std::is_same_v<NormalizedT, Float16>)
            {
                result.flags |= TypeInfoFlags::FLOAT_TYPE;
            }

            if constexpr (ImplementationExistsV<TypeInfoImpl<NormalizedT, HypData>>)
            {
                TypeInfoImpl<NormalizedT, HypData>()(result);
            }

            ValueStorage<Mutex::Guard> guardStorage;

            TypeInfo* pTypeInfo = TypeInfo_Alloc(
                typeId,
                uint16(sizeof(NormalizedT)),
                uint16(alignof(NormalizedT)),
                guardStorage.GetPointer());

            HYP_CORE_ASSERT(pTypeInfo != nullptr);

            new (pTypeInfo) TypeInfo(std::move(result));

            // sanity check
            HYP_CORE_ASSERT(pTypeInfo->id == typeId);

            DebugLog(LogType::Debug, "Registered type info: %s : %u\n", pTypeInfo->name.LookupString(), pTypeInfo->id.Value());

            guardStorage.GetPointer()->~Guard();

            return *pTypeInfo;
        }
    }

    HYP_FORCE_INLINE constexpr bool IsPod() const
    {
        return flags & TypeInfoFlags::POD_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsClass() const
    {
        return flags & TypeInfoFlags::CLASS_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsStruct() const
    {
        return flags & TypeInfoFlags::STRUCT_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsEnum() const
    {
        return flags & TypeInfoFlags::ENUM_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsEnumFlags() const
    {
        return flags & TypeInfoFlags::ENUM_FLAGS_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsFundamental() const
    {
        return flags & TypeInfoFlags::FUNDAMENTAL_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsIntegralType() const
    {
        return flags & TypeInfoFlags::INTEGRAL_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsFloatType() const
    {
        return flags & TypeInfoFlags::FLOAT_TYPE;
    }

    HYP_FORCE_INLINE const HypClass* GetHypClass() const
    {
        return GetClass(id);
    }

    HYP_FORCE_INLINE bool IsArrayType() const
    {
        return flags & TypeInfoFlags::ARRAY_TYPE;
    }

    HYP_FORCE_INLINE bool IsLinkedListType() const
    {
        return flags & TypeInfoFlags::LINKEDLIST_TYPE;
    }

    HYP_FORCE_INLINE bool IsStringType() const
    {
        return flags & TypeInfoFlags::STRING_TYPE;
    }

    HYP_FORCE_INLINE bool IsMapType() const
    {
        return flags & TypeInfoFlags::MAP_TYPE;
    }

    HYP_FORCE_INLINE bool IsSetType() const
    {
        return flags & TypeInfoFlags::SET_TYPE;
    }

    HYP_FORCE_INLINE bool IsVariantType() const
    {
        return flags & TypeInfoFlags::VARIANT_TYPE;
    }

    HYP_FORCE_INLINE bool IsBoolType() const
    {
        return id == TypeId::ForType<bool>();
    }

    HYP_FORCE_INLINE bool IsVec2Type() const
    {
        return flags & TypeInfoFlags::VEC2_TYPE;
    }

    HYP_FORCE_INLINE bool IsVec3Type() const
    {
        return flags & TypeInfoFlags::VEC3_TYPE;
    }

    HYP_FORCE_INLINE bool IsVec4Type() const
    {
        return flags & TypeInfoFlags::VEC4_TYPE;
    }

    HYP_FORCE_INLINE bool IsVectorType() const
    {
        return flags & TypeInfoFlags::VECTOR_TYPE;
    }

    HYP_FORCE_INLINE bool IsMatrixType() const
    {
        return flags & TypeInfoFlags::MATRIX_TYPE;
    }

    HYP_FORCE_INLINE bool IsContainerType() const
    {
        constexpr EnumFlags<TypeInfoFlags> Mask = TypeInfoFlags::ARRAY_TYPE
            | TypeInfoFlags::STRING_TYPE
            | TypeInfoFlags::LINKEDLIST_TYPE
            | TypeInfoFlags::MAP_TYPE
            | TypeInfoFlags::SET_TYPE;

        return flags & Mask;
    }

    /*! \brief Get alternative type at index for Variant<Types...>
     *  \param index The index of the alternative type (0-based)
     *  \return Pointer to TypeInfo for the alternative type, or nullptr if invalid */
    HYP_FORCE_INLINE const TypeInfo* GetVariantAlternativeType(uint32 index) const
    {
        if (!IsVariantType())
        {
            return nullptr;
        }

        TypeInfoEx* current = extendedInfo.next;
        for (uint32 i = 0; i < index && current; ++i)
        {
            current = current->next;
        }

        return current ? current->GetElementType() : nullptr;
    }

    /*! \brief Get element type for Array, String, HashSet, FlatSet, or key type for HashMap/FlatMap */
    HYP_FORCE_INLINE const TypeInfo* GetElementType() const
    {
        return extendedInfo.GetElementType();
    }

    HYP_FORCE_INLINE const TypeInfo* GetEnumType() const
    {
        if (IsEnum())
        {
            return this;
        }
        else if (IsEnumFlags())
        {
            const TypeInfo* enumType = GetElementType();

            if (enumType && enumType->IsEnum())
            {
                return enumType;
            }
        }

        return nullptr;
    }

    /*! \brief Get underlying type for Enum or EnumFlags */
    HYP_FORCE_INLINE const TypeInfo* GetUnderlyingType() const
    {
        if (IsEnum())
        {
            return GetElementType();
        }
        else if (IsEnumFlags())
        {
            const TypeInfo* enumType = GetElementType();

            if (enumType && enumType->IsEnum())
            {
                return enumType->GetElementType();
            }
        }

        return nullptr;
    }

    /*! \brief Get value type for HashMap/FlatMap (stored in extendedInfo.next->data) */
    HYP_FORCE_INLINE const TypeInfo* GetValueType() const
    {
        if (!extendedInfo.next)
        {
            return nullptr;
        }

        return extendedInfo.next->GetElementType();
    }

    /*! \brief Get key type for HashMap/FlatMap (same as GetElementType for these types) */
    HYP_FORCE_INLINE const TypeInfo* GetKeyType() const
    {
        if (IsMapType())
        {
            return GetElementType();
        }

        return nullptr;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(id.GetHashCode());
        hc.Add(name.GetHashCode());
        hc.Add(size);
        hc.Add(alignment);
        hc.Add(flags);
        hc.Add(extendedInfo.GetHashCode());

        return hc;
    }
};

/// Impls for specific types

template <class T, class HypDataType>
void TypeInfoImpl<T, HypDataType, std::enable_if_t<std::is_same_v<T, GenericArrayWrapper>>>::operator()(TypeInfo& result) const
{
    // GenericArrayWrapper is a special case since it can hold any array type
    class GenericArrayHandler final : public ITypeInfoArrayHandler
    {
    public:
        virtual ITypeInfoHandler* Clone() const override
        {
            return new GenericArrayHandler();
        }

        virtual bool CreateInstance(HypDataType& outInstance) const override
        {
            outInstance = HypDataType(T {});

            return true;
        }

        virtual bool GetElementAt(const HypDataType& instance, SizeType index, HypDataType& outValue) const override
        {
            T& array = instance.template Get<T>();

            if (index >= array.Size())
            {
                return false;
            }

            return array.GetElementAt(index, outValue);
        }

        virtual bool SetElementAt(const HypDataType& instance, SizeType index, HypDataType&& value) const override
        {
            T& array = instance.template Get<T>();
            if (index >= array.Size())
            {
                return false;
            }

            return array.SetElementAt(index, std::move(value));
        }

        virtual SizeType GetSize(const HypDataType& instance) const override
        {
            T& array = instance.template Get<T>();
            return array.Size();
        }

        virtual void Resize(const HypDataType& instance, SizeType newSize) const override
        {
            T& array = instance.template Get<T>();

            if (!array.Resize(newSize))
            {
                HYP_CORE_ASSERT(false, "Failed to resize GenericArrayWrapper");
            }
        }
    };

    result.flags |= TypeInfoFlags::ARRAY_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<HypData>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    result.extendedInfo.handler = new GenericArrayHandler();
}

template <class ArrayType, class HypDataType>
void TypeInfoImpl<ArrayType, HypDataType, std::enable_if_t<IsArray<ArrayType>::value>>::operator()(TypeInfo& result) const
{
    class ArrayHandler final : public ITypeInfoArrayHandler
    {
    public:
        virtual ITypeInfoHandler* Clone() const override
        {
            return new ArrayHandler();
        }

        virtual bool CreateInstance(HypDataType& outInstance) const override
        {
            outInstance = HypDataType(ArrayType {});
            return true;
        }

        virtual bool GetElementAt(const HypDataType& instance, SizeType index, HypDataType& outValue) const override
        {
            ArrayType& array = instance.template Get<ArrayType>();

            if (index >= array.Size())
            {
                return false;
            }

            outValue = HypDataType(AnyRef(&array[index]));

            return true;
        }

        virtual bool SetElementAt(const HypDataType& instance, SizeType index, HypDataType&& value) const override
        {
            ArrayType& array = instance.template Get<ArrayType>();

            if (index >= array.Size())
            {
                return false;
            }

            if constexpr (std::is_same_v<typename ArrayType::ValueType, HypDataType>)
            {
                array[index] = std::move(value);
            }
            else
            {
                array[index] = value.template Get<typename ArrayType::ValueType>();
            }

            return true;
        }

        virtual SizeType GetSize(const HypDataType& instance) const override
        {
            ArrayType& array = instance.template Get<ArrayType>();
            return array.Size();
        }

        virtual void Resize(const HypDataType& instance, SizeType newSize) const override
        {
            ArrayType& array = instance.template Get<ArrayType>();
            array.Resize(newSize);
        }
    };

    result.flags |= TypeInfoFlags::ARRAY_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<typename ArrayType::ValueType>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
    result.extendedInfo.handler = new ArrayHandler();
}

template <class T, SizeType Size, class HypDataType>
void TypeInfoImpl<containers::FixedArray<T, Size>, HypDataType>::operator()(TypeInfo& result) const
{
    using FixedArrayType = FixedArray<T, Size>;

    class FixedArrayHandler final : public ITypeInfoArrayHandler
    {
    public:
        virtual ITypeInfoHandler* Clone() const override
        {
            return new FixedArrayHandler();
        }

        virtual bool CreateInstance(HypDataType& outInstance) const override
        {
            outInstance = HypDataType(FixedArrayType {});
            return true;
        }

        virtual bool GetElementAt(const HypDataType& instance, SizeType index, HypDataType& outValue) const override
        {
            FixedArrayType& array = instance.template Get<FixedArrayType>();

            if (index >= array.Size())
            {
                return false;
            }

            outValue = HypData(AnyRef(&array[index]));

            return true;
        }

        virtual bool SetElementAt(const HypDataType& instance, SizeType index, HypDataType&& value) const override
        {
            FixedArrayType& array = instance.template Get<FixedArrayType>();

            if (index >= array.Size())
            {
                return false;
            }

            if constexpr (std::is_same_v<T, HypDataType>)
            {
                array[index] = std::move(value);
            }
            else
            {
                array[index] = value.template Get<T>();
            }

            return true;
        }

        virtual SizeType GetSize(const HypDataType& instance) const override
        {
            FixedArrayType& array = instance.template Get<FixedArrayType>();
            return array.Size();
        }

        virtual void Resize(const HypDataType& instance, SizeType newSize) const override
        {
            // FixedArray has a fixed size, so resizing is not supported
            // This operation is a no-op
        }
    };

    result.flags |= TypeInfoFlags::ARRAY_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<T>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
    result.extendedInfo.handler = new FixedArrayHandler();
}

template <class T, class HypDataType>
void TypeInfoImpl<containers::LinkedList<T>, HypDataType>::operator()(TypeInfo& result) const
{
    using ListType = containers::LinkedList<T>;

    class LinkedListHandler final : public ITypeInfoLinkedListHandler
    {
    public:
        virtual ITypeInfoHandler* Clone() const override
        {
            return new LinkedListHandler();
        }

        virtual bool CreateInstance(HypDataType& outInstance) const override
        {
            outInstance = HypDataType(ListType {});
            return true;
        }

        virtual AnyRef GetElementAt(const HypDataType& instance, SizeType index) const override
        {
            ListType& list = instance.template Get<ListType>();
            auto it = list.Begin() + index;
            return AnyRef(&(*it));
        }

        virtual void SetElementAt(const HypDataType& instance, SizeType index, const HypDataType& value) const override
        {
            ListType& list = instance.template Get<ListType>();
            auto it = list.Begin() + index;
            *it = value.template Get<T>();
        }

        virtual SizeType GetSize(const HypDataType& instance) const override
        {
            ListType& list = instance.template Get<ListType>();
            return list.Size();
        }

        virtual void Resize(const HypDataType& instance, SizeType newSize) const override
        {
            ListType& list = instance.template Get<ListType>();

            while (list.Size() < newSize)
            {
                list.PushBack(T {});
            }

            while (list.Size() > newSize)
            {
                list.PopBack();
            }
        }
    };

    result.flags |= TypeInfoFlags::LINKEDLIST_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<T>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    result.extendedInfo.handler = new LinkedListHandler();
}

template <int TStringType, class HypDataType>
void TypeInfoImpl<containers::String<TStringType>, HypDataType>::operator()(TypeInfo& result) const
{
    using StringType = containers::String<TStringType>;

    class StringHandler final : public ITypeInfoStringHandler
    {
    public:
        virtual ITypeInfoHandler* Clone() const override
        {
            return new StringHandler();
        }

        virtual bool CreateInstance(HypDataType& outInstance) const override
        {
            outInstance = HypDataType(StringType {});
            return true;
        }

        virtual String GetValue(const HypDataType& instance) const override
        {
            StringType& string = instance.template Get<StringType>();
            return string.ToUTF8();
        }

        virtual void SetValue(const HypDataType& instance, const UTF8StringView& str) const override
        {

            StringType& string = instance.template Get<StringType>();
            string = str;
        }
    };

    result.flags |= TypeInfoFlags::STRING_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<typename StringType::CharType>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
    result.extendedInfo.handler = new StringHandler();
}

template <class T, class HypDataType>
void TypeInfoImpl<T, HypDataType, std::enable_if_t<std::is_same_v<T, filesystem::FilePath>>>::operator()(TypeInfo& result) const
{
    // delegate to String impl since FilePath is just a wrapper around String
    TypeInfoImpl<typename T::Base, HypDataType>()(result);
}

template <class Key, class Value, class NodeAllocatorType, class HypDataType>
void TypeInfoImpl<containers::HashMap<Key, Value, NodeAllocatorType>, HypDataType>::operator()(TypeInfo& result) const
{
    using MapType = containers::HashMap<Key, Value, NodeAllocatorType>;

    class HashMapHandler final : public ITypeInfoMapHandler
    {
    public:
        virtual ITypeInfoHandler* Clone() const override
        {
            return new HashMapHandler();
        }

        virtual bool CreateInstance(HypDataType& outInstance) const override
        {
            outInstance = HypData(MapType {});
            return true;
        }

        virtual AnyRef GetValueAt(const HypDataType& instance, const HypDataType& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            Key& k = key.template Get<Key>();

            auto it = map.Find(k);
            if (it != map.End())
            {
                return AnyRef(&it->second);
            }

            return AnyRef();
        }

        virtual void SetValueAt(const HypDataType& instance, const HypDataType& key, const HypDataType& value) const override
        {
            MapType& map = instance.template Get<MapType>();
            Key& k = key.template Get<Key>();
            Value& v = value.template Get<Value>();

            map[k] = v;
        }

        virtual bool ContainsKey(const HypDataType& instance, const HypDataType& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            Key& k = key.template Get<Key>();

            return map.Find(k) != map.End();
        }

        virtual bool RemoveKey(const HypDataType& instance, const HypDataType& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            Key& k = key.template Get<Key>();

            auto it = map.Find(k);
            if (it != map.End())
            {
                map.Erase(it);
                return true;
            }

            return false;
        }

        virtual SizeType GetSize(const HypDataType& instance) const override
        {
            MapType& map = instance.template Get<MapType>();

            return map.Size();
        }
    };

    result.flags |= TypeInfoFlags::MAP_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<Key>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    result.extendedInfo.next = new TypeInfoEx();
    result.extendedInfo.next->data.typeInfo = &TypeInfo::ForType<Value>();
    result.extendedInfo.next->dataType = TypeInfoEx::DT_TYPE_INFO;

    result.extendedInfo.handler = new HashMapHandler();
}

template <class Key, class Value, class HypDataType>
void TypeInfoImpl<containers::FlatMap<Key, Value>, HypDataType>::operator()(TypeInfo& result) const
{
    using MapType = containers::FlatMap<Key, Value>;

    class FlatMapHandler final : public ITypeInfoMapHandler
    {
    public:
        virtual ITypeInfoHandler* Clone() const override
        {
            return new FlatMapHandler();
        }

        virtual bool CreateInstance(HypDataType& outInstance) const override
        {
            outInstance = HypData(MapType {});
            return true;
        }

        virtual AnyRef GetValueAt(const HypDataType& instance, const HypDataType& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            Key& k = key.template Get<Key>();

            auto it = map.Find(k);
            if (it != map.End())
            {
                return AnyRef(&it->second);
            }

            return AnyRef();
        }

        virtual void SetValueAt(const HypDataType& instance, const HypDataType& key, const HypDataType& value) const override
        {
            MapType& map = instance.template Get<MapType>();
            Key& k = key.template Get<Key>();
            Value& v = value.template Get<Value>();

            map[k] = v;
        }

        virtual bool ContainsKey(const HypDataType& instance, const HypDataType& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            Key& k = key.template Get<Key>();

            return map.Find(k) != map.End();
        }

        virtual bool RemoveKey(const HypDataType& instance, const HypDataType& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            Key& k = key.template Get<Key>();

            auto it = map.Find(k);
            if (it != map.End())
            {
                map.Erase(it);
                return true;
            }

            return false;
        }

        virtual SizeType GetSize(const HypDataType& instance) const override
        {

            MapType& map = instance.template Get<MapType>();

            return map.Size();
        }
    };

    result.flags |= TypeInfoFlags::MAP_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<Key>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    result.extendedInfo.next = new TypeInfoEx();
    result.extendedInfo.next->data.typeInfo = &TypeInfo::ForType<Value>();
    result.extendedInfo.next->dataType = TypeInfoEx::DT_TYPE_INFO;

    result.extendedInfo.handler = new FlatMapHandler();
}

template <class Key, class Value, class HypDataType>
void TypeInfoImpl<containers::ArrayMap<Key, Value>, HypDataType>::operator()(TypeInfo& result) const
{
    using MapType = containers::ArrayMap<Key, Value>;

    class ArrayMapHandler final : public ITypeInfoMapHandler
    {
    public:
        virtual ITypeInfoHandler* Clone() const override
        {
            return new ArrayMapHandler();
        }

        virtual bool CreateInstance(HypDataType& outInstance) const override
        {
            outInstance = HypData(MapType {});
            return true;
        }

        virtual AnyRef GetValueAt(const HypDataType& instance, const HypDataType& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            Key& k = key.template Get<Key>();

            auto it = map.Find(k);
            if (it != map.End())
            {
                return AnyRef(&it->second);
            }

            return AnyRef();
        }

        virtual void SetValueAt(const HypDataType& instance, const HypDataType& key, const HypDataType& value) const override
        {
            MapType& map = instance.template Get<MapType>();
            Key& k = key.template Get<Key>();
            Value& v = value.template Get<Value>();

            map[k] = v;
        }

        virtual bool ContainsKey(const HypDataType& instance, const HypDataType& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            Key& k = key.template Get<Key>();

            return map.Find(k) != map.End();
        }

        virtual bool RemoveKey(const HypDataType& instance, const HypDataType& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            Key& k = key.template Get<Key>();

            auto it = map.Find(k);
            if (it != map.End())
            {
                map.Erase(it);
                return true;
            }

            return false;
        }

        virtual SizeType GetSize(const HypDataType& instance) const override
        {
            MapType& map = instance.template Get<MapType>();

            return map.Size();
        }
    };

    result.flags |= TypeInfoFlags::MAP_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<Key>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    result.extendedInfo.next = new TypeInfoEx();
    result.extendedInfo.next->data.typeInfo = &TypeInfo::ForType<Value>();
    result.extendedInfo.next->dataType = TypeInfoEx::DT_TYPE_INFO;

    result.extendedInfo.handler = new ArrayMapHandler();
}

template <class Value, class HypDataType>
void TypeInfoImpl<containers::FlatSet<Value>, HypDataType>::operator()(TypeInfo& result) const
{
    result.flags |= TypeInfoFlags::SET_TYPE;
    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<Value>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
}

template <class Value, auto KeyBy, class NodeAllocatorType, class HypDataType>
void TypeInfoImpl<containers::HashSet<Value, KeyBy, NodeAllocatorType>, HypDataType>::operator()(TypeInfo& result) const
{
    result.flags |= TypeInfoFlags::SET_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<Value>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
}

template <class... Types, class HypDataType>
void TypeInfoImpl<utilities::Variant<Types...>, HypDataType>::operator()(TypeInfo& result) const
{
    result.flags |= TypeInfoFlags::VARIANT_TYPE;

    constexpr SizeType variantSize = sizeof...(Types);

    if constexpr (variantSize > 0)
    {
        auto altArray = BuildVariantTypeArray<utilities::Variant<Types...>>(std::make_index_sequence<SizeType(variantSize)> {});

        // Convert the fixed array into a linked list of TypeInfoEx nodes
        TypeInfoEx* head = nullptr;
        TypeInfoEx* current = nullptr;

        for (SizeType i = 0; i < altArray.Size(); ++i)
        {
            TypeInfoEx* node = new TypeInfoEx();
            node->data.typeInfo = altArray[i];
            node->dataType = TypeInfoEx::DT_TYPE_INFO;

            if (!head)
            {
                head = node;
                current = node;
            }
            else
            {
                current->next = node;
                current = node;
            }
        }

        result.extendedInfo.next = head;
    }
}

template <class T, class HypDataType>
void TypeInfoImpl<math::Vec2<T>, HypDataType>::operator()(TypeInfo& result) const
{
    using Vec2Type = math::Vec2<T>;

    result.flags |= TypeInfoFlags::VEC2_TYPE;
    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<T>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    // set handler
    class Vec2Handler final : public ITypeInfoVectorHandler
    {
    public:
        virtual ITypeInfoHandler* Clone() const override
        {
            return new Vec2Handler();
        }

        virtual bool CreateInstance(HypDataType& outInstance) const override
        {
            outInstance = HypData(Vec2Type {});
            return true;
        }

        virtual int GetNumComponents() const override
        {
            return 2;
        }

        virtual AnyRef GetComponent(const HypDataType& instance, int index) const override
        {
            Vec2Type& vec = instance.template Get<Vec2Type>();
            switch (index)
            {
            case 0:
                return AnyRef(&vec.x);
            case 1:
                return AnyRef(&vec.y);
            default:
                return AnyRef();
            }
        }

        virtual void SetComponent(const HypDataType& instance, int index, const HypDataType& value) const override
        {
            Vec2Type& vec = instance.template Get<Vec2Type>();
            T v = value.template Get<T>();

            switch (index)
            {
            case 0:
                vec.x = v;
                break;
            case 1:
                vec.y = v;
                break;
            default:
                break;
            }
        }
    };

    result.extendedInfo.handler = new Vec2Handler();
}

template <class T, class HypDataType>
void TypeInfoImpl<math::Vec3<T>, HypDataType>::operator()(TypeInfo& result) const
{
    using Vec3Type = math::Vec3<T>;

    result.flags |= TypeInfoFlags::VEC3_TYPE;
    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<T>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    // set handler
    class Vec3Handler final : public ITypeInfoVectorHandler
    {
    public:
        virtual ITypeInfoHandler* Clone() const override
        {
            return new Vec3Handler();
        }

        virtual bool CreateInstance(HypDataType& outInstance) const override
        {
            outInstance = HypData(Vec3Type {});
            return true;
        }

        virtual int GetNumComponents() const override
        {
            return 3;
        }

        virtual AnyRef GetComponent(const HypDataType& instance, int index) const override
        {
            Vec3Type& vec = instance.template Get<Vec3Type>();
            switch (index)
            {
            case 0:
                return AnyRef(&vec.x);
            case 1:
                return AnyRef(&vec.y);
            case 2:
                return AnyRef(&vec.z);
            default:
                return AnyRef();
            }
        }

        virtual void SetComponent(const HypDataType& instance, int index, const HypDataType& value) const override
        {
            Vec3Type& vec = instance.template Get<Vec3Type>();
            T v = value.template Get<T>();

            switch (index)
            {
            case 0:
                vec.x = v;
                break;
            case 1:
                vec.y = v;
                break;
            case 2:
                vec.z = v;
                break;
            default:
                break;
            }
        }
    };

    result.extendedInfo.handler = new Vec3Handler();
}

template <class T, class HypDataType>
void TypeInfoImpl<math::Vec4<T>, HypDataType>::operator()(TypeInfo& result) const
{
    using Vec4Type = math::Vec4<T>;

    result.flags |= TypeInfoFlags::VEC4_TYPE;
    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<T>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    // set handler
    class Vec4Handler final : public ITypeInfoVectorHandler
    {
    public:
        virtual ITypeInfoHandler* Clone() const override
        {
            return new Vec4Handler();
        }

        virtual bool CreateInstance(HypDataType& outInstance) const override
        {
            outInstance = HypData(Vec4Type {});
            return true;
        }

        virtual int GetNumComponents() const override
        {
            return 4;
        }

        virtual AnyRef GetComponent(const HypDataType& instance, int index) const override
        {
            Vec4Type& vec = instance.template Get<Vec4Type>();
            switch (index)
            {
            case 0:
                return AnyRef(&vec.x);
            case 1:
                return AnyRef(&vec.y);
            case 2:
                return AnyRef(&vec.z);
            case 3:
                return AnyRef(&vec.w);
            default:
                return AnyRef();
            }
        }

        virtual void SetComponent(const HypDataType& instance, int index, const HypDataType& value) const override
        {
            Vec4Type& vec = instance.template Get<Vec4Type>();
            T v = value.template Get<T>();

            switch (index)
            {
            case 0:
                vec.x = v;
                break;
            case 1:
                vec.y = v;
                break;
            case 2:
                vec.z = v;
                break;
            case 3:
                vec.w = v;
                break;
            default:
                break;
            }
        }
    };

    result.extendedInfo.handler = new Vec4Handler();
}

template <class T, class HypDataType>
void TypeInfoImpl<T, HypDataType, std::enable_if_t<std::is_same_v<T, Mat3f>>>::operator()(TypeInfo& result) const
{
    using MatrixType = T;

    result.flags |= TypeInfoFlags::MATRIX_TYPE;
    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<float>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    // set handler
    class Mat3fHandler final : public ITypeInfoMatrixHandler
    {
    public:
        virtual ITypeInfoHandler* Clone() const override
        {
            return new Mat3fHandler();
        }

        virtual bool CreateInstance(HypDataType& outInstance) const override
        {
            outInstance = HypData(MatrixType {});
            return true;
        }

        virtual int GetNumRows() const override
        {
            return 3;
        }

        virtual int GetNumColumns() const override
        {
            return 3;
        }

        virtual AnyRef GetElement(const HypDataType& instance, int row, int column) const override
        {
            MatrixType& mat = instance.template Get<MatrixType>();
            if (row < 0 || row >= 3 || column < 0 || column >= 3)
            {
                return AnyRef();
            }

            return AnyRef(&mat[row][column]);
        }

        virtual void SetElement(const HypDataType& instance, int row, int column, const HypDataType& value) const override
        {
            MatrixType& mat = instance.template Get<MatrixType>();
            if (row < 0 || row >= 3 || column < 0 || column >= 3)
            {
                return;
            }

            mat[row][column] = value.template Get<float>();
        }
    };

    result.extendedInfo.handler = new Mat3fHandler();
}

template <class T, class HypDataType>
void TypeInfoImpl<T, HypDataType, std::enable_if_t<std::is_same_v<T, Mat4f>>>::operator()(TypeInfo& result) const
{
    using MatrixType = T;

    result.flags |= TypeInfoFlags::MATRIX_TYPE;
    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<float>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    // set handler
    class Mat4fHandler final : public ITypeInfoMatrixHandler
    {
    public:
        virtual ITypeInfoHandler* Clone() const override
        {
            return new Mat4fHandler();
        }

        virtual bool CreateInstance(HypDataType& outInstance) const override
        {
            outInstance = HypData(MatrixType {});
            return true;
        }

        virtual int GetNumRows() const override
        {
            return 4;
        }

        virtual int GetNumColumns() const override
        {
            return 4;
        }

        virtual AnyRef GetElement(const HypDataType& instance, int row, int column) const override
        {
            MatrixType& mat = instance.template Get<MatrixType>();
            if (row < 0 || row >= 4 || column < 0 || column >= 4)
            {
                return AnyRef();
            }

            return AnyRef(&mat[row][column]);
        }

        virtual void SetElement(const HypDataType& instance, int row, int column, const HypDataType& value) const override
        {
            MatrixType& mat = instance.template Get<MatrixType>();
            if (row < 0 || row >= 4 || column < 0 || column >= 4)
            {
                return;
            }

            mat[row][column] = value.template Get<float>();
        }
    };

    result.extendedInfo.handler = new Mat4fHandler();
}

/// Wrapper functions for forward decls

inline const TypeInfo& TypeInfo_Void()
{
    return TypeInfo::Void();
}

inline const TypeInfo& TypeInfo_ForHypClass(const HypClass* hypClass)
{
    return TypeInfo::ForHypClass(hypClass);
}

inline const TypeId& TypeInfo_GetId(const TypeInfo& typeInfo)
{
    return typeInfo.id;
}

inline const Name& TypeInfo_GetName(const TypeInfo& typeInfo)
{
    return typeInfo.name;
}

inline SizeType TypeInfo_GetSize(const TypeInfo& typeInfo)
{
    return typeInfo.size;
}

template <class T>
const TypeInfo& TypeInfo_ForType()
{
    return TypeInfo::ForType<T>();
}

// Definition of BuildVariantTypeArray placed after TypeInfo is defined.
template <class NormalizedT, std::size_t... Indices>
inline FixedArray<const TypeInfo*, sizeof...(Indices)> BuildVariantTypeArray(std::index_sequence<Indices...>)
{
    FixedArray<const TypeInfo*, sizeof...(Indices)> res;
    ((res[Indices] = &TypeInfo_ForType<typename IsVariant<NormalizedT>::template TypeAtIndex<Indices>>()), ...);
    return res;
}

} // namespace utilities

using utilities::ITypeInfoHandler;

using utilities::ITypeInfoArrayHandler;
using utilities::ITypeInfoLinkedListHandler;
using utilities::ITypeInfoMapHandler;
using utilities::ITypeInfoMatrixHandler;
using utilities::ITypeInfoStringHandler;
using utilities::ITypeInfoVectorHandler;

using utilities::TypeInfo_ForHypClass;
using utilities::TypeInfo_ForType;
using utilities::TypeInfo_Void;

using utilities::TypeInfo;
using utilities::TypeInfo_Initialize;
using utilities::TypeInfo_Shutdown;

} // namespace hyperion
