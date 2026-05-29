/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/DeferredScope.hpp>
#include <Core/utilities/Pair.hpp>

#include <Core/memory/AnyRef.hpp>
#include <Core/memory/Any.hpp>

#include <Core/name/Name.hpp>

#include <Core/containers/FixedArray.hpp>
#include <Core/containers/String.hpp>

#include <Core/reflection/TypeId.hpp>
#include <Core/reflection/ObjectFwd.hpp>

#include <type_traits>

namespace Hyperion {

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
    MATRIX_TYPE = MAT3_TYPE | MAT4_TYPE,

    // Tuple/Pair types
    TUPLE_TYPE = 0x4000000,
    PAIR_TYPE = 0x8000000,

    HANDLE_TYPE = 0x10000000
};

HYP_MAKE_ENUM_FLAGS(TypeInfoFlags)

class Class;
struct BoxedValue;
struct GenericArrayWrapper;

struct Float16;

HYP_API extern const Class* GetClass(const TypeId& typeId);
HYP_API extern bool ClassRegistry_IsInitialized();

namespace containers {

template <int TStringType>
class String;

template <class T, class AllocatorType>
class LinkedList;

template <class Key, class Value, class AllocatorType, class Policy>
class TMap;

template <class Value, class AllocatorType, class Policy>
class TSet;

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

template <class... Types>
class Tuple;

} // namespace utilities

namespace filesystem {
class FilePath;
} // namespace filesystem

namespace utilities {

struct TypeInfo;

// Forward-declare the free helper so BuildVariantTypeArray can use it without
// needing the full TypeInfo definition yet.
template <class T>
const TypeInfo& TypeOf();

// Helper to build a FixedArray of TypeInfo* for variant alternative types.
// Forward-declared here; the definition is placed after `TypeInfo` and
// `TypeOf` are fully defined, to avoid referencing incomplete types
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

/*! \brief Iterator interface for traversing container elements
 *  Provides a generic way to iterate over sets, maps, and other containers */
class ITypeInfoIterator
{
public:
    virtual ~ITypeInfoIterator() = default;

    /*! \brief Check if there are more elements to iterate */
    virtual bool HasNext() const = 0;

    /*! \brief Advance to the next element
     *  \return true if advanced successfully, false if at end */
    virtual bool Next() = 0;

    /*! \brief Get the current element value
     *  For sets/arrays: returns the element
     *  For maps: returns the value (use GetKey() for the key)
     *  \return AnyRef to the current element, or empty AnyRef if invalid */
    virtual AnyRef GetCurrent() const = 0;

    /*! \brief Get the current key (for map iterators only)
     *  \return AnyRef to the current key, or empty AnyRef if not a map iterator */
    virtual AnyRef GetKey() const = 0;

    /*! \brief Reset iterator to the beginning */
    virtual void Reset() = 0;

    /*! \brief Clone this iterator */
    virtual ITypeInfoIterator* Clone() const = 0;
};

class ITypeInfoHandler
{
public:
    enum Type
    {
        TYPE_NONE,
        TYPE_ARRAY,
        TYPE_LINKEDLIST,
        TYPE_MAP,
        TYPE_SET,
        TYPE_STRING,
        TYPE_VECTOR,
        TYPE_MATRIX,
        TYPE_VARIANT,
        TYPE_TUPLE,
        TYPE_PAIR,
        TYPE_HANDLE
    };

    virtual ~ITypeInfoHandler() = default;

    virtual Type GetHandlerType() const = 0;

    virtual bool CreateInstance(BoxedValue& outInstance) const = 0;
};

class ITypeInfoArrayHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoArrayHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_ARRAY;
    }

    virtual bool CreateInstance(BoxedValue& outInstance) const override = 0;

    virtual bool GetElementAt(const BoxedValue& instance, size_t index, BoxedValue& outValue) const = 0;
    virtual bool SetElementAt(const BoxedValue& instance, size_t index, BoxedValue&& value) const = 0;

    virtual size_t GetSize(const BoxedValue& instance) const = 0;

    virtual void Resize(const BoxedValue& instance, size_t newSize) const = 0;
};

class ITypeInfoLinkedListHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoLinkedListHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_LINKEDLIST;
    }

    virtual bool CreateInstance(BoxedValue& outInstance) const override = 0;

    virtual AnyRef GetElementAt(const BoxedValue& instance, size_t index) const = 0;
    virtual void SetElementAt(const BoxedValue& instance, size_t index, const BoxedValue& value) const = 0;

    virtual size_t GetSize(const BoxedValue& instance) const = 0;

    virtual void Resize(const BoxedValue& instance, size_t newSize) const = 0;
};

class ITypeInfoMapHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoMapHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_MAP;
    }

    virtual bool CreateInstance(BoxedValue& outInstance) const override = 0;

    virtual AnyRef GetValueAt(const BoxedValue& instance, const BoxedValue& key) const = 0;
    virtual void SetValueAt(const BoxedValue& instance, const BoxedValue& key, const BoxedValue& value) const = 0;

    virtual bool ContainsKey(const BoxedValue& instance, const BoxedValue& key) const = 0;
    virtual bool RemoveKey(const BoxedValue& instance, const BoxedValue& key) const = 0;

    virtual size_t GetSize(const BoxedValue& instance) const = 0;

    /*! \brief Create an iterator for the map.
     *  \returns A new iterator (caller takes ownership and must delete). GetCurrent() returns the value; GetKey() returns the key. */
    virtual ITypeInfoIterator* CreateIterator(const BoxedValue& instance) const = 0;
};

class ITypeInfoSetHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoSetHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_SET;
    }

    virtual bool CreateInstance(BoxedValue& outInstance) const override = 0;

    virtual bool Contains(const BoxedValue& instance, const BoxedValue& value) const = 0;
    virtual bool Insert(const BoxedValue& instance, const BoxedValue& value) const = 0;
    virtual bool Remove(const BoxedValue& instance, const BoxedValue& value) const = 0;

    virtual size_t GetSize(const BoxedValue& instance) const = 0;

    /*! \brief Create an iterator for the set
     *  \param instance The set instance to iterate over
     *  \return A new iterator (caller takes ownership and must delete) */
    virtual ITypeInfoIterator* CreateIterator(const BoxedValue& instance) const = 0;
};

class ITypeInfoStringHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoStringHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_STRING;
    }

    virtual bool CreateInstance(BoxedValue& outInstance) const override = 0;

    virtual String GetValue(const BoxedValue& instance) const = 0;
    virtual void SetValue(const BoxedValue& instance, const UTF8StringView& str) const = 0;
};

class ITypeInfoVectorHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoVectorHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_VECTOR;
    }

    virtual bool CreateInstance(BoxedValue& outInstance) const override = 0;

    virtual int GetNumComponents() const = 0;

    virtual AnyRef GetComponent(const BoxedValue& instance, int index) const = 0;
    virtual void SetComponent(const BoxedValue& instance, int index, const BoxedValue& value) const = 0;
};

class ITypeInfoMatrixHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoMatrixHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_MATRIX;
    }

    virtual bool CreateInstance(BoxedValue& outInstance) const override = 0;

    virtual int GetNumRows() const = 0;
    virtual int GetNumColumns() const = 0;

    virtual AnyRef GetElement(const BoxedValue& instance, int row, int column) const = 0;
    virtual void SetElement(const BoxedValue& instance, int row, int column, const BoxedValue& value) const = 0;
};

class ITypeInfoVariantHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoVariantHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_VARIANT;
    }

    virtual bool CreateInstance(BoxedValue& outInstance) const override = 0;

    virtual int GetNumTypes() const = 0;
    virtual const TypeInfo* GetTypeInfoAtIndex(int typeIndex) const = 0;

    virtual int GetCurrentTypeIndex(const BoxedValue& instance) const = 0;

    virtual AnyRef GetValue(const BoxedValue& instance) const = 0;
    virtual bool SetValue(const BoxedValue& instance, const BoxedValue& value) const = 0;
};

class ITypeInfoTupleHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoTupleHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_TUPLE;
    }

    virtual bool CreateInstance(BoxedValue& outInstance) const override = 0;

    virtual int GetNumElements() const = 0;
    virtual const TypeInfo* GetElementTypeInfoAtIndex(int index) const = 0;

    virtual AnyRef GetElement(const BoxedValue& instance, int index) const = 0;
    virtual bool SetElement(const BoxedValue& instance, int index, const BoxedValue& value) const = 0;
};

class ITypeInfoPairHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoPairHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_PAIR;
    }

    virtual bool CreateInstance(BoxedValue& outInstance) const override = 0;

    virtual const TypeInfo* GetFirstTypeInfo() const = 0;
    virtual const TypeInfo* GetSecondTypeInfo() const = 0;

    virtual void GetFirst(const BoxedValue& instance, BoxedValue& outValue) const = 0;
    virtual void GetSecond(const BoxedValue& instance, BoxedValue& outValue) const = 0;
    virtual void SetFirst(const BoxedValue& instance, const BoxedValue& value) const = 0;
    virtual void SetSecond(const BoxedValue& instance, const BoxedValue& value) const = 0;
};

class ITypeInfoHandleHandler : public ITypeInfoHandler
{
public:
    virtual ~ITypeInfoHandleHandler() = default;

    virtual Type GetHandlerType() const override final
    {
        return TYPE_HANDLE;
    }

    virtual bool CreateInstance(BoxedValue& outInstance) const override = 0;

    virtual AnyRef GetObject(const BoxedValue& instance) const = 0;
    virtual void SetObject(BoxedValue& instance, const BoxedValue& value) const = 0;
};

/*! \brief Additional type information for containers and complex types */
struct TypeInfoEx
{
    enum DataType
    {
        DT_NONE = 0,
        DT_TYPE_INFO = 1
    };

    /*! \brief Tagged union holding either:
     *  - const TypeInfo* for container element types (single type)
     *  - const Class* for types with Class reflection info */
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

template <class T, class TBoxed = BoxedValue, class EnableIf = void>
struct TypeInfoImpl;

template <class T, class TBoxed>
struct TypeInfoImpl<T, TBoxed, std::enable_if_t<std::is_same_v<T, GenericArrayWrapper>>>
{
    void operator()(TypeInfo& result) const;
};

template <class ArrayType, class TBoxed>
struct TypeInfoImpl<ArrayType, TBoxed, std::enable_if_t<IsArray<ArrayType>::value>>
{
    void operator()(TypeInfo& result) const;
};

template <class T, size_t Size, class TBoxed>
struct TypeInfoImpl<containers::FixedArray<T, Size>, TBoxed>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class AllocatorType, class TBoxed>
struct TypeInfoImpl<containers::LinkedList<T, AllocatorType>, TBoxed>
{
    void operator()(TypeInfo& result) const;
};

template <int TStringType, class TBoxed>
struct TypeInfoImpl<containers::String<TStringType>, TBoxed>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class TBoxed>
struct TypeInfoImpl<T, TBoxed, std::enable_if_t<std::is_same_v<T, filesystem::FilePath>>>
{
    void operator()(TypeInfo& result) const;
};

template <class Key, class Value, class NodeAllocatorType, class TBoxed>
struct TypeInfoImpl<containers::TMap<Key, Value, NodeAllocatorType>, TBoxed>
{
    void operator()(TypeInfo& result) const;
};

template <class Value, class NodeAllocatorType, class TBoxed>
struct TypeInfoImpl<containers::TSet<Value, NodeAllocatorType>, TBoxed>
{
    void operator()(TypeInfo& result) const;
};

template <class Key, class Value, class TBoxed>
struct TypeInfoImpl<containers::FlatMap<Key, Value>, TBoxed>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class TBoxed>
struct TypeInfoImpl<containers::FlatSet<T>, TBoxed>
{
    void operator()(TypeInfo& result) const;
};

template <class Key, class Value, class TBoxed>
struct TypeInfoImpl<containers::ArrayMap<Key, Value>, TBoxed>
{
    void operator()(TypeInfo& result) const;
};

template <class... Types, class TBoxed>
struct TypeInfoImpl<utilities::Variant<Types...>, TBoxed>
{
    void operator()(TypeInfo& result) const;
};

template <class... Types, class TBoxed>
struct TypeInfoImpl<utilities::Tuple<Types...>, TBoxed>
{
    void operator()(TypeInfo& result) const;
};

template <class First, class Second, class T1, class T2, class TBoxed>
struct TypeInfoImpl<utilities::detail::Pair<First, Second, T1, T2>, TBoxed>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class TBoxed>
struct TypeInfoImpl<math::Vec2<T>, TBoxed>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class TBoxed>
struct TypeInfoImpl<math::Vec3<T>, TBoxed>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class TBoxed>
struct TypeInfoImpl<math::Vec4<T>, TBoxed>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class TBoxed>
struct TypeInfoImpl<T, TBoxed, std::enable_if_t<std::is_same_v<T, Mat3f>>>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class TBoxed>
struct TypeInfoImpl<T, TBoxed, std::enable_if_t<std::is_same_v<T, Mat4f>>>
{
    void operator()(TypeInfo& result) const;
};

template <class T, class TBoxed>
struct TypeInfoImpl<Handle<T>, TBoxed>
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

    HYP_API static const TypeInfo& ForClass(const Class* cls);

    /*! \brief Not added to cache, needs to be deleted by caller */
    HYP_API static TypeInfo* ForDynamicClass(const Class* cls);

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
            result.name = CreateNameFromStaticString(HashedName<TypeNameHelper<NormalizedT>::value>());
            result.size = uint16(sizeof(NormalizedT));
            result.alignment = uint16(alignof(NormalizedT));
            result.flags = TypeInfoFlags::NONE;

