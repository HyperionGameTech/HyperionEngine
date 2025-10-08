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

enum class TypeAttributeFlags : uint32
{
    NONE = 0x0,
    POD_TYPE = 0x1,
    CLASS_TYPE = 0x2,
    STRUCT_TYPE = 0x4,
    CLASS_OR_STRUCT_TYPE = CLASS_TYPE | STRUCT_TYPE,
    ENUM_TYPE = 0x8,
    FUNDAMENTAL_TYPE = 0x10,
    INTEGRAL_TYPE = 0x20,
    FLOAT_TYPE = 0x40,

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
    VECTOR_TYPE = VEC2_TYPE | VEC3_TYPE | VEC4_TYPE
};

HYP_MAKE_ENUM_FLAGS(TypeAttributeFlags)

class HypClass;
struct HypData;
struct GenericArrayWrapper;

struct Float16;

HYP_API extern const HypClass* GetClass(TypeId typeId);
HYP_API extern bool HypClassRegistry_IsInitialized();
namespace containers {

template <class T, class AllocatorType>
class Array;

template <int TStringType>
class String;

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

// Type traits

template <class T>
struct IsVec2 : std::false_type
{
};

template <class T>
struct IsVec2<math::Vec2<T>> : std::true_type
{
};

template <class T>
struct IsVec3 : std::false_type
{
};

template <class T>
struct IsVec3<math::Vec3<T>> : std::true_type
{
};

template <class T>
struct IsVec4 : std::false_type
{
};

template <class T>
struct IsVec4<math::Vec4<T>> : std::true_type
{
};

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
        TYPE_VECTOR
    };

    virtual ~ITypeInfoHandler() = default;

    virtual ITypeInfoHandler* Clone() const = 0;
    virtual Type GetHandlerType() const = 0;

    virtual bool CreateInstance(Any& outInstance) const = 0;
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

    virtual bool CreateInstance(Any& outInstance) const override = 0;

    virtual bool GetElementAt(AnyRef arrayRef, SizeType index, HypData& outValue) const = 0;
    virtual bool SetElementAt(AnyRef arrayRef, SizeType index, HypData&& value) const = 0;

    virtual SizeType GetSize(AnyRef arrayRef) const = 0;

    virtual void Resize(AnyRef arrayRef, SizeType newSize) const = 0;
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

    virtual bool CreateInstance(Any& outInstance) const override = 0;

    virtual AnyRef GetElementAt(AnyRef listRef, SizeType index) const = 0;
    virtual void SetElementAt(AnyRef listRef, SizeType index, AnyRef value) const = 0;

    virtual SizeType GetSize(AnyRef listRef) const = 0;

    virtual void Resize(AnyRef listRef, SizeType newSize) const = 0;
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

    virtual bool CreateInstance(Any& outInstance) const override = 0;

    virtual AnyRef GetValueAt(AnyRef mapRef, AnyRef key) const = 0;
    virtual void SetValueAt(AnyRef mapRef, AnyRef key, AnyRef value) const = 0;

    virtual bool ContainsKey(AnyRef mapRef, AnyRef key) const = 0;
    virtual bool RemoveKey(AnyRef mapRef, AnyRef key) const = 0;

    virtual SizeType GetSize(AnyRef mapRef) const = 0;
};

class ITypeInfoStringHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoStringHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_STRING;
    }

    virtual bool CreateInstance(Any& outInstance) const override = 0;

    virtual ITypeInfoHandler* Clone() const override = 0;

    virtual String GetValue(AnyRef stringRef) const = 0;
    virtual void SetValue(AnyRef stringRef, const UTF8StringView& str) const = 0;
};

class ITypeInfoVectorHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoVectorHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_VECTOR;
    }

    virtual bool CreateInstance(Any& outInstance) const override = 0;

    virtual ITypeInfoHandler* Clone() const override = 0;

    virtual int GetNumComponents() const = 0;

    virtual AnyRef GetComponent(AnyRef vectorRef, int index) const = 0;
    virtual void SetComponent(AnyRef vectorRef, int index, AnyRef value) const = 0;
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

