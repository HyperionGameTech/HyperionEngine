#pragma once

#include <core/memory/RefCountedPtr.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/HashSet.hpp>
#include <core/containers/String.hpp>
#include <core/Types.hpp>

namespace hyperion {

#define HYP_SYMBOL_TYPE_DANGLING_PTR_DEBUG 1

#if defined(HYP_SYMBOL_TYPE_DANGLING_PTR_DEBUG) && HYP_SYMBOL_TYPE_DANGLING_PTR_DEBUG
extern void CheckDanglingSymbolTypes();
#endif

// forward declaration
class SymbolType;
class AstExpression;
class AstArgument;
struct Scope;
class CompilationUnit;

class SymbolTypeMember final
{
public:
    SymbolTypeMember()
        : m_type(nullptr)
    {
    }

    SymbolTypeMember(const String& name, SymbolType* type, const RC<AstExpression>& expr = nullptr)
        : m_name(name),
          m_type(type),
          m_expr(expr)
    {
    }

    HYP_FORCE_INLINE const String& GetName() const
    {
        return m_name;
    }

    HYP_FORCE_INLINE void SetName(const String& name)
    {
        m_name = name;
    }

    HYP_FORCE_INLINE SymbolType* GetType()
    {
        return m_type;
    }

    HYP_FORCE_INLINE const SymbolType* GetType() const
    {
        return m_type;
    }

    HYP_FORCE_INLINE void SetType(SymbolType* type)
    {
        m_type = type;
    }

    HYP_FORCE_INLINE const RC<AstExpression>& GetExpr() const
    {
        return m_expr;
    }

    HYP_FORCE_INLINE void SetExpr(const RC<AstExpression>& expr)
    {
        m_expr = expr;
    }

    HYP_FORCE_INLINE void SetExpr(RC<AstExpression>&& expr)
    {
        m_expr = std::move(expr);
    }

private:
    String m_name;
    SymbolType* m_type;
    RC<AstExpression> m_expr;
};

enum SymbolTypeClass : uint8
{
    TYPE_INVALID = uint8(-1),

    TYPE_BUILTIN = 0,
    TYPE_USER_DEFINED,
    TYPE_ENUM,
    TYPE_ALIAS,
    TYPE_GENERIC_INSTANCE,
    TYPE_GENERIC_PARAMETER,
    TYPE_PLACEHOLDER
};

static inline constexpr const char* SymbolTypeClassToString(SymbolTypeClass typeClass)
{
    switch (typeClass)
    {
    case TYPE_INVALID:
        return "<invalid>";
    case TYPE_BUILTIN:
        return "<builtin>";
    case TYPE_USER_DEFINED:
        return "<class>";
    case TYPE_ENUM:
        return "<enum>";
    case TYPE_ALIAS:
        return "<alias>";
    case TYPE_GENERIC_INSTANCE:
        return "<generic>";
    case TYPE_GENERIC_PARAMETER:
        return "<generic parameter>";
    case TYPE_PLACEHOLDER:
        return "<placeholder>";
    default:
        return "<unknown>";
    }
}

using SymbolTypeFlags = uint32;

enum SymbolTypeFlagsBits : SymbolTypeFlags
{
    SYMBOL_TYPE_FLAGS_NONE = 0x0,
    SYMBOL_TYPE_FLAGS_PROXY = 0x1,
    SYMBOL_TYPE_FLAGS_NATIVE = 0x4
};

enum SymbolTypeIncompatibilityType : uint32
{
    IT_UNKNOWN = 1,
    IT_TYPE_CLASS_MISMATCH,
    IT_UNDEFINED_TYPE,
    IT_NAME_MISMATCH,
    IT_DATA_LOSS,
    IT_IMPLICIT_ANY,
    IT_VARARGS,
    IT_NULLABLE_MISMATCH,
    IT_BASE_MISMATCH,
    IT_MEMBER_MISMATCH,
    IT_STATIC_MEMBER_MISMATCH,
    IT_GENERIC_ARG_MISMATCH
};

struct SymbolTypeIncompatibility
{
    SymbolTypeIncompatibilityType type;
    String details;
};

using SymbolTypeIncompatibilities = Array<SymbolTypeIncompatibility, DynamicAllocator>;

enum ConstantBitSize : uint8
{
    CBS_INVALID = 0,

