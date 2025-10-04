/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/TypeId.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <core/memory/AnyRef.hpp>

#include <core/Name.hpp>
#include <core/Types.hpp>
#include <core/Traits.hpp>
#include <core/HashCode.hpp>

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

    HYP_CLASS = 0x1000,

    // Container types
    ARRAY_TYPE = 0x2000,
    STRING_TYPE = 0x4000,
    HASHMAP_TYPE = 0x8000,
    HASHSET_TYPE = 0x10000,
    FLATMAP_TYPE = 0x20000,
    FLATSET_TYPE = 0x40000,
    VARIANT_TYPE = 0x80000,

    // Vector types
    VEC2_TYPE = 0x100000,
    VEC3_TYPE = 0x200000,
    VEC4_TYPE = 0x400000
};

HYP_MAKE_ENUM_FLAGS(TypeAttributeFlags)

class HypClass;
extern HYP_API const HypClass* GetClass(TypeId typeId);

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

/*! \brief Get cached type attributes for a given TypeId
 *  \param typeId The TypeId to get attributes for
 *  \param typeSize The size of the type
 *  \param typeAlignment The alignment of the type
 *  \return Pointer to TypeInfo if found, nullptr otherwise */
HYP_API extern TypeInfo* TypeInfo_FetchFromCache(TypeId typeId, SizeType typeSize, SizeType typeAlignment);

/*! \brief Allocate a TypeInfo instance from the pool
 *  \param typeId The TypeId of the type
 *  \param typeSize The size of the type
 *  \param typeAlignment The alignment of the type
 *  \param outPGuard Optional pointer to construct a Mutex::Guard into, to lock the type attribute cache mutex
 *   If nullptr, it is assumed the caller already holds exclusive access to the cache mutex
 *  \return Pointer to newly allocated TypeInfo instance */
HYP_API extern TypeInfo* TypeInfo_Alloc(
    TypeId typeId, SizeType typeSize, SizeType typeAlignment,
    Mutex::Guard* pGuard);

/*! \brief Initialize the TypeInfo system, must be called before any other TypeInfo functions */
HYP_API extern void TypeInfo_Initialize();

/*! \brief Free all allocated TypeInfo instances and clear the cache */
HYP_API extern void TypeInfo_Shutdown();

class ITypeAttributeHandler
{
public:
    enum Type
    {
        TYPE_NONE,
        TYPE_ARRAY,
        TYPE_MAP,
        TYPE_STRING
    };

    virtual ~ITypeAttributeHandler() = default;

    virtual ITypeAttributeHandler* Clone() const = 0;
    virtual Type GetHandlerType() const = 0;
};

class ITypeAttributeArrayHandler : public ITypeAttributeHandler
{
public:
    virtual ~ITypeAttributeArrayHandler() = default;

    virtual ITypeAttributeHandler* Clone() const override = 0;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_ARRAY;
    }

    virtual AnyRef GetElementAt(AnyRef arrayRef, SizeType index) const = 0;
    virtual void SetElementAt(AnyRef arrayRef, SizeType index, AnyRef value) const = 0;

    virtual SizeType GetSize(AnyRef arrayRef) const = 0;

    virtual void Resize(AnyRef arrayRef, SizeType newSize) const = 0;
};

class ITypeAttributeMapHandler : public ITypeAttributeHandler
{
public:
    virtual ~ITypeAttributeMapHandler() = default;

    virtual ITypeAttributeHandler* Clone() const override = 0;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_MAP;
    }

    virtual AnyRef GetValueAt(AnyRef mapRef, AnyRef key) const = 0;
    virtual void SetValueAt(AnyRef mapRef, AnyRef key, AnyRef value) const = 0;

    virtual bool ContainsKey(AnyRef mapRef, AnyRef key) const = 0;
    virtual bool RemoveKey(AnyRef mapRef, AnyRef key) const = 0;

    virtual SizeType GetSize(AnyRef mapRef) const = 0;
};

class ITypeAttributeStringHandler : public ITypeAttributeHandler
{
public:
    virtual ~ITypeAttributeStringHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_STRING;
    }

    virtual ITypeAttributeHandler* Clone() const override = 0;

    virtual String GetValue(AnyRef stringRef) const = 0;
};