template <class T, class AllocatorType, class HypDataType>
struct TypeInfoImpl<Array<T, AllocatorType>, HypDataType>
{
    void operator()(TypeInfo& result) const;
};

template <class T, SizeType Size, class HypDataType>
struct TypeInfoImpl<FixedArray<T, Size>, HypDataType>
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
    EnumFlags<TypeAttributeFlags> flags = TypeAttributeFlags::NONE;
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

    static const TypeInfo& ForHypClass(const HypClass* hypClass);

    template <class T>
    static const TypeInfo& ForType()
    {
        using NormalizedT = NormalizedType<T>;

        constexpr TypeId typeId = TypeId::ForType<NormalizedT>();

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
            result.name = CreateNameFromStaticString(HashedName<TypeNameHelper<T, false>::value>());
            result.size = uint16(sizeof(NormalizedT));
            result.alignment = uint16(alignof(NormalizedT));
            result.flags = TypeAttributeFlags::NONE;

            if constexpr (std::is_class_v<NormalizedT>)
            {
                result.flags |= TypeAttributeFlags::CLASS_TYPE;
            }

            if constexpr (IsPodTypeV<NormalizedT>)
            {
                result.flags |= TypeAttributeFlags::POD_TYPE;
            }

            if constexpr (std::is_enum_v<NormalizedT>)
            {
                result.flags |= TypeAttributeFlags::ENUM_TYPE;
            }

            if constexpr (std::is_fundamental_v<NormalizedT>)
            {
                result.flags |= TypeAttributeFlags::FUNDAMENTAL_TYPE;
            }

            if constexpr (std::is_integral_v<NormalizedT>)
            {
                result.flags |= TypeAttributeFlags::INTEGRAL_TYPE;
            }

            if constexpr (std::is_floating_point_v<NormalizedT> || std::is_same_v<NormalizedT, Float16>)
            {
                result.flags |= TypeAttributeFlags::FLOAT_TYPE;
            }

            if constexpr (ImplementationExistsV<TypeInfoImpl<NormalizedT, HypData>>)
            {
                TypeInfoImpl<NormalizedT, HypData>()(result);
            }
            else if constexpr (IsLinkedList<NormalizedT>::value)
            {
                class LinkedListHandler final : public ITypeInfoLinkedListHandler
                {
                public:
                    virtual ITypeInfoHandler* Clone() const override
                    {
                        return new LinkedListHandler();
                    }

                    virtual bool CreateInstance(Any& outInstance) const override
                    {
                        using ListType = NormalizedT;

                        outInstance = Any(ListType {});
                        return true;
                    }

                    virtual AnyRef GetElementAt(AnyRef arrayRef, SizeType index) const override
                    {
                        using ListType = NormalizedT;
                        using ValueType = typename ListType::ValueType;

                        ListType& list = arrayRef.Get<ListType>();
                        auto it = list.Begin() + index;
                        return AnyRef(&(*it));
                    }

                    virtual void SetElementAt(AnyRef arrayRef, SizeType index, AnyRef value) const override
                    {
                        using ListType = NormalizedT;
                        using ValueType = typename ListType::ValueType;

                        ListType& list = arrayRef.Get<ListType>();
                        auto it = list.Begin() + index;
                        ValueType& v = value.Get<ValueType>();

                        *it = v;
                    }

                    virtual SizeType GetSize(AnyRef arrayRef) const override
                    {
                        using ListType = NormalizedT;

                        ListType& list = arrayRef.Get<ListType>();
                        return list.Size();
                    }

                    virtual void Resize(AnyRef arrayRef, SizeType newSize) const override
                    {
                        using ListType = NormalizedT;

                        ListType& list = arrayRef.Get<ListType>();

                        while (list.Size() < newSize)
                        {
                            list.PushBack(typename ListType::ValueType {});
                        }

                        while (list.Size() > newSize)
                        {
                            list.PopBack();
                        }
                    }
                };

                result.flags |= TypeAttributeFlags::LINKEDLIST_TYPE;

                result.extendedInfo.data.typeInfo = &ForType<typename NormalizedT::ValueType>();
                result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

                result.extendedInfo.handler = new LinkedListHandler();
            }
            else if constexpr (IsString<NormalizedT>::value)
            {
                class StringHandler final : public ITypeInfoStringHandler
                {
                public:
                    virtual ITypeInfoHandler* Clone() const override
                    {
                        return new StringHandler();
                    }

                    virtual bool CreateInstance(Any& outInstance) const override
                    {
                        using StringType = NormalizedT;

                        outInstance = Any(StringType {});
                        return true;
                    }

                    virtual String GetValue(AnyRef stringRef) const override
                    {
                        using StringType = NormalizedT;

                        StringType& string = stringRef.Get<StringType>();
                        return string.ToUTF8();
                    }

                    virtual void SetValue(AnyRef stringRef, const UTF8StringView& str) const override
                    {
                        using StringType = NormalizedT;

                        StringType& string = stringRef.Get<StringType>();
                        string = str;
                    }
                };

                result.flags |= TypeAttributeFlags::STRING_TYPE;

                result.extendedInfo.data.typeInfo = &ForType<typename NormalizedT::ValueType>();
                result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
                result.extendedInfo.handler = new StringHandler();
            }
            else if constexpr (IsHashMap<NormalizedT>::value)
            {
                class HashMapHandler final : public ITypeInfoMapHandler
                {
                public:
                    virtual ITypeInfoHandler* Clone() const override
                    {
                        return new HashMapHandler();
                    }

                    virtual bool CreateInstance(Any& outInstance) const override
                    {
                        using MapType = NormalizedT;

                        outInstance = Any(MapType {});
                        return true;
                    }

                    virtual AnyRef GetValueAt(AnyRef mapRef, AnyRef key) const override
                    {
                        using MapType = NormalizedT;
                        using KeyType = typename MapType::KeyType;
                        using ValueType = typename MapType::ValueType;

                        MapType& map = mapRef.Get<MapType>();
                        KeyType& k = key.Get<KeyType>();

                        auto it = map.Find(k);
                        if (it != map.End())
                        {
                            return AnyRef(&it->second);
                        }

                        return AnyRef();
                    }

                    virtual void SetValueAt(AnyRef mapRef, AnyRef key, AnyRef value) const override
                    {
                        using MapType = NormalizedT;
                        using KeyType = typename MapType::KeyType;
                        using ValueType = typename MapType::ValueType;

                        MapType& map = mapRef.Get<MapType>();
                        KeyType& k = key.Get<KeyType>();
                        ValueType& v = value.Get<ValueType>();

                        map[k] = v;
                    }

                    virtual bool ContainsKey(AnyRef mapRef, AnyRef key) const override
                    {
                        using MapType = NormalizedT;
                        using KeyType = typename MapType::KeyType;

                        MapType& map = mapRef.Get<MapType>();
                        KeyType& k = key.Get<KeyType>();

                        return map.Find(k) != map.End();
                    }

                    virtual bool RemoveKey(AnyRef mapRef, AnyRef key) const override
                    {
                        using MapType = NormalizedT;
                        using KeyType = typename MapType::KeyType;

                        MapType& map = mapRef.Get<MapType>();
                        KeyType& k = key.Get<KeyType>();

                        auto it = map.Find(k);
                        if (it != map.End())
                        {
                            map.Erase(it);
                            return true;
                        }

                        return false;
                    }

                    virtual SizeType GetSize(AnyRef mapRef) const override
                    {
                        using MapType = NormalizedT;

                        MapType& map = mapRef.Get<MapType>();

                        return map.Size();
                    }
                };

                result.flags |= TypeAttributeFlags::MAP_TYPE;

                result.extendedInfo.data.typeInfo = &ForType<typename NormalizedT::KeyType>();
                result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

                result.extendedInfo.next = new TypeInfoEx();
                result.extendedInfo.next->data.typeInfo = &ForType<typename NormalizedT::ValueType>();
                result.extendedInfo.next->dataType = TypeInfoEx::DT_TYPE_INFO;

                result.extendedInfo.handler = new HashMapHandler();
            }
            else if constexpr (IsHashSet<NormalizedT>::value)
            {
                result.flags |= TypeAttributeFlags::SET_TYPE;

                using ValueType = typename NormalizedT::ValueType;

                result.extendedInfo.data.typeInfo = &ForType<ValueType>();
                result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
            }
            else if constexpr (IsFlatMap<NormalizedT>::value)
            {
                class FlatMapHandler final : public ITypeInfoMapHandler
                {
                public:
                    virtual ITypeInfoHandler* Clone() const override
                    {
                        return new FlatMapHandler();
                    }

                    virtual bool CreateInstance(Any& outInstance) const override
                    {
                        using MapType = NormalizedT;

                        outInstance = Any(MapType {});
                        return true;
                    }

                    virtual AnyRef GetValueAt(AnyRef mapRef, AnyRef key) const override
                    {
                        using MapType = NormalizedT;
                        using KeyType = typename MapType::KeyType;
                        using ValueType = typename MapType::ValueType;

                        MapType& map = mapRef.Get<MapType>();
                        KeyType& k = key.Get<KeyType>();

                        auto it = map.Find(k);
                        if (it != map.End())
                        {
                            return AnyRef(&it->second);
                        }

                        return AnyRef();
                    }

                    virtual void SetValueAt(AnyRef mapRef, AnyRef key, AnyRef value) const override
                    {
                        using MapType = NormalizedT;
                        using KeyType = typename MapType::KeyType;
                        using ValueType = typename MapType::ValueType;

                        MapType& map = mapRef.Get<MapType>();
                        KeyType& k = key.Get<KeyType>();
                        ValueType& v = value.Get<ValueType>();

                        map[k] = v;
                    }

                    virtual bool ContainsKey(AnyRef mapRef, AnyRef key) const override
                    {
                        using MapType = NormalizedT;
                        using KeyType = typename MapType::KeyType;

                        MapType& map = mapRef.Get<MapType>();
                        KeyType& k = key.Get<KeyType>();

                        return map.Find(k) != map.End();
                    }

                    virtual bool RemoveKey(AnyRef mapRef, AnyRef key) const override
                    {
                        using MapType = NormalizedT;
                        using KeyType = typename MapType::KeyType;

                        MapType& map = mapRef.Get<MapType>();
                        KeyType& k = key.Get<KeyType>();

                        auto it = map.Find(k);
                        if (it != map.End())
                        {
                            map.Erase(it);
                            return true;
                        }

                        return false;
                    }

                    virtual SizeType GetSize(AnyRef mapRef) const override
                    {
                        using MapType = NormalizedT;

                        MapType& map = mapRef.Get<MapType>();

                        return map.Size();
                    }
                };

                result.flags |= TypeAttributeFlags::MAP_TYPE;

                result.extendedInfo.data.typeInfo = &ForType<typename NormalizedT::KeyType>();
                result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

                result.extendedInfo.next = new TypeInfoEx();
                result.extendedInfo.next->data.typeInfo = &ForType<typename NormalizedT::ValueType>();
                result.extendedInfo.next->dataType = TypeInfoEx::DT_TYPE_INFO;

                result.extendedInfo.handler = new FlatMapHandler();
            }
            else if constexpr (IsArrayMap<NormalizedT>::value)
            {
                class ArrayMapHandler final : public ITypeInfoMapHandler
                {
                public:
                    virtual ITypeInfoHandler* Clone() const override
                    {
                        return new ArrayMapHandler();
                    }

                    virtual bool CreateInstance(Any& outInstance) const override
                    {
                        using MapType = NormalizedT;

                        outInstance = Any(MapType {});
                        return true;
                    }

                    virtual AnyRef GetValueAt(AnyRef mapRef, AnyRef key) const override
                    {
                        using MapType = NormalizedT;
                        using KeyType = typename MapType::KeyType;
                        using ValueType = typename MapType::ValueType;

                        MapType& map = mapRef.Get<MapType>();
                        KeyType& k = key.Get<KeyType>();

                        auto it = map.Find(k);
                        if (it != map.End())
                        {
                            return AnyRef(&it->second);
                        }

                        return AnyRef();
                    }

                    virtual void SetValueAt(AnyRef mapRef, AnyRef key, AnyRef value) const override
                    {
                        using MapType = NormalizedT;
                        using KeyType = typename MapType::KeyType;
                        using ValueType = typename MapType::ValueType;

                        MapType& map = mapRef.Get<MapType>();
                        KeyType& k = key.Get<KeyType>();
                        ValueType& v = value.Get<ValueType>();

                        map[k] = v;
                    }

                    virtual bool ContainsKey(AnyRef mapRef, AnyRef key) const override
                    {
                        using MapType = NormalizedT;
                        using KeyType = typename MapType::KeyType;

                        MapType& map = mapRef.Get<MapType>();
                        KeyType& k = key.Get<KeyType>();

                        return map.Find(k) != map.End();
                    }

                    virtual bool RemoveKey(AnyRef mapRef, AnyRef key) const override
                    {
                        using MapType = NormalizedT;
                        using KeyType = typename MapType::KeyType;

                        MapType& map = mapRef.Get<MapType>();
                        KeyType& k = key.Get<KeyType>();

                        auto it = map.Find(k);
                        if (it != map.End())
                        {
                            map.Erase(it);
                            return true;
                        }

                        return false;
                    }

                    virtual SizeType GetSize(AnyRef mapRef) const override
                    {
                        using MapType = NormalizedT;

                        MapType& map = mapRef.Get<MapType>();

                        return map.Size();
                    }
                };

                result.flags |= TypeAttributeFlags::MAP_TYPE;

                result.extendedInfo.data.typeInfo = &ForType<typename NormalizedT::KeyType>();
                result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

                result.extendedInfo.next = new TypeInfoEx();
                result.extendedInfo.next->data.typeInfo = &ForType<typename NormalizedT::ValueType>();
                result.extendedInfo.next->dataType = TypeInfoEx::DT_TYPE_INFO;

                result.extendedInfo.handler = new ArrayMapHandler();
            }
            else if constexpr (IsFlatSet<NormalizedT>::value)
            {
                result.flags |= TypeAttributeFlags::SET_TYPE;
                using ValueType = typename NormalizedT::ValueType;
                result.extendedInfo.data.typeInfo = &ForType<ValueType>();
                result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
            }
            else if constexpr (IsVariant<NormalizedT>::value)
            {
                result.flags |= TypeAttributeFlags::VARIANT_TYPE;

                constexpr SizeType variantSize = IsVariant<NormalizedT>::size;

                // Store alternative types in linked list without using a generic lambda
                // which has been observed to crash some clang versions.
                if constexpr (variantSize > 0)
                {
                    // Build a fixed array of TypeInfo* for each variant alternative using
                    // the namespace-level helper. Cast variantSize to std::size_t for the
                    // index sequence.
                    auto altArray = BuildVariantTypeArray<NormalizedT>(std::make_index_sequence<static_cast<std::size_t>(variantSize)> {});

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
            else if constexpr (IsVec2<NormalizedT>::value)
            {
                result.flags |= TypeAttributeFlags::VEC2_TYPE;
                using ElementType = typename NormalizedT::Type;
                result.extendedInfo.data.typeInfo = &ForType<ElementType>();
                result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

                // set handler
                class Vec2Handler final : public ITypeInfoVectorHandler
                {
                public:
                    virtual ITypeInfoHandler* Clone() const override
                    {
                        return new Vec2Handler();
                    }

                    virtual bool CreateInstance(Any& outInstance) const override
                    {
                        using Vec2Type = NormalizedT;

                        outInstance = Any(Vec2Type {});
                        return true;
                    }

                    virtual int GetNumComponents() const override
                    {
                        return 2;
                    }

                    virtual AnyRef GetComponent(AnyRef vectorRef, int index) const override
                    {
                        using Vec2Type = NormalizedT;
                        using ValueType = typename Vec2Type::Type;

                        Vec2Type& vec = vectorRef.Get<Vec2Type>();
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

                    virtual void SetComponent(AnyRef vectorRef, int index, AnyRef value) const override
                    {
                        using Vec2Type = NormalizedT;
                        using ValueType = typename Vec2Type::Type;

                        Vec2Type& vec = vectorRef.Get<Vec2Type>();
                        ValueType& v = value.Get<ValueType>();

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
            else if constexpr (IsVec3<NormalizedT>::value)
            {
                result.flags |= TypeAttributeFlags::VEC3_TYPE;
                using ElementType = typename NormalizedT::Type;
                result.extendedInfo.data.typeInfo = &ForType<ElementType>();
                result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

                // set handler
                class Vec3Handler final : public ITypeInfoVectorHandler
                {
                public:
                    virtual ITypeInfoHandler* Clone() const override
                    {
                        return new Vec3Handler();
                    }

                    virtual bool CreateInstance(Any& outInstance) const override
                    {
                        using Vec3Type = NormalizedT;

                        outInstance = Any(Vec3Type {});
                        return true;
                    }

                    virtual int GetNumComponents() const override
                    {
                        return 3;
                    }

                    virtual AnyRef GetComponent(AnyRef vectorRef, int index) const override
                    {
                        using Vec3Type = NormalizedT;
                        using ValueType = typename Vec3Type::Type;

                        Vec3Type& vec = vectorRef.Get<Vec3Type>();
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

                    virtual void SetComponent(AnyRef vectorRef, int index, AnyRef value) const override
                    {
                        using Vec3Type = NormalizedT;
                        using ValueType = typename Vec3Type::Type;

                        Vec3Type& vec = vectorRef.Get<Vec3Type>();
                        ValueType& v = value.Get<ValueType>();

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
            else if constexpr (IsVec4<NormalizedT>::value)
            {
                result.flags |= TypeAttributeFlags::VEC4_TYPE;
                using ElementType = typename NormalizedT::Type;
                result.extendedInfo.data.typeInfo = &ForType<ElementType>();
                result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

                // set handler
                class Vec4Handler final : public ITypeInfoVectorHandler
                {
                public:
                    virtual ITypeInfoHandler* Clone() const override
                    {
                        return new Vec4Handler();
                    }

                    virtual bool CreateInstance(Any& outInstance) const override
                    {
                        using Vec4Type = NormalizedT;

                        outInstance = Any(Vec4Type {});
                        return true;
                    }

                    virtual int GetNumComponents() const override
                    {
                        return 4;
                    }

                    virtual AnyRef GetComponent(AnyRef vectorRef, int index) const override
                    {
                        using Vec4Type = NormalizedT;
                        using ValueType = typename Vec4Type::Type;

                        Vec4Type& vec = vectorRef.Get<Vec4Type>();
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

                    virtual void SetComponent(AnyRef vectorRef, int index, AnyRef value) const override
                    {
                        using Vec4Type = NormalizedT;
                        using ValueType = typename Vec4Type::Type;

                        Vec4Type& vec = vectorRef.Get<Vec4Type>();
                        ValueType& v = value.Get<ValueType>();

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

            ValueStorage<Mutex::Guard> guardStorage;

            TypeInfo* pTypeInfo = TypeInfo_Alloc(
                typeId,
                uint16(sizeof(NormalizedT)),
                uint16(alignof(NormalizedT)),
                guardStorage.GetPointer());

            HYP_CORE_ASSERT(pTypeInfo != nullptr);

            new (pTypeInfo) TypeInfo(std::move(result));

            guardStorage.GetPointer()->~Guard();

            return *pTypeInfo;
        }
    }

    HYP_FORCE_INLINE constexpr bool IsPod() const
    {
        return flags & TypeAttributeFlags::POD_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsClass() const
    {
        return flags & TypeAttributeFlags::CLASS_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsStruct() const
    {
        return flags & TypeAttributeFlags::STRUCT_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsEnum() const
    {
        return flags & TypeAttributeFlags::ENUM_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsFundamental() const
    {
        return flags & TypeAttributeFlags::FUNDAMENTAL_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsIntegralType() const
    {
        return flags & TypeAttributeFlags::INTEGRAL_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsFloatType() const
    {
        return flags & TypeAttributeFlags::FLOAT_TYPE;
    }

    HYP_FORCE_INLINE const HypClass* GetHypClass() const
    {
        return GetClass(id);
    }

    HYP_FORCE_INLINE bool IsArrayType() const
    {
        return flags & TypeAttributeFlags::ARRAY_TYPE;
    }

    HYP_FORCE_INLINE bool IsLinkedListType() const
    {
        return flags & TypeAttributeFlags::LINKEDLIST_TYPE;
    }

    HYP_FORCE_INLINE bool IsStringType() const
    {
        return flags & TypeAttributeFlags::STRING_TYPE;
    }

    HYP_FORCE_INLINE bool IsMapType() const
    {
        return flags & TypeAttributeFlags::MAP_TYPE;
    }

    HYP_FORCE_INLINE bool IsSetType() const
    {
        return flags & TypeAttributeFlags::SET_TYPE;
    }

    HYP_FORCE_INLINE bool IsVariantType() const
    {
        return flags & TypeAttributeFlags::VARIANT_TYPE;
    }

    HYP_FORCE_INLINE bool IsBoolType() const
    {
        return id == TypeId::ForType<bool>();
    }

    HYP_FORCE_INLINE bool IsVec2Type() const
    {
        return flags & TypeAttributeFlags::VEC2_TYPE;
    }

    HYP_FORCE_INLINE bool IsVec3Type() const
    {
        return flags & TypeAttributeFlags::VEC3_TYPE;
    }

    HYP_FORCE_INLINE bool IsVec4Type() const
    {
        return flags & TypeAttributeFlags::VEC4_TYPE;
    }

    HYP_FORCE_INLINE bool IsVectorType() const
    {
        return flags & TypeAttributeFlags::VECTOR_TYPE;
    }

    HYP_FORCE_INLINE bool IsContainerType() const
    {
        constexpr EnumFlags<TypeAttributeFlags> Mask = TypeAttributeFlags::ARRAY_TYPE
            | TypeAttributeFlags::STRING_TYPE
            | TypeAttributeFlags::LINKEDLIST_TYPE
            | TypeAttributeFlags::MAP_TYPE
            | TypeAttributeFlags::SET_TYPE;

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

        virtual bool CreateInstance(Any& outInstance) const override
        {
            outInstance = Any(T {});

            return true;
        }

        virtual bool GetElementAt(AnyRef arrayRef, SizeType index, HypDataType& outValue) const override
        {
            T& array = arrayRef.Get<T>();

            if (index >= array.Size())
            {
                return false;
            }

            return array.GetElementAt(index, outValue);
        }

        virtual bool SetElementAt(AnyRef arrayRef, SizeType index, HypDataType&& value) const override
        {
            T& array = arrayRef.Get<T>();
            if (index >= array.Size())
            {
                return false;
            }

            return array.SetElementAt(index, std::move(value));
        }

        virtual SizeType GetSize(AnyRef arrayRef) const override
        {
            T& array = arrayRef.Get<T>();
            return array.Size();
        }

        virtual void Resize(AnyRef arrayRef, SizeType newSize) const override
        {
            T& array = arrayRef.Get<T>();

            if (!array.Resize(newSize))
            {
                HYP_CORE_ASSERT(false, "Failed to resize GenericArrayWrapper");
            }
        }
    };

    result.flags |= TypeAttributeFlags::ARRAY_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<HypData>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    result.extendedInfo.handler = new GenericArrayHandler();
}

template <class T, class AllocatorType, class HypDataType>
void TypeInfoImpl<Array<T, AllocatorType>, HypDataType>::operator()(TypeInfo& result) const
{
    using ArrayType = Array<T, AllocatorType>;

    class ArrayHandler final : public ITypeInfoArrayHandler
    {
    public:
        virtual ITypeInfoHandler* Clone() const override
        {
            return new ArrayHandler();
        }

        virtual bool CreateInstance(Any& outInstance) const override
        {
            outInstance = Any(ArrayType {});
            return true;
        }

        virtual bool GetElementAt(AnyRef arrayRef, SizeType index, HypDataType& outValue) const override
        {
            ArrayType& array = arrayRef.Get<ArrayType>();

            if (index >= array.Size())
            {
                return false;
            }

            outValue = HypDataType(AnyRef(&array[index]));

            return true;
        }

        virtual bool SetElementAt(AnyRef arrayRef, SizeType index, HypDataType&& value) const override
        {
            ArrayType& array = arrayRef.Get<ArrayType>();

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

        virtual SizeType GetSize(AnyRef arrayRef) const override
        {
            ArrayType& array = arrayRef.Get<ArrayType>();
            return array.Size();
        }

        virtual void Resize(AnyRef arrayRef, SizeType newSize) const override
        {
            ArrayType& array = arrayRef.Get<ArrayType>();
            array.Resize(newSize);
        }
    };

    result.flags |= TypeAttributeFlags::ARRAY_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<T>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
    result.extendedInfo.handler = new ArrayHandler();
}

template <class T, SizeType Size, class HypDataType>
void TypeInfoImpl<FixedArray<T, Size>, HypDataType>::operator()(TypeInfo& result) const
{
    using FixedArrayType = FixedArray<T, Size>;

    class FixedArrayHandler final : public ITypeInfoArrayHandler
    {
    public:
        virtual ITypeInfoHandler* Clone() const override
        {
            return new FixedArrayHandler();
        }

        virtual bool CreateInstance(Any& outInstance) const override
        {
            outInstance = Any(FixedArrayType {});
            return true;
        }

        virtual bool GetElementAt(AnyRef arrayRef, SizeType index, HypDataType& outValue) const override
        {
            FixedArrayType& array = arrayRef.Get<FixedArrayType>();

            if (index >= array.Size())
            {
                return false;
            }

            outValue = HypData(AnyRef(&array[index]));

            return true;
        }

        virtual bool SetElementAt(AnyRef arrayRef, SizeType index, HypDataType&& value) const override
        {
            FixedArrayType& array = arrayRef.Get<FixedArrayType>();

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

        virtual SizeType GetSize(AnyRef arrayRef) const override
        {
            FixedArrayType& array = arrayRef.Get<FixedArrayType>();
            return array.Size();
        }

        virtual void Resize(AnyRef arrayRef, SizeType newSize) const override
        {
            // FixedArray has a fixed size, so resizing is not supported
            // This operation is a no-op
        }
    };

    result.flags |= TypeAttributeFlags::ARRAY_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<T>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
    result.extendedInfo.handler = new FixedArrayHandler();
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

inline const TypeId& TypeInfo_GetId(const TypeInfo& type_info)
{
    return type_info.id;
}

inline const Name& TypeInfo_GetName(const TypeInfo& type_info)
{
    return type_info.name;
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
using utilities::ITypeInfoStringHandler;
using utilities::ITypeInfoVectorHandler;

using utilities::TypeInfo_ForHypClass;
using utilities::TypeInfo_ForType;
using utilities::TypeInfo_Void;

using utilities::TypeInfo;
using utilities::TypeInfo_Initialize;
using utilities::TypeInfo_Shutdown;

} // namespace hyperion