    CBS_8 = 8,
    CBS_16 = 16,
    CBS_32 = 32,
    CBS_64 = 64
};

static inline constexpr int64 CBS_Max_Signed(ConstantBitSize cbs)
{
    constexpr int64 s_table[] = {
        0,
        INT8_MAX,
        INT16_MAX,
        INT32_MAX,
        INT64_MAX
    };

    return s_table[MathUtil::FastLog2_Pow2(int(cbs)) - 2];
}

static inline constexpr int64 CBS_Min_Signed(ConstantBitSize cbs)
{
    constexpr int64 s_table[] = {
        0,
        INT8_MIN,
        INT16_MIN,
        INT32_MIN,
        INT64_MIN
    };

    return s_table[MathUtil::FastLog2_Pow2(int(cbs)) - 2];
}

static inline constexpr uint64 CBS_Max_Unsigned(ConstantBitSize cbs)
{
    constexpr uint64 s_table[] = {
        0,
        UINT8_MAX,
        UINT16_MAX,
        UINT32_MAX,
        UINT64_MAX
    };

    return s_table[MathUtil::FastLog2_Pow2(int(cbs)) - 2];
}

static inline constexpr double CBS_Min_Float(ConstantBitSize cbs)
{
    constexpr double s_table[] = {
        0.0,
        0.0,
        0.0,
        -FLT_MAX,
        -DBL_MAX
    };

    return s_table[MathUtil::FastLog2_Pow2(int(cbs)) - 2];
}

static inline constexpr double CBS_Max_Float(ConstantBitSize cbs)
{
    constexpr double s_table[] = {
        0.0,
        0.0,
        0.0,
        FLT_MAX,
        DBL_MAX
    };

    return s_table[MathUtil::FastLog2_Pow2(int(cbs)) - 2];
}

struct AliasTypeInfo
{
    const SymbolType* m_aliasee = nullptr;
};

struct GenericInstanceTypeInfo
{
    struct Arg
    {
        String m_name;
        const SymbolType* m_type;
        RC<AstExpression> m_defaultValue;
        bool m_isRef : 1;
        bool m_isConst : 1;

        Arg()
            : m_type(nullptr),
              m_isRef(false),
              m_isConst(false)
        {
        }

        Arg(const String& name, const SymbolType* type, const RC<AstExpression>& defaultValue = nullptr)
            : Arg(name, type, defaultValue, /* isRef */ false, /* isConst */ false)
        {
        }

        Arg(const String& name, const SymbolType* type, const RC<AstExpression>& defaultValue, bool isRef, bool isConst)
            : m_name(name),
              m_type(type),
              m_defaultValue(defaultValue),
              m_isRef(isRef),
              m_isConst(isConst)
        {
        }

        Arg(const Arg& other) = default;
        Arg& operator=(const Arg& other) = default;

        Arg(Arg&& other) noexcept = default;
        Arg& operator=(Arg&& other) noexcept = default;

        ~Arg() = default;

        HashCode GetHashCode() const;
    };

    Array<Arg> m_genericArgs;

    HashCode GetHashCode() const;
};

struct GenericParameterTypeInfo
{
};

class SymbolType;

class SymbolTypeRegistration
{
public:
    explicit SymbolTypeRegistration(SymbolType* symbolType);

    SymbolTypeRegistration(const SymbolTypeRegistration& other) = delete;
    SymbolTypeRegistration& operator=(const SymbolTypeRegistration& other) = delete;

    SymbolTypeRegistration(SymbolTypeRegistration&& other) noexcept = delete;
    SymbolTypeRegistration& operator=(SymbolTypeRegistration&& other) noexcept = delete;