            if constexpr (std::is_class_v<NormalizedT>)
            {
                result.flags |= TypeInfoFlags::CLASS_TYPE;
            }

            if constexpr (is_pod_type_v<NormalizedT>)
            {
                result.flags |= TypeInfoFlags::POD_TYPE;
            }

            if constexpr (std::is_enum_v<NormalizedT> || IsEnumFlagsV<NormalizedT> || EnumFlagsDecl<NormalizedT>::IsEnumFlags)
            {
                result.flags |= TypeInfoFlags::ENUM_TYPE;

                result.extendedInfo.handler = nullptr;

                if constexpr (IsEnumFlagsV<NormalizedT>)
                {
                    using EnumType = typename NormalizedT::EnumType;

                    // for EnumFlags<T>, we base it off of T enum
                    result.id = TypeId::ForType<EnumType>();
                    result.name = CreateNameFromStaticString(HashedName<TypeNameHelper<EnumType>::value>());
                    result.flags |= TypeInfoFlags::ENUM_FLAGS_TYPE;

                    result.extendedInfo.data.typeInfo = &ForType<typename std::underlying_type_t<EnumType>>();
                    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
                }
                else
                {
                    using EnumType = NormalizedT;

                    // dont need to set id/name here, already set above

                    if constexpr (EnumFlagsDecl<EnumType>::IsEnumFlags)
                    {
                        result.flags |= TypeInfoFlags::ENUM_FLAGS_TYPE;
                    }

                    result.extendedInfo.data.typeInfo = &ForType<typename std::underlying_type_t<EnumType>>();
                    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;
                }
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

            if constexpr (implementation_exists_v<TypeInfoImpl<NormalizedT, BoxedValue>>)
            {
                TypeInfoImpl<NormalizedT, BoxedValue>()(result);
            }

            ValueStorage<Mutex::Guard> guardStorage;

            TypeInfo* pTypeInfo = TypeInfo_Alloc(
                typeId,
                uint16(sizeof(NormalizedT)),
                uint16(alignof(NormalizedT)),
                guardStorage.GetPointer());

            HYP_CORE_ASSERT(pTypeInfo != nullptr);

            new (pTypeInfo) TypeInfo(std::move(result));

            guardStorage.GetPointer()->~TLockGuard();

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

    HYP_FORCE_INLINE const Class* GetClass() const
    {
        // For Handle<T> -- we use the class of T
        if (IsHandleType())
        {
            HYP_CORE_ASSERT(extendedInfo.data.typeInfo != nullptr);

            return Hyperion::GetClass(extendedInfo.data.typeInfo->id);
        }

        return Hyperion::GetClass(id);
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

    HYP_FORCE_INLINE bool IsTupleType() const
    {
        return flags & TypeInfoFlags::TUPLE_TYPE;
    }

    HYP_FORCE_INLINE bool IsPairType() const
    {
        return flags & TypeInfoFlags::PAIR_TYPE;
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

    HYP_FORCE_INLINE bool IsHandleType() const
    {
        return flags & TypeInfoFlags::HANDLE_TYPE;
    }

    /*! \brief Get element type for Array, String, HashSet, FlatSet, or key type for TMap/FlatMap */
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

    /*! \brief Get value type for TMap/FlatMap (stored in extendedInfo.next->data) */
    HYP_FORCE_INLINE const TypeInfo* GetValueType() const
    {
        if (!extendedInfo.next)
        {
            return nullptr;
        }

        return extendedInfo.next->GetElementType();
    }

    /*! \brief Get key type for TMap/FlatMap (same as GetElementType for these types) */
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

template <class T, class TBoxed>
void TypeInfoImpl<T, TBoxed, std::enable_if_t<std::is_same_v<T, GenericArrayWrapper>>>::operator()(TypeInfo& result) const
{
    // GenericArrayWrapper is a special case since it can hold any array type
    class GenericArrayHandler final : public ITypeInfoArrayHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = TBoxed(T {});

            return true;
        }

        virtual bool GetElementAt(const TBoxed& instance, size_t index, TBoxed& outValue) const override
        {
            T& array = instance.template Get<T>();

            if (index >= array.Size())
            {
                return false;
            }

            return array.GetElementAt(index, outValue);
        }

        virtual bool SetElementAt(const TBoxed& instance, size_t index, TBoxed&& value) const override
        {
            T& array = instance.template Get<T>();
            if (index >= array.Size())
            {
                return false;
            }

            return array.SetElementAt(index, std::move(value));
        }

        virtual size_t GetSize(const TBoxed& instance) const override
        {
            T& array = instance.template Get<T>();
            return array.Size();
        }

        virtual void Resize(const TBoxed& instance, size_t newSize) const override
        {
            T& array = instance.template Get<T>();

            if (!array.Resize(newSize))
            {
                HYP_CORE_ASSERT(false, "Failed to resize GenericArrayWrapper");
            }
        }
    };

    result.flags |= TypeInfoFlags::ARRAY_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<BoxedValue>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    static GenericArrayHandler s_handler {};
    result.extendedInfo.handler = &s_handler;
}

template <class ArrayType, class TBoxed>
void TypeInfoImpl<ArrayType, TBoxed, std::enable_if_t<IsArray<ArrayType>::value>>::operator()(TypeInfo& result) const
{
    class ArrayHandler final : public ITypeInfoArrayHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = TBoxed(ArrayType {});
            return true;
        }

        virtual bool GetElementAt(const TBoxed& instance, size_t index, TBoxed& outValue) const override
        {
            ArrayType& array = instance.template Get<ArrayType>();

            if (index >= array.Size())
            {
                return false;
            }

            outValue = TBoxed(AnyRef(&array[index]));

            return true;
        }

        virtual bool SetElementAt(const TBoxed& instance, size_t index, TBoxed&& value) const override
        {
            ArrayType& array = instance.template Get<ArrayType>();

            if (index >= array.Size())
            {
                return false;
            }

            if constexpr (std::is_same_v<typename ArrayType::ValueType, TBoxed>)
            {
                array[index] = std::move(value);
            }
            else
            {
                array[index] = value.template Get<typename ArrayType::ValueType>();
            }

            return true;
        }

        virtual size_t GetSize(const TBoxed& instance) const override
        {
            ArrayType& array = instance.template Get<ArrayType>();
            return array.Size();
        }

        virtual void Resize(const TBoxed& instance, size_t newSize) const override
        {
            ArrayType& array = instance.template Get<ArrayType>();
            array.Resize(newSize);
        }
    };

    result.flags |= TypeInfoFlags::ARRAY_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<typename ArrayType::ValueType>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    static ArrayHandler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <class T, size_t Size, class TBoxed>
void TypeInfoImpl<containers::FixedArray<T, Size>, TBoxed>::operator()(TypeInfo& result) const
{
    using FixedArrayType = FixedArray<T, Size>;

    class FixedArrayHandler final : public ITypeInfoArrayHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = TBoxed(FixedArrayType {});
            return true;
        }