/*! \brief Additional type information for containers and complex types */
struct HYP_API TypeInfoEx
{
    enum DataType
    {
        DT_NONE = 0,
        DT_TYPE_INFO = 1,
        DT_HYP_CLASS = 2
    };

    /*! \brief Tagged union holding either:
     *  - const TypeInfo* for container element types (single type)
     *  - const HypClass* for types with HypClass reflection info */
    union
    {
        const TypeInfo* typeInfo;
        const HypClass* hypClass;
    } data;

    DataType dataType : 2;

    ITypeAttributeHandler* handler = nullptr;

    /*! \brief Linked list pointer for:
     *  - HashMap<K,V>, FlatMap<K,V>: next points to value type
     *  - Variant<Types...>: next points to next alternative type in chain
     *  - String types: character encoding type info */
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

    /*! \brief Get HypClass pointer if this holds a const HypClass* */
    HYP_FORCE_INLINE const HypClass* GetHypClass() const
    {
        if (dataType == DT_HYP_CLASS)
        {
            return data.hypClass;
        }

        return nullptr;
    }

    HashCode GetHashCode() const;
};

struct TypeInfo
{
    /*! \brief Get a default TypeInfo instance representing void type */
    HYP_API static const TypeInfo& Void();

    TypeId id = TypeId::Void();
    Name name = Name::Invalid();
    SizeType size = 0;
    SizeType alignment = 0;
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

            // Check cache first
            if (TypeInfo* cached = TypeInfo_FetchFromCache(typeId, sizeof(NormalizedT), alignof(NormalizedT)))
            {
                return *cached;
            }

            TypeInfo result;
            result.id = typeId;
            result.name = CreateNameFromDynamicString(TypeNameWithoutNamespace<NormalizedT>().Data());
            result.size = sizeof(NormalizedT);
            result.alignment = alignof(NormalizedT);
            result.flags = TypeAttributeFlags::NONE;

            const HypClass* hypClass = nullptr;