    ~SymbolTypeRegistration();

private:
    SymbolType* m_symbolType;
};

class SymbolType final : public EnableRefCountedPtrFromThis<SymbolType>
{
    friend class SymbolTypeRegistration;
    friend class IdentifierTable;

    SymbolType();

public:
    /*! \brief Create a temporary type to be filled in later. */
    static SymbolType* Temp()
    {
        return new SymbolType();
    }

    static SymbolType* Alias(
        const String& name,
        const AliasTypeInfo& info);

    /*! \brief Defer resolution of this type until usage.
     */
    static SymbolType* Placeholder(const String& name);

    static SymbolType* Primitive(
        const String& name,
        const RC<AstExpression>& defaultValue,
        ConstantBitSize bitSize = CBS_INVALID,
        Array<SymbolTypeMember>&& members = {},
        Array<SymbolTypeMember>&& staticMembers = {});

    static SymbolType* Enum(
        const String& name,
        const SymbolType* underlyingType,
        Array<SymbolTypeMember>&& enumMembers);

    static SymbolType* Object(
        const String& name,
        const SymbolType* baseType,
        Array<SymbolTypeMember>&& members,
        Array<SymbolTypeMember>&& staticMembers);

    static SymbolType* Generic(
        const String& name,
        Array<SymbolTypeMember>&& members,
        Array<SymbolTypeMember>&& staticMembers,
        const GenericInstanceTypeInfo& info);

    static SymbolType* Generic(
        const String& name,
        const SymbolType* baseType,
        Array<SymbolTypeMember>&& members,
        Array<SymbolTypeMember>&& staticMembers,
        const GenericInstanceTypeInfo& info);

    static SymbolType* GenericInstance(
        const SymbolType* genericType,
        Array<SymbolTypeMember>&& members,
        Array<SymbolTypeMember>&& staticMembers,
        const GenericInstanceTypeInfo& info);

    static SymbolType* GenericInstance(
        const String& name,
        const SymbolType* genericType,
        Array<SymbolTypeMember>&& members,
        Array<SymbolTypeMember>&& staticMembers,
        const GenericInstanceTypeInfo& info);

    static SymbolType* GenericParameter(const String& name);

    static SymbolType* Extend(
        const String& name,
        const SymbolType* baseType,
        Array<SymbolTypeMember>&& members,
        Array<SymbolTypeMember>&& staticMembers);

    static const SymbolType* TypePromotion(
        const SymbolType* lptr,
        const SymbolType* rptr);

public:
    SymbolType(
        const String& name,
        SymbolTypeClass typeClass,
        const SymbolType* base);

    SymbolType(
        const String& name,
        SymbolTypeClass typeClass,
        const SymbolType* base,
        const RC<AstExpression>& defaultValue,
        Array<SymbolTypeMember>&& members,
        Array<SymbolTypeMember>&& staticMembers);

    SymbolType(const SymbolType& other) = delete;
    SymbolType& operator=(const SymbolType& other) = delete;
    SymbolType(SymbolType&& other) noexcept = delete;
    SymbolType& operator=(SymbolType&& other) noexcept = delete;
    ~SymbolType() override;

    HYP_FORCE_INLINE bool operator==(const SymbolType& other) const
    {
        return TypeEqual(other);
    }

    HYP_FORCE_INLINE bool operator!=(const SymbolType& other) const
    {
        return !operator==(other);
    }

    HYP_FORCE_INLINE const String& GetName() const
    {
        return m_name;
    }

    HYP_FORCE_INLINE SymbolTypeClass GetTypeClass() const
    {
        return m_typeClass;
    }

    HYP_FORCE_INLINE const SymbolType* GetBaseType() const
    {
        return m_base;
    }

    HYP_FORCE_INLINE void SetBaseType(const SymbolType* base)
    {
        m_base = base ? base->GetUnaliased() : nullptr;
    }

    HYP_FORCE_INLINE const RC<AstExpression>& GetDefaultValue() const
    {
        return m_defaultValue;
    }

    HYP_FORCE_INLINE void SetDefaultValue(const RC<AstExpression>& defaultValue)
    {
        m_defaultValue = defaultValue;
    }