        virtual bool GetElementAt(const TBoxed& instance, size_t index, TBoxed& outValue) const override
        {
            FixedArrayType& array = instance.template Get<FixedArrayType>();

            if (index >= array.Size())
            {
                return false;
            }

            outValue = BoxedValue(AnyRef(&array[index]));

            return true;
        }

        virtual bool SetElementAt(const TBoxed& instance, size_t index, TBoxed&& value) const override
        {
            FixedArrayType& array = instance.template Get<FixedArrayType>();

            if (index >= array.Size())
            {
                return false;
            }

            if constexpr (std::is_same_v<T, TBoxed>)
            {
                array[index] = std::move(value);
            }
            else
            {
                array[index] = value.template Get<T>();
            }

            return true;
        }

        virtual size_t GetSize(const TBoxed& instance) const override
        {
            FixedArrayType& array = instance.template Get<FixedArrayType>();
            return array.Size();
        }

        virtual void Resize(const TBoxed& instance, size_t newSize) const override
        {
            // FixedArray has a fixed size, so resizing is not supported
            // This operation is a no-op
        }
    };

    result.flags |= TypeInfoFlags::ARRAY_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<T>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    static FixedArrayHandler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <class T, class AllocatorType, class TBoxed>
void TypeInfoImpl<containers::LinkedList<T, AllocatorType>, TBoxed>::operator()(TypeInfo& result) const
{
    using ListType = containers::LinkedList<T, AllocatorType>;

    class LinkedListHandler final : public ITypeInfoLinkedListHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = TBoxed(ListType {});
            return true;
        }

        virtual AnyRef GetElementAt(const TBoxed& instance, size_t index) const override
        {
            ListType& list = instance.template Get<ListType>();
            auto it = list.Begin() + index;
            return AnyRef(&(*it));
        }

        virtual void SetElementAt(const TBoxed& instance, size_t index, const TBoxed& value) const override
        {
            ListType& list = instance.template Get<ListType>();
            auto it = list.Begin() + index;
            *it = value.template Get<T>();
        }

        virtual size_t GetSize(const TBoxed& instance) const override
        {
            ListType& list = instance.template Get<ListType>();
            return list.Size();
        }

        virtual void Resize(const TBoxed& instance, size_t newSize) const override
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

    static LinkedListHandler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <int TStringType, class TBoxed>
void TypeInfoImpl<containers::String<TStringType>, TBoxed>::operator()(TypeInfo& result) const
{
    using StringType = containers::String<TStringType>;

    class StringHandler final : public ITypeInfoStringHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = TBoxed(StringType {});
            return true;
        }

        virtual String GetValue(const TBoxed& instance) const override
        {
            StringType& string = instance.template Get<StringType>();
            return string.ToUtf8();
        }

        virtual void SetValue(const TBoxed& instance, const UTF8StringView& str) const override
        {

            StringType& string = instance.template Get<StringType>();
            string = str;
        }
    };

    result.flags |= TypeInfoFlags::STRING_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<typename StringType::CharType>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    static StringHandler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <class T, class TBoxed>
void TypeInfoImpl<T, TBoxed, std::enable_if_t<std::is_same_v<T, filesystem::FilePath>>>::operator()(TypeInfo& result) const
{
    // delegate to String impl since FilePath is just a wrapper around String
    TypeInfoImpl<typename T::Base, TBoxed>()(result);
}

template <class Key, class Value, class NodeAllocatorType, class TBoxed>
void TypeInfoImpl<containers::TMap<Key, Value, NodeAllocatorType>, TBoxed>::operator()(TypeInfo& result) const
{
    using MapType = containers::TMap<Key, Value, NodeAllocatorType>;

    class HashMapHandler final : public ITypeInfoMapHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = BoxedValue(MapType {});
            return true;
        }

        virtual AnyRef GetValueAt(const TBoxed& instance, const TBoxed& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            const Key& k = key.template Get<Key>();

            auto it = map.Find(k);
            if (it != map.End())
            {
                return AnyRef(&it->second);
            }

            return AnyRef();
        }

        virtual void SetValueAt(const TBoxed& instance, const TBoxed& key, const TBoxed& value) const override
        {
            MapType& map = instance.template Get<MapType>();
            const Key& k = key.template Get<Key>();
            const Value& v = value.template Get<Value>();

            map[k] = v;
        }

        virtual bool ContainsKey(const TBoxed& instance, const TBoxed& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            const Key& k = key.template Get<Key>();

            return map.Find(k) != map.End();
        }

        virtual bool RemoveKey(const TBoxed& instance, const TBoxed& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            const Key& k = key.template Get<Key>();

            auto it = map.Find(k);
            if (it != map.End())
            {
                map.Erase(it);
                return true;
            }

            return false;
        }

        virtual size_t GetSize(const TBoxed& instance) const override
        {
            MapType& map = instance.template Get<MapType>();

            return map.Size();
        }

        virtual ITypeInfoIterator* CreateIterator(const TBoxed& instance) const override
        {
            class HashMapIterator final : public ITypeInfoIterator
            {
                MapType* m_map;
                typename MapType::Iterator m_current;
                typename MapType::Iterator m_end;

            public:
                HashMapIterator(MapType* map)
                    : m_map(map),
                      m_current(map->Begin()),
                      m_end(map->End())
                {
                }

                virtual ~HashMapIterator() = default;

                virtual bool HasNext() const override
                {
                    return m_current != m_end;
                }

                virtual bool Next() override
                {
                    if (m_current != m_end)
                    {
                        ++m_current;
                        return true;
                    }
                    return false;
                }

                virtual AnyRef GetCurrent() const override
                {
                    if (m_current != m_end)
                    {
                        return AnyRef(&m_current->second);
                    }
                    return AnyRef();
                }

                virtual AnyRef GetKey() const override
                {
                    if (m_current != m_end)
                    {
                        return AnyRef(&m_current->first);
                    }
                    return AnyRef();
                }

                virtual void Reset() override
                {
                    m_current = m_map->Begin();
                }

                virtual ITypeInfoIterator* Clone() const override
                {
                    return new HashMapIterator(m_map);
                }
            };

            MapType& map = instance.template Get<MapType>();
            return new HashMapIterator(&map);
        }
    };

    result.flags |= TypeInfoFlags::MAP_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<Key>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    result.extendedInfo.next = new TypeInfoEx();
    result.extendedInfo.next->data.typeInfo = &TypeInfo::ForType<Value>();
    result.extendedInfo.next->dataType = TypeInfoEx::DT_TYPE_INFO;

    static HashMapHandler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <class Key, class Value, class TBoxed>