            if constexpr (std::is_class_v<NormalizedT>)
            {
                result.flags |= TypeAttributeFlags::CLASS_TYPE;

                hypClass = GetClass(typeId);

                if (hypClass)
                {
                    result.flags |= TypeAttributeFlags::HYP_CLASS;
                }

                if constexpr (isPodType<NormalizedT>)
                {
                    result.flags |= TypeAttributeFlags::POD_TYPE;
                }
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

            if constexpr (std::is_floating_point_v<NormalizedT>)
            {
                result.flags |= TypeAttributeFlags::FLOAT_TYPE;
            }

            // Store HypClass in extended info if present
            if (hypClass)
            {
                result.extendedInfo.data.hypClass = hypClass;
                result.extendedInfo.dataType = TypeInfoEx::DT_HYP_CLASS;
            }
            else
            {
                // Detect container types using type traits
                // Check Array types
                if constexpr (IsArray<NormalizedT>::value)
                {
                    class ArrayHandler final : public ITypeAttributeArrayHandler
                    {
                    public:
                        virtual ITypeAttributeHandler* Clone() const override
                        {
                            return new ArrayHandler();
                        }

                        virtual AnyRef GetElementAt(AnyRef arrayRef, SizeType index) const override
                        {
                            using ArrayType = NormalizedT;
                            using ValueType = typename ArrayType::ValueType;

                            ArrayType& array = arrayRef.Get<ArrayType>();
                            return AnyRef(&array[index]);
                        }

                        virtual void SetElementAt(AnyRef arrayRef, SizeType index, AnyRef value) const override
                        {
                            using ArrayType = NormalizedT;
                            using ValueType = typename ArrayType::ValueType;

                            ArrayType& array = arrayRef.Get<ArrayType>();
                            ValueType& v = value.Get<ValueType>();

                            array[index] = v;
                        }

                        virtual SizeType GetSize(AnyRef arrayRef) const override
                        {
                            using ArrayType = NormalizedT;

                            ArrayType& array = arrayRef.Get<ArrayType>();
                            return array.Size();
                        }

                        virtual void Resize(AnyRef arrayRef, SizeType newSize) const override
                        {
                            using ArrayType = NormalizedT;

                            ArrayType& array = arrayRef.Get<ArrayType>();
                            array.Resize(newSize);
                        }
                    };

                    result.flags |= TypeAttributeFlags::ARRAY_TYPE;

                    result.extendedInfo.data.typeInfo = &ForType<typename NormalizedT::ValueType>();
                    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
                    result.extendedInfo.handler = new ArrayHandler();
                }
                // Check String types
                else if constexpr (IsString<NormalizedT>::value)
                {
                    class StringHandler final : public ITypeAttributeStringHandler
                    {
                    public:
                        virtual ITypeAttributeHandler* Clone() const override
                        {
                            return new StringHandler();
                        }

                        virtual String GetValue(AnyRef stringRef) const override
                        {
                            using StringType = NormalizedT;

                            StringType& string = stringRef.Get<StringType>();
                            return string.ToUTF8();
                        }
                    };

                    result.flags |= TypeAttributeFlags::STRING_TYPE;

                    result.extendedInfo.data.typeInfo = &ForType<typename NormalizedT::ValueType>();
                    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
                    result.extendedInfo.handler = new StringHandler();
                }
                // Check HashMap types
                else if constexpr (IsHashMap<NormalizedT>::value)
                {
                    class HashMapHandler final : public ITypeAttributeMapHandler
                    {
                    public:
                        virtual ITypeAttributeHandler* Clone() const override
                        {
                            return new HashMapHandler();
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

                    result.flags |= TypeAttributeFlags::HASHMAP_TYPE;

                    result.extendedInfo.data.typeInfo = &ForType<typename NormalizedT::KeyType>();
                    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
                    result.extendedInfo.next = new TypeInfoEx();
                    result.extendedInfo.next->data.typeInfo = &ForType<typename NormalizedT::ValueType>();
                    result.extendedInfo.next->dataType = TypeInfoEx::DT_TYPE_INFO;
                    result.extendedInfo.handler = new HashMapHandler();
                }
                // Check HashSet types
                else if constexpr (IsHashSet<NormalizedT>::value)
                {
                    result.flags |= TypeAttributeFlags::HASHSET_TYPE;
                    using ValueType = typename NormalizedT::ValueType;
                    result.extendedInfo.data.typeInfo = &ForType<ValueType>();
                    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
                }
                // Check FlatMap types
                else if constexpr (IsFlatMap<NormalizedT>::value)
                {
                    class FlatMapHandler final : public ITypeAttributeMapHandler
                    {
                    public:
                        virtual ITypeAttributeHandler* Clone() const override
                        {
                            return new FlatMapHandler();
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

                    result.flags |= TypeAttributeFlags::FLATMAP_TYPE;

                    result.extendedInfo.data.typeInfo = &ForType<typename NormalizedT::KeyType>();
                    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
                    result.extendedInfo.next = new TypeInfoEx();
                    result.extendedInfo.next->data.typeInfo = &ForType<typename NormalizedT::ValueType>();
                    result.extendedInfo.next->dataType = TypeInfoEx::DT_TYPE_INFO;
                    result.extendedInfo.handler = new FlatMapHandler();
                }
                // Check FlatSet types
                else if constexpr (IsFlatSet<NormalizedT>::value)
                {
                    result.flags |= TypeAttributeFlags::FLATSET_TYPE;
                    using ValueType = typename NormalizedT::ValueType;
                    result.extendedInfo.data.typeInfo = &ForType<ValueType>();
                    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
                }
                // Check Variant types
                else if constexpr (IsVariant<NormalizedT>::value)
                {
                    result.flags |= TypeAttributeFlags::VARIANT_TYPE;

                    constexpr SizeType variantSize = IsVariant<NormalizedT>::size;

                    // Store alternative types in linked list
                    if constexpr (variantSize > 0)
                    {
                        // Helper to build linked list of variant alternative types
                        auto buildVariantTypes = []<SizeType... Indices>(std::index_sequence<Indices...>) -> TypeInfoEx*
                        {
                            TypeInfoEx* head = nullptr;
                            TypeInfoEx* current = nullptr;

                            ((void)[&]() {
                                using AlternativeType = typename IsVariant<NormalizedT>::template TypeAtIndex<Indices>;

                                TypeInfoEx* node = new TypeInfoEx();
                                node->data.typeInfo = &ForType<AlternativeType>();
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
                            }(),
                                ...);

                            return head;
                        };

                        result.extendedInfo.next = buildVariantTypes(std::make_index_sequence<variantSize> {});
                    }
                }
                else if constexpr (IsVec2<NormalizedT>::value)
                {
                    result.flags |= TypeAttributeFlags::VEC2_TYPE;
                    using ElementType = typename NormalizedT::Type;
                    result.extendedInfo.data.typeInfo = &ForType<ElementType>();
                    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
                }
                else if constexpr (IsVec3<NormalizedT>::value)
                {
                    result.flags |= TypeAttributeFlags::VEC3_TYPE;
                    using ElementType = typename NormalizedT::Type;
                    result.extendedInfo.data.typeInfo = &ForType<ElementType>();
                    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
                }
                else if constexpr (IsVec4<NormalizedT>::value)
                {
                    result.flags |= TypeAttributeFlags::VEC4_TYPE;
                    using ElementType = typename NormalizedT::Type;
                    result.extendedInfo.data.typeInfo = &ForType<ElementType>();
                    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
                }
            }

            ValueStorage<Mutex::Guard> guardStorage;

            TypeInfo* pTypeInfo = TypeInfo_Alloc(
                typeId,
                sizeof(NormalizedT),
                alignof(NormalizedT),
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

    HYP_FORCE_INLINE constexpr bool HasHypClass() const
    {
        return flags & TypeAttributeFlags::HYP_CLASS;
    }

    HYP_FORCE_INLINE const HypClass* GetHypClass() const
    {
        return extendedInfo.GetHypClass();
    }

    HYP_FORCE_INLINE bool IsArrayType() const
    {
        return flags & TypeAttributeFlags::ARRAY_TYPE;
    }

    HYP_FORCE_INLINE bool IsStringType() const
    {
        return flags & TypeAttributeFlags::STRING_TYPE;
    }

    HYP_FORCE_INLINE bool IsHashMapType() const
    {
        return flags & TypeAttributeFlags::HASHMAP_TYPE;
    }

    HYP_FORCE_INLINE bool IsHashSetType() const
    {
        return flags & TypeAttributeFlags::HASHSET_TYPE;
    }

    HYP_FORCE_INLINE bool IsFlatMapType() const
    {
        return flags & TypeAttributeFlags::FLATMAP_TYPE;
    }

    HYP_FORCE_INLINE bool IsFlatSetType() const
    {
        return flags & TypeAttributeFlags::FLATSET_TYPE;
    }

    HYP_FORCE_INLINE bool IsVariantType() const
    {
        return flags & TypeAttributeFlags::VARIANT_TYPE;
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
        constexpr EnumFlags<TypeAttributeFlags> mask = TypeAttributeFlags::VEC2_TYPE
            | TypeAttributeFlags::VEC3_TYPE
            | TypeAttributeFlags::VEC4_TYPE;

        return flags & mask;
    }

    HYP_FORCE_INLINE bool IsContainerType() const
    {
        constexpr EnumFlags<TypeAttributeFlags> mask = TypeAttributeFlags::ARRAY_TYPE
            | TypeAttributeFlags::STRING_TYPE
            | TypeAttributeFlags::HASHMAP_TYPE
            | TypeAttributeFlags::HASHSET_TYPE
            | TypeAttributeFlags::FLATMAP_TYPE
            | TypeAttributeFlags::FLATSET_TYPE;

        return flags & mask;
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
        if (IsHashMapType() || IsFlatMapType())
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

extern HYP_API const TypeInfo& TypeInfo_Void();

template <class T>
inline const TypeInfo& TypeInfo_ForType()
{
    return TypeInfo::ForType<T>();
}

} // namespace utilities

using utilities::TypeInfo_ForType;
using utilities::TypeInfo_Void;

using utilities::TypeInfo;
using utilities::TypeInfo_Initialize;
using utilities::TypeInfo_Shutdown;

} // namespace hyperion