    HYP_FORCE_INLINE Array<SymbolTypeMember>& GetMembers()
    {
        return m_members;
    }

    HYP_FORCE_INLINE const Array<SymbolTypeMember>& GetMembers() const
    {
        return m_members;
    }

    HYP_FORCE_INLINE void SetMembers(const Array<SymbolTypeMember>& members)
    {
        m_members = members;
    }

    HYP_FORCE_INLINE Array<SymbolTypeMember>& GetStaticMembers()
    {
        return m_staticMembers;
    }

    HYP_FORCE_INLINE const Array<SymbolTypeMember>& GetStaticMembers() const
    {
        return m_staticMembers;
    }

    HYP_FORCE_INLINE AliasTypeInfo& GetAliasInfo()
    {
        return m_aliasInfo;
    }

    HYP_FORCE_INLINE const AliasTypeInfo& GetAliasInfo() const
    {
        return m_aliasInfo;
    }

    HYP_FORCE_INLINE GenericInstanceTypeInfo& GetGenericInstanceInfo()
    {
        return m_genericInstanceInfo;
    }

    HYP_FORCE_INLINE const GenericInstanceTypeInfo& GetGenericInstanceInfo() const
    {
        return m_genericInstanceInfo;
    }

    HYP_FORCE_INLINE GenericParameterTypeInfo& GetGenericParameterInfo()
    {
        return m_genericParamInfo;
    }

    HYP_FORCE_INLINE const GenericParameterTypeInfo& GetGenericParameterInfo() const
    {
        return m_genericParamInfo;
    }

    HYP_FORCE_INLINE SymbolTypeFlags GetFlags() const
    {
        return m_flags;
    }

    HYP_FORCE_INLINE SymbolTypeFlags& GetFlags()
    {
        return m_flags;
    }

    HYP_FORCE_INLINE void SetFlags(SymbolTypeFlags flags)
    {
        m_flags = flags;
    }

    HYP_FORCE_INLINE Scope* GetDeclScope() const
    {
        return m_declScope;
    }

    HYP_FORCE_INLINE void SetDeclScope(Scope* scope)
    {
        m_declScope = scope;
    }

    HYP_FORCE_INLINE bool IsRegistered() const
    {
        return m_registration != nullptr;
    }

    HYP_FORCE_INLINE void AssertRegistered() const
    {
        Assert(IsRegistered());
    }

    void Register(CompilationUnit* compilationUnit);

    String ToString(bool includeParameterNames = false) const;

    HYP_FORCE_INLINE bool IsAlias() const
    {
        return m_typeClass == TYPE_ALIAS;
    }

    bool TypeEqual(const SymbolType& other) const;

    bool TypeCompatible(
        const SymbolType& other,
        bool strictNumbers,
        bool strictAny,
        bool strictEnum,
        SymbolTypeIncompatibilities* outIncompatibilities = nullptr) const;

    const SymbolType* FindMember(UTF8StringView name) const;
    bool FindMember(UTF8StringView name, SymbolTypeMember& out) const;
    bool FindMember(UTF8StringView name, SymbolTypeMember& out, uint32& outIndex) const;

    const SymbolType* FindMemberDeep(UTF8StringView name) const;
    bool FindMemberDeep(UTF8StringView name, SymbolTypeMember& out) const;
    bool FindMemberDeep(UTF8StringView name, SymbolTypeMember& out, uint32& outIndex) const;
    bool FindMemberDeep(UTF8StringView name, SymbolTypeMember& out, uint32& outIndex, uint32& outDepth) const;

    const SymbolType* FindStaticMember(UTF8StringView name) const;
    bool FindStaticMember(UTF8StringView name, SymbolTypeMember& out) const;
    bool FindStaticMember(UTF8StringView name, SymbolTypeMember& out, uint32& outIndex) const;

    bool IsOrHasBase(const SymbolType& baseType) const;
    /** Search the inheritance chain to see if the given type
        is a base of this type. */
    bool HasBase(const SymbolType& baseType) const;