void TypeInfoImpl<containers::FlatMap<Key, Value>, TBoxed>::operator()(TypeInfo& result) const
{
    using MapType = containers::FlatMap<Key, Value>;

    class FlatMapHandler final : public ITypeInfoMapHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = BoxedValue(MapType {});
            return true;
        }

        virtual AnyRef GetValueAt(const TBoxed& instance, const TBoxed& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            const Key& k = key.template Get<Key>();

            auto it = map.Find(k);
            if (it != map.End())
            {
                return AnyRef(&it->second);
            }

            return AnyRef();
        }

        virtual void SetValueAt(const TBoxed& instance, const TBoxed& key, const TBoxed& value) const override
        {
            MapType& map = instance.template Get<MapType>();
            const Key& k = key.template Get<Key>();
            const Value& v = value.template Get<Value>();

            map[k] = v;
        }

        virtual bool ContainsKey(const TBoxed& instance, const TBoxed& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            const Key& k = key.template Get<Key>();

            return map.Find(k) != map.End();
        }

        virtual bool RemoveKey(const TBoxed& instance, const TBoxed& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            const Key& k = key.template Get<Key>();

            auto it = map.Find(k);
            if (it != map.End())
            {
                map.Erase(it);
                return true;
            }

            return false;
        }

        virtual size_t GetSize(const TBoxed& instance) const override
        {
            MapType& map = instance.template Get<MapType>();

            return map.Size();
        }

        virtual ITypeInfoIterator* CreateIterator(const TBoxed& instance) const override
        {
            class FlatMapIterator final : public ITypeInfoIterator
            {
                MapType* m_map;
                typename MapType::Iterator m_current;
                typename MapType::Iterator m_end;

            public:
                FlatMapIterator(MapType* map)
                    : m_map(map),
                      m_current(map->Begin()),
                      m_end(map->End())
                {
                }

                virtual ~FlatMapIterator() = default;

                virtual bool HasNext() const override
                {
                    return m_current != m_end;
                }

                virtual bool Next() override
                {
                    if (m_current != m_end)
                    {
                        ++m_current;
                        return true;
                    }
                    return false;
                }

                virtual AnyRef GetCurrent() const override
                {
                    if (m_current != m_end)
                    {
                        return AnyRef(&m_current->second);
                    }
                    return AnyRef();
                }

                virtual AnyRef GetKey() const override
                {
                    if (m_current != m_end)
                    {
                        return AnyRef(&m_current->first);
                    }
                    return AnyRef();
                }

                virtual void Reset() override
                {
                    m_current = m_map->Begin();
                }

                virtual ITypeInfoIterator* Clone() const override
                {
                    return new FlatMapIterator(m_map);
                }
            };

            MapType& map = instance.template Get<MapType>();
            return new FlatMapIterator(&map);
        }
    };

    result.flags |= TypeInfoFlags::MAP_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<Key>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    result.extendedInfo.next = new TypeInfoEx();
    result.extendedInfo.next->data.typeInfo = &TypeInfo::ForType<Value>();
    result.extendedInfo.next->dataType = TypeInfoEx::DT_TYPE_INFO;

    static FlatMapHandler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <class Key, class Value, class TBoxed>
void TypeInfoImpl<containers::ArrayMap<Key, Value>, TBoxed>::operator()(TypeInfo& result) const
{
    using MapType = containers::ArrayMap<Key, Value>;

    class ArrayMapHandler final : public ITypeInfoMapHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = BoxedValue(MapType {});
            return true;
        }

        virtual AnyRef GetValueAt(const TBoxed& instance, const TBoxed& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            const Key& k = key.template Get<Key>();

            auto it = map.Find(k);
            if (it != map.End())
            {
                return AnyRef(&it->second);
            }

            return AnyRef();
        }

        virtual void SetValueAt(const TBoxed& instance, const TBoxed& key, const TBoxed& value) const override
        {
            MapType& map = instance.template Get<MapType>();
            const Key& k = key.template Get<Key>();
            const Value& v = value.template Get<Value>();

            map[k] = v;
        }

        virtual bool ContainsKey(const TBoxed& instance, const TBoxed& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            const Key& k = key.template Get<Key>();

            return map.Find(k) != map.End();
        }

        virtual bool RemoveKey(const TBoxed& instance, const TBoxed& key) const override
        {
            MapType& map = instance.template Get<MapType>();
            const Key& k = key.template Get<Key>();

            auto it = map.Find(k);
            if (it != map.End())
            {
                map.Erase(it);
                return true;
            }

            return false;
        }

        virtual size_t GetSize(const TBoxed& instance) const override
        {
            MapType& map = instance.template Get<MapType>();

            return map.Size();
        }

        virtual ITypeInfoIterator* CreateIterator(const TBoxed& instance) const override
        {
            class ArrayMapIterator final : public ITypeInfoIterator
            {
                MapType* m_map;
                typename MapType::Iterator m_current;
                typename MapType::Iterator m_end;

            public:
                ArrayMapIterator(MapType* map)
                    : m_map(map),
                      m_current(map->Begin()),
                      m_end(map->End())
                {
                }

                virtual ~ArrayMapIterator() = default;

                virtual bool HasNext() const override
                {
                    return m_current != m_end;
                }

                virtual bool Next() override
                {
                    if (m_current != m_end)
                    {
                        ++m_current;
                        return true;
                    }
                    return false;
                }

                virtual AnyRef GetCurrent() const override
                {
                    if (m_current != m_end)
                    {
                        return AnyRef(&m_current->second);
                    }
                    return AnyRef();
                }

                virtual AnyRef GetKey() const override
                {
                    if (m_current != m_end)
                    {
                        return AnyRef(&m_current->first);
                    }
                    return AnyRef();
                }

                virtual void Reset() override
                {
                    m_current = m_map->Begin();
                }

                virtual ITypeInfoIterator* Clone() const override
                {
                    return new ArrayMapIterator(m_map);
                }
            };

            MapType& map = instance.template Get<MapType>();
            return new ArrayMapIterator(&map);
        }
    };

    result.flags |= TypeInfoFlags::MAP_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<Key>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    result.extendedInfo.next = new TypeInfoEx();
    result.extendedInfo.next->data.typeInfo = &TypeInfo::ForType<Value>();
    result.extendedInfo.next->dataType = TypeInfoEx::DT_TYPE_INFO;

    static ArrayMapHandler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <class Value, class TBoxed>
void TypeInfoImpl<containers::FlatSet<Value>, TBoxed>::operator()(TypeInfo& result) const
{
    using SetType = containers::FlatSet<Value>;

    class FlatSetHandler final : public ITypeInfoSetHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = TBoxed(SetType {});
            return true;
        }

        virtual bool Contains(const TBoxed& instance, const TBoxed& value) const override
        {
            SetType& set = instance.template Get<SetType>();
            return set.Contains(value.template Get<Value>());
        }

        virtual bool Insert(const TBoxed& instance, const TBoxed& value) const override
        {
            SetType& set = instance.template Get<SetType>();
            return set.Insert(value.template Get<Value>()).second;
        }

        virtual bool Remove(const TBoxed& instance, const TBoxed& value) const override
        {
            SetType& set = instance.template Get<SetType>();
            return set.Erase(value.template Get<Value>());
        }

        virtual size_t GetSize(const TBoxed& instance) const override
        {
            SetType& set = instance.template Get<SetType>();
            return set.Size();
        }

        virtual ITypeInfoIterator* CreateIterator(const TBoxed& instance) const override
        {
            class FlatSetIterator final : public ITypeInfoIterator
            {
                SetType* m_set;
                typename SetType::Iterator m_current;
                typename SetType::Iterator m_end;

            public:
                FlatSetIterator(SetType* set)
                    : m_set(set),
                      m_current(set->Begin()),
                      m_end(set->End())
                {
                }

                virtual ~FlatSetIterator() = default;

                virtual bool HasNext() const override
                {
                    return m_current != m_end;
                }

                virtual bool Next() override
                {
                    if (m_current != m_end)
                    {
                        ++m_current;
                        return true;
                    }
                    return false;
                }

                virtual AnyRef GetCurrent() const override
                {
                    if (m_current != m_end)
                    {
                        return AnyRef(&(*m_current));
                    }
                    return AnyRef();
                }

                virtual AnyRef GetKey() const override
                {
                    // Sets don't have keys
                    return AnyRef();
                }

                virtual void Reset() override
                {
                    m_current = m_set->Begin();
                }

                virtual ITypeInfoIterator* Clone() const override
                {
                    return new FlatSetIterator(m_set);
                }
            };

            SetType& set = instance.template Get<SetType>();
            return new FlatSetIterator(&set);
        }
    };

    result.flags |= TypeInfoFlags::SET_TYPE;
    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<Value>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    static FlatSetHandler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <class Value, class NodeAllocatorType, class TBoxed>
void TypeInfoImpl<containers::TSet<Value, NodeAllocatorType>, TBoxed>::operator()(TypeInfo& result) const
{
    using SetType = containers::TSet<Value, NodeAllocatorType>;

    class HashSetHandler final : public ITypeInfoSetHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = TBoxed(SetType {});
            return true;
        }

        virtual bool Contains(const TBoxed& instance, const TBoxed& value) const override
        {
            SetType& set = instance.template Get<SetType>();
            return set.Contains(value.template Get<Value>());
        }

        virtual bool Insert(const TBoxed& instance, const TBoxed& value) const override
        {
            SetType& set = instance.template Get<SetType>();
            return set.Insert(value.template Get<Value>()).second;
        }

        virtual bool Remove(const TBoxed& instance, const TBoxed& value) const override
        {
            SetType& set = instance.template Get<SetType>();
            return set.Erase(value.template Get<Value>());
        }

        virtual size_t GetSize(const TBoxed& instance) const override
        {
            SetType& set = instance.template Get<SetType>();
            return set.Size();
        }

        virtual ITypeInfoIterator* CreateIterator(const TBoxed& instance) const override
        {
            class HashSetIterator final : public ITypeInfoIterator
            {
                SetType* m_set;
                typename SetType::Iterator m_current;
                typename SetType::Iterator m_end;

            public:
                HashSetIterator(SetType* set)
                    : m_set(set),
                      m_current(set->Begin()),
                      m_end(set->End())
                {
                }

                virtual ~HashSetIterator() = default;

                virtual bool HasNext() const override
                {
                    return m_current != m_end;
                }

                virtual bool Next() override
                {
                    if (m_current != m_end)
                    {
                        ++m_current;
                        return true;
                    }
                    return false;
                }

                virtual AnyRef GetCurrent() const override
                {
                    if (m_current != m_end)
                    {
                        return AnyRef(&(*m_current));
                    }
                    return AnyRef();
                }

                virtual AnyRef GetKey() const override
                {
                    // Sets don't have keys
                    return AnyRef();
                }

                virtual void Reset() override
                {
                    m_current = m_set->Begin();
                }

                virtual ITypeInfoIterator* Clone() const override
                {
                    return new HashSetIterator(m_set);
                }
            };

            SetType& set = instance.template Get<SetType>();
            return new HashSetIterator(&set);
        }
    };

    result.flags |= TypeInfoFlags::SET_TYPE;
    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<Value>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    static HashSetHandler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <class... Types, class TBoxed>
void TypeInfoImpl<utilities::Variant<Types...>, TBoxed>::operator()(TypeInfo& result) const
{
    using VariantType = utilities::Variant<Types...>;

    result.flags |= TypeInfoFlags::VARIANT_TYPE;

    constexpr size_t VariantSize = sizeof...(Types);

    if constexpr (VariantSize > 0)
    {
        auto altArray = BuildVariantTypeArray<utilities::Variant<Types...>>(std::make_index_sequence<VariantSize> {});

        // Convert the fixed array into a linked list of TypeInfoEx nodes
        TypeInfoEx* head = nullptr;
        TypeInfoEx* current = nullptr;

        for (size_t i = 0; i < altArray.Size(); ++i)
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

    class VariantHandler final : public ITypeInfoVariantHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = BoxedValue(VariantType {});
            return true;
        }

        virtual int GetNumTypes() const override
        {
            return int(VariantSize);
        }

        virtual const TypeInfo* GetTypeInfoAtIndex(int typeIndex) const override
        {
            // typeinfos stores first elem as nullptr to match typeids
            return VariantType::typeInfos[typeIndex + 1];
        }

        virtual int GetCurrentTypeIndex(const TBoxed& instance) const override
        {
            VariantType& variant = instance.template Get<VariantType>();
            return variant.GetTypeIndex();
        }

        virtual AnyRef GetValue(const TBoxed& instance) const override
        {
            VariantType& variant = instance.template Get<VariantType>();
            return variant.ToRef();
        }

        virtual bool SetValue(const TBoxed& instance, const TBoxed& value) const override
        {
            VariantType& variant = instance.template Get<VariantType>();
            bool isSet = false;

            // initial pass, strict type match
            StaticForEach<Tuple<Types...>>([&]<class T>(TypeWrapper<T>)
                {
                    if (isSet)
                    {
                        return;
                    }

                    if (value.template Is<T>(/* strict */ true))
                    {
                        variant.template Set<T>(value.template Get<T>());
                        isSet = true;
                    }
                });

            if (isSet)
                return true;

            // second pass, for float/integral types.
            // if the initial 'strict' pass didn't find any type to match against the given value,
            // we do another pass to allow `double`, for example,
            // to be cast down to float, rather than settling on an integral type if it comes first in the type list.
            bool isValueIntegral = value.GetTypeInfo()->IsIntegralType();
            bool isValueFloat = value.GetTypeInfo()->IsFloatType();

            if (isValueIntegral || isValueFloat)
            {
                if (MathUtil::Fract(value.template Get<double>()) == 0)
                {
                    isValueIntegral = true;
                    isValueFloat = false;
                }

                StaticForEach<Tuple<Types...>>([&]<class T>(TypeWrapper<T>)
                    {
                        if (isSet)
                        {
                            return;
                        }

                        if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>)
                        {
                            if (isValueIntegral == std::is_integral_v<T>)
                            {
                                variant.template Set<T>(value.template Get<T>());
                                isSet = true;
                            }
                        }
                    });

                if (isSet)
                    return true;
            }

            // final pass: non-strict match.
            // allows fundamental types to be used interchangably; the first compatible type that is encountered
            // will be used.
            StaticForEach<Tuple<Types...>>([&]<class T>(TypeWrapper<T>)
                {
                    if (isSet)
                    {
                        return;
                    }

                    if (value.template Is<T>(/* strict */ false))
                    {
                        variant.template Set<T>(value.template Get<T>());
                        isSet = true;
                    }
                });

            if (isSet)
                return true;

            // Type not found in variant types; reset to default
            variant = VariantType {};

            return false;
        }
    };

    static VariantHandler s_handler;
    result.extendedInfo.handler = &s_handler;
}