    /** Find the root aliasee. If not an alias, just returns itself */
    SymbolType* GetUnaliased();

    HYP_FORCE_INLINE const SymbolType* GetUnaliased() const
    {
        return const_cast<SymbolType*>(this)->GetUnaliased();
    }

    bool IsNumber() const;
    bool IsIntegral() const;
    bool IsSignedIntegral() const;
    bool IsUnsignedIntegral() const;
    bool IsFloat() const;
    bool IsBoolean() const;
    bool IsObject() const;
    bool IsAnyType() const;
    bool IsPlaceholderType() const;
    bool IsNullType() const;
    bool IsNullableType() const;
    bool IsVarArgsType() const;
    bool IsString() const;

    /*! \brief Is this type an uninstantiated generic parameter? (e.g. T) */
    bool IsGenericParameter() const;

    /*! \brief Is this type an instantiated generic type? (e.g. `List<int>`) */
    bool IsGenericInstanceType() const;

    /*! \brief Is this type a primitive type? (e.g. int, float) */
    bool IsPrimitive() const;

    /*! \brief Is this an enum type? */
    bool IsEnumType() const;

    bool IsProxyClass() const
    {
        return m_flags & SYMBOL_TYPE_FLAGS_PROXY;
    }

    ConstantBitSize GetConstantBitSize() const
    {
        return m_constantBitSize;
    }

    HashCode GetHashCode() const
    {
        HashSet<String> duplicateNames;

        return GetHashCodeWithDuplicateRemoval(duplicateNames);
    }

    SymbolType* Clone() const
    {
        SymbolType* result = new SymbolType();
        result->m_name = m_name;
        result->m_typeClass = m_typeClass;
        result->m_base = m_base;
        result->m_defaultValue = m_defaultValue;
        result->m_members = m_members;
        result->m_staticMembers = m_staticMembers;
        result->m_aliasInfo = m_aliasInfo;
        result->m_genericInstanceInfo = m_genericInstanceInfo;
        result->m_genericParamInfo = m_genericParamInfo;
        result->m_constantBitSize = m_constantBitSize;
        result->m_flags = m_flags;
        result->m_declScope = nullptr; // do not copy scope

        return result;
    }

    /*! \brief Copy the contents of another SymbolType into this one, mutating it.
        This is used when instantiating generic types to avoid having to re-cache
        the new instance. */
    void Assign(const SymbolType& other)
    {
        m_name = std::move(other.m_name);
        m_typeClass = other.m_typeClass;
        m_base = other.m_base;
        m_defaultValue = std::move(other.m_defaultValue);
        m_members = std::move(other.m_members);
        m_staticMembers = std::move(other.m_staticMembers);
        m_aliasInfo = std::move(other.m_aliasInfo);
        m_genericInstanceInfo = std::move(other.m_genericInstanceInfo);
        m_genericParamInfo = std::move(other.m_genericParamInfo);
        m_constantBitSize = other.m_constantBitSize;
        m_flags = other.m_flags;
        m_declScope = nullptr; // do not copy scope
    }

private:
    HashCode GetHashCodeWithDuplicateRemoval(HashSet<String>& duplicateNames) const;

    String m_name;
    SymbolTypeClass m_typeClass;
    RC<AstExpression> m_defaultValue;
    Array<SymbolTypeMember> m_members;
    Array<SymbolTypeMember> m_staticMembers;

    // type that this type is based off of
    const SymbolType* m_base;

    // if this is an alias of another type
    AliasTypeInfo m_aliasInfo;
    // if this is an instance of a generic type
    GenericInstanceTypeInfo m_genericInstanceInfo;
    // if this is a generic param
    GenericParameterTypeInfo m_genericParamInfo;

    ConstantBitSize m_constantBitSize;
    SymbolTypeFlags m_flags;
    Scope* m_declScope;

    // set to default empty registration upon creation so we can delete all unregistered types
    SymbolTypeRegistration* m_registration;
};

} // namespace hyperion