/// Tuple implementation
/// Avert your eyes or your sanity shall belong to me!!!

template <class... Types, class TBoxed>
void TypeInfoImpl<utilities::Tuple<Types...>, TBoxed>::operator()(TypeInfo& result) const
{
    using TupleType = utilities::Tuple<Types...>;

    result.flags |= TypeInfoFlags::TUPLE_TYPE;

    constexpr size_t TupleSize = sizeof...(Types);

    // Store each element's TypeInfo as a linked list of TypeInfoEx nodes
    if constexpr (TupleSize > 0)
    {
        TypeInfoEx* head = nullptr;
        TypeInfoEx* current = nullptr;

        auto BuildNodes = [&]<size_t... Indices>(std::index_sequence<Indices...>)
        {
            ((
                [&]() {
                    TypeInfoEx* node = new TypeInfoEx();
                    node->data.typeInfo = &TypeInfo::ForType<typename utilities::TupleElement<Indices, Types...>::Type>();
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
                }()
            ), ...);
        };

        BuildNodes(std::make_index_sequence<TupleSize> {});

        result.extendedInfo.next = head;
    }

    class TupleHandler final : public ITypeInfoTupleHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = BoxedValue(TupleType {});
            return true;
        }

        virtual int GetNumElements() const override
        {
            return int(TupleSize);
        }

        virtual const TypeInfo* GetElementTypeInfoAtIndex(int index) const override
        {
            if (index < 0 || size_t(index) >= TupleSize)
            {
                return nullptr;
            }

            const TypeInfo* result = nullptr;
            int i = 0;

            WithTupleElement(TupleType {}, index, [&](auto&& elem)
                {
                    result = &TypeInfo::ForType<NormalizedType<decltype(elem)>>();
                });

            return result;
        }

        virtual AnyRef GetElement(const TBoxed& instance, int index) const override
        {
            if (index < 0 || size_t(index) >= TupleSize)
            {
                return AnyRef();
            }

            AnyRef result;

            // gross again
            int i = 0;
            [&]<size_t... Indices>(std::index_sequence<Indices...>)
            {
                ((
                    [&]() {
                        if (i == index)
                        {
                            TupleType& tup = instance.template Get<TupleType>();
                            result = AnyRef(&tup.template GetElement<Indices>());
                        }
                        ++i;
                    }()
                ), ...);
            }(std::make_index_sequence<TupleSize> {});

            return result;
        }

        virtual bool SetElement(const TBoxed& instance, int index, const TBoxed& value) const override
        {
            if (index < 0 || size_t(index) >= TupleSize)
            {
                return false;
            }

            bool isSet = false;
            int i = 0;

            // yep still gross
            [&]<size_t... Indices>(std::index_sequence<Indices...>)
            {
                ((
                    [&]() {
                        if (i == index && !isSet)
                        {
                            using ElemType = typename utilities::TupleElement<Indices, Types...>::Type;
                            TupleType& tup = instance.template Get<TupleType>();
                            tup.template GetElement<Indices>() = value.template Get<ElemType>();
                            isSet = true;
                        }
                        ++i;
                    }()
                ), ...);
            }(std::make_index_sequence<TupleSize> {});

            return isSet;
        }
    };

    static TupleHandler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <class First, class Second, class T1, class T2, class TBoxed>
void TypeInfoImpl<utilities::detail::Pair<First, Second, T1, T2>, TBoxed>::operator()(TypeInfo& result) const
{
    using PairType = utilities::detail::Pair<First, Second, T1, T2>;

    result.flags |= TypeInfoFlags::PAIR_TYPE;

    // Store first type in extendedInfo, second type in extendedInfo.next
    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<First>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    result.extendedInfo.next = new TypeInfoEx();
    result.extendedInfo.next->data.typeInfo = &TypeInfo::ForType<Second>();
    result.extendedInfo.next->dataType = TypeInfoEx::DT_TYPE_INFO;

    class PairHandler final : public ITypeInfoPairHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = BoxedValue(PairType {});
            return true;
        }

        virtual const TypeInfo* GetFirstTypeInfo() const override
        {
            return &TypeInfo::ForType<First>();
        }

        virtual const TypeInfo* GetSecondTypeInfo() const override
        {
            return &TypeInfo::ForType<Second>();
        }

        virtual void GetFirst(const TBoxed& instance, TBoxed& outValue) const override
        {
            PairType& pair = instance.template Get<PairType>();
            outValue = TBoxed(pair.first);
        }

        virtual void SetFirst(const TBoxed& instance, const TBoxed& value) const override
        {
            PairType& pair = instance.template Get<PairType>();
            pair.first = value.template Get<First>();
        }

        virtual void GetSecond(const TBoxed& instance, TBoxed& outValue) const override
        {
            PairType& pair = instance.template Get<PairType>();
            outValue = BoxedValue(pair.second);
        }

        virtual void SetSecond(const TBoxed& instance, const TBoxed& value) const override
        {
            PairType& pair = instance.template Get<PairType>();
            pair.second = value.template Get<Second>();
        }
    };

    static PairHandler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <class T, class TBoxed>
void TypeInfoImpl<math::Vec2<T>, TBoxed>::operator()(TypeInfo& result) const
{
    using Vec2Type = math::Vec2<T>;

    result.flags |= TypeInfoFlags::VEC2_TYPE;
    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<T>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    // set handler
    class Vec2Handler final : public ITypeInfoVectorHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = BoxedValue(Vec2Type {});
            return true;
        }

        virtual int GetNumComponents() const override
        {
            return 2;
        }

        virtual AnyRef GetComponent(const TBoxed& instance, int index) const override
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

        virtual void SetComponent(const TBoxed& instance, int index, const TBoxed& value) const override
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

    static Vec2Handler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <class T, class TBoxed>
void TypeInfoImpl<math::Vec3<T>, TBoxed>::operator()(TypeInfo& result) const
{
    using Vec3Type = math::Vec3<T>;

    result.flags |= TypeInfoFlags::VEC3_TYPE;
    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<T>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    // set handler
    class Vec3Handler final : public ITypeInfoVectorHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = BoxedValue(Vec3Type {});
            return true;
        }

        virtual int GetNumComponents() const override
        {
            return 3;
        }

        virtual AnyRef GetComponent(const TBoxed& instance, int index) const override
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

        virtual void SetComponent(const TBoxed& instance, int index, const TBoxed& value) const override
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

    static Vec3Handler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <class T, class TBoxed>
void TypeInfoImpl<math::Vec4<T>, TBoxed>::operator()(TypeInfo& result) const
{
    using Vec4Type = math::Vec4<T>;

    result.flags |= TypeInfoFlags::VEC4_TYPE;
    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<T>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    // set handler
    class Vec4Handler final : public ITypeInfoVectorHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = BoxedValue(Vec4Type {});
            return true;
        }

        virtual int GetNumComponents() const override
        {
            return 4;
        }

        virtual AnyRef GetComponent(const TBoxed& instance, int index) const override
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

        virtual void SetComponent(const TBoxed& instance, int index, const TBoxed& value) const override
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

    static Vec4Handler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <class T, class TBoxed>
void TypeInfoImpl<T, TBoxed, std::enable_if_t<std::is_same_v<T, Mat3f>>>::operator()(TypeInfo& result) const
{
    using MatrixType = T;

    result.flags |= TypeInfoFlags::MATRIX_TYPE;
    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<float>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    // set handler
    class Mat3fHandler final : public ITypeInfoMatrixHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = BoxedValue(MatrixType {});
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

        virtual AnyRef GetElement(const TBoxed& instance, int row, int column) const override
        {
            MatrixType& mat = instance.template Get<MatrixType>();
            if (row < 0 || row >= 3 || column < 0 || column >= 3)
            {
                return AnyRef();
            }

            return AnyRef(&mat[row][column]);
        }

        virtual void SetElement(const TBoxed& instance, int row, int column, const TBoxed& value) const override
        {
            MatrixType& mat = instance.template Get<MatrixType>();
            if (row < 0 || row >= 3 || column < 0 || column >= 3)
            {
                return;
            }

            mat[row][column] = value.template Get<float>();
        }
    };

    static Mat3fHandler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <class T, class TBoxed>
void TypeInfoImpl<T, TBoxed, std::enable_if_t<std::is_same_v<T, Mat4f>>>::operator()(TypeInfo& result) const
{
    using MatrixType = T;

    result.flags |= TypeInfoFlags::MATRIX_TYPE;
    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<float>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    // set handler
    class Mat4fHandler final : public ITypeInfoMatrixHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = BoxedValue(MatrixType {});
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

        virtual AnyRef GetElement(const TBoxed& instance, int row, int column) const override
        {
            MatrixType& mat = instance.template Get<MatrixType>();
            if (row < 0 || row >= 4 || column < 0 || column >= 4)
            {
                return AnyRef();
            }

            return AnyRef(&mat[row][column]);
        }

        virtual void SetElement(const TBoxed& instance, int row, int column, const TBoxed& value) const override
        {
            MatrixType& mat = instance.template Get<MatrixType>();
            if (row < 0 || row >= 4 || column < 0 || column >= 4)
            {
                return;
            }

            mat[row][column] = value.template Get<float>();
        }
    };

    static Mat4fHandler s_handler;
    result.extendedInfo.handler = &s_handler;
}

template <class T, class TBoxed>
void TypeInfoImpl<Handle<T>, TBoxed>::operator()(TypeInfo& result) const
{
    using HandleType = Handle<T>;

    // we set CLASS_TYPE because for Handle<T>, T must be a subclass of ObjectBase
    result.flags |= TypeInfoFlags::HANDLE_TYPE | TypeInfoFlags::CLASS_TYPE;

    result.extendedInfo.data.typeInfo = &TypeInfo::ForType<T>();
    result.extendedInfo.dataType = TypeInfoEx::DT_TYPE_INFO;

    class HandleTHandler final : public ITypeInfoHandleHandler
    {
    public:
        virtual bool CreateInstance(TBoxed& outInstance) const override
        {
            outInstance = BoxedValue(HandleType {});
            return true;
        }

        virtual AnyRef GetObject(const TBoxed& instance) const override
        {
            return instance.ToRef();
        }

        virtual void SetObject(TBoxed& instance, const TBoxed& value) const override
        {
            instance = value;
        }
    };

    static HandleTHandler s_handler;
    result.extendedInfo.handler = &s_handler;
}

/// Wrapper functions for forward decls

inline const TypeInfo& TypeInfo_Void()
{
    return TypeInfo::Void();
}

inline const TypeInfo& TypeInfo_ForClass(const Class* cls)
{
    return TypeInfo::ForClass(cls);
}

inline const TypeId& TypeInfo_GetId(const TypeInfo& typeInfo)
{
    return typeInfo.id;
}

inline const Name& TypeInfo_GetName(const TypeInfo& typeInfo)
{
    return typeInfo.name;
}

inline const Class* TypeInfo_GetClass(const TypeInfo& typeInfo)
{
    return typeInfo.GetClass();
}

inline size_t TypeInfo_GetSize(const TypeInfo& typeInfo)
{
    return typeInfo.size;
}

template <class T>
const TypeInfo& TypeOf()
{
    return TypeInfo::ForType<T>();
}

// Definition of BuildVariantTypeArray placed after TypeInfo is defined.
template <class NormalizedT, std::size_t... Indices>
inline FixedArray<const TypeInfo*, sizeof...(Indices)> BuildVariantTypeArray(std::index_sequence<Indices...>)
{
    FixedArray<const TypeInfo*, sizeof...(Indices)> res;
    ((res[Indices] = &TypeOf<typename IsVariant<NormalizedT>::template TypeAtIndex<Indices>>()), ...);
    return res;
}

} // namespace utilities

using utilities::ITypeInfoHandler;
using utilities::ITypeInfoIterator;

using utilities::ITypeInfoArrayHandler;
using utilities::ITypeInfoLinkedListHandler;
using utilities::ITypeInfoMapHandler;
using utilities::ITypeInfoMatrixHandler;
using utilities::ITypeInfoSetHandler;
using utilities::ITypeInfoStringHandler;
using utilities::ITypeInfoVariantHandler;
using utilities::ITypeInfoVectorHandler;
using utilities::ITypeInfoTupleHandler;
using utilities::ITypeInfoPairHandler;

using utilities::TypeInfo_ForClass;
using utilities::TypeInfo_Void;
using utilities::TypeOf;

using utilities::TypeInfo;
using utilities::TypeInfo_Initialize;
using utilities::TypeInfo_Shutdown;

} // namespace Hyperion
