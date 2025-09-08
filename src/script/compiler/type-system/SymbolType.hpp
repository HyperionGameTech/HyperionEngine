#pragma once

#include <core/memory/RefCountedPtr.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/HashSet.hpp>
#include <core/containers/String.hpp>
#include <core/Types.hpp>

#include <memory>
#include <string>
#include <vector>
#include <tuple>
#include <utility>

namespace hyperion {

// forward declaration
class SymbolType;
class AstExpression;
class AstArgument;
struct Scope;

using SymbolTypeRef = RC<SymbolType>;
using SymbolTypeWeakRef = Weak<SymbolType>;

struct SymbolTypeFunctionSignature
{
    SymbolTypeRef returnType;
    Array<RC<AstArgument>> params;
};

struct SymbolTypeMember
{
    String name;
    SymbolTypeRef type;
    RC<AstExpression> expr;
};

enum SymbolTypeClass : uint8
{
    TYPE_INVALID = uint8(-1),

    TYPE_BUILTIN = 0,
    TYPE_USER_DEFINED,
    TYPE_ALIAS,
    TYPE_GENERIC_INSTANCE,
    TYPE_GENERIC_PARAMETER,
    TYPE_PLACEHOLDER
};

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

struct AliasTypeInfo
{
    SymbolTypeWeakRef m_aliasee;
};

struct FunctionTypeInfo
{
    Array<SymbolTypeRef> m_paramTypes;
    SymbolTypeRef m_returnType;
};

struct GenericInstanceTypeInfo
{
    struct Arg
    {
        String m_name;
        SymbolTypeRef m_type;
        RC<AstExpression> m_defaultValue;
        bool m_isRef : 1;
        bool m_isConst : 1;

        Arg()
            : m_isRef(false),
              m_isConst(false)
        {
        }

        Arg(const String& name, const SymbolTypeRef& type, const RC<AstExpression>& defaultValue = nullptr)
            : Arg(name, type, defaultValue, /* isRef */ false, /* isConst */ false)
        {
        }

        Arg(const String& name, const SymbolTypeRef& type, const RC<AstExpression>& defaultValue, bool isRef, bool isConst)
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

struct SymbolTypeTrait
{
    String name;

    HYP_FORCE_INLINE bool operator==(const SymbolTypeTrait& other) const
    {
        return name == other.name;
    }

    HYP_FORCE_INLINE bool operator!=(const SymbolTypeTrait& other) const
    {
        return !operator==(other);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name);

        return hc;
    }
};

class SymbolType : public EnableRefCountedPtrFromThis<SymbolType>
{
    friend class IdentifierTable;

    SymbolType()
        : m_name("<temp>"),
          m_typeClass(TYPE_INVALID),
          m_base(nullptr),
          m_defaultValue(nullptr),
          m_flags(SYMBOL_TYPE_FLAGS_NONE),
          m_declScope(nullptr)
    {
    }

public:
    /*! \brief Create a temporary type to be filled in later. */
    static SymbolTypeRef Temp()
    {
        return RC<SymbolType>(new SymbolType());
    }

    static SymbolTypeRef Alias(
        const String& name,
        const AliasTypeInfo& info);

    /*! \brief Defer resolution of this type until usage.
     */
    static SymbolTypeRef Placeholder(
        const String& name);

    static SymbolTypeRef Primitive(
        const String& name,
        const RC<AstExpression>& defaultValue);

    static SymbolTypeRef Object(
        const String& name,
        const SymbolTypeRef& base,
        const Array<SymbolTypeMember>& members,
        const Array<SymbolTypeMember>& staticMembers);

    static SymbolTypeRef Generic(
        const String& name,
        const Array<SymbolTypeMember>& members,
        const Array<SymbolTypeMember>& staticMembers,
        const GenericInstanceTypeInfo& info);

    static SymbolTypeRef Generic(
        const String& name,
        const SymbolTypeRef& baseType,
        const Array<SymbolTypeMember>& members,
        const Array<SymbolTypeMember>& staticMembers,
        const GenericInstanceTypeInfo& info);

    static SymbolTypeRef GenericInstance(
        const SymbolTypeRef& genericType,
        const Array<SymbolTypeMember>& members,
        const Array<SymbolTypeMember>& staticMembers,
        const GenericInstanceTypeInfo& info);

    static SymbolTypeRef GenericInstance(
        const String& name,
        const SymbolTypeRef& genericType,
        const Array<SymbolTypeMember>& members,
        const Array<SymbolTypeMember>& staticMembers,
        const GenericInstanceTypeInfo& info);

    static SymbolTypeRef GenericParameter(
        const String& name);

    static SymbolTypeRef Extend(
        const String& name,
        const SymbolTypeRef& base,
        const Array<SymbolTypeMember>& members,
        const Array<SymbolTypeMember>& staticMembers);

    static SymbolTypeRef TypePromotion(
        const SymbolTypeRef& lptr,
        const SymbolTypeRef& rptr);

    /** Substitute this or any generic parameters of this object which
        are the given generic type with the supplied substitution */
    static SymbolTypeRef SubstituteGenericParams(
        const SymbolTypeRef& lptr,
        const SymbolTypeRef& placeholder,
        const SymbolTypeRef& substitute);

public:
    SymbolType(
        const String& name,
        SymbolTypeClass typeClass,
        const SymbolTypeRef& base);

    SymbolType(
        const String& name,
        SymbolTypeClass typeClass,
        const SymbolTypeRef& base,
        const RC<AstExpression>& defaultValue,
        const Array<SymbolTypeMember>& members,
        const Array<SymbolTypeMember>& staticMembers);

    SymbolType(const SymbolType& other) = delete;
    SymbolType& operator=(const SymbolType& other) = delete;
    SymbolType(SymbolType&& other) noexcept = delete;
    SymbolType& operator=(SymbolType&& other) noexcept = delete;
    ~SymbolType() override = default;

    const String& GetName() const
    {
        return m_name;
    }

    SymbolTypeClass GetTypeClass() const
    {
        return m_typeClass;
    }

    const SymbolTypeRef& GetBaseType() const
    {
        return m_base;
    }

    void SetBaseType(const SymbolTypeRef& base)
    {
        m_base = base ? base->GetUnaliased() : nullptr;
    }

    const RC<AstExpression>& GetDefaultValue() const
    {
        return m_defaultValue;
    }

    void SetDefaultValue(const RC<AstExpression>& defaultValue)
    {
        m_defaultValue = defaultValue;
    }

    Array<SymbolTypeMember>& GetMembers()
    {
        return m_members;
    }

    const Array<SymbolTypeMember>& GetMembers() const
    {
        return m_members;
    }

    void SetMembers(const Array<SymbolTypeMember>& members)
    {
        m_members = members;
    }

    Array<SymbolTypeMember>& GetStaticMembers()
    {
        return m_staticMembers;
    }

    const Array<SymbolTypeMember>& GetStaticMembers() const
    {
        return m_staticMembers;
    }

    AliasTypeInfo& GetAliasInfo()
    {
        return m_aliasInfo;
    }

    const AliasTypeInfo& GetAliasInfo() const
    {
        return m_aliasInfo;
    }

    GenericInstanceTypeInfo& GetGenericInstanceInfo()
    {
        return m_genericInstanceInfo;
    }

    const GenericInstanceTypeInfo& GetGenericInstanceInfo() const
    {
        return m_genericInstanceInfo;
    }

    GenericParameterTypeInfo& GetGenericParameterInfo()
    {
        return m_genericParamInfo;
    }
    const GenericParameterTypeInfo& GetGenericParameterInfo() const
    {
        return m_genericParamInfo;
    }

    SymbolTypeFlags GetFlags() const
    {
        return m_flags;
    }

    SymbolTypeFlags& GetFlags()
    {
        return m_flags;
    }

    void SetFlags(SymbolTypeFlags flags)
    {
        m_flags = flags;
    }

    Scope* GetDeclScope() const
    {
        return m_declScope;
    }

    void SetDeclScope(Scope* scope)
    {
        m_declScope = scope;
    }

    String ToString(bool includeParameterNames = false) const;

    bool IsAlias() const
    {
        return m_typeClass == TYPE_ALIAS;
    }

    bool TypeEqual(const SymbolType& other) const;

    bool TypeCompatible(
        const SymbolType& other,
        bool strictNumbers,
        bool strictAny,
        SymbolTypeIncompatibilities* outIncompatibilities = nullptr) const;

    bool operator==(const SymbolType& other) const
    {
        return TypeEqual(other);
    }
    bool operator!=(const SymbolType& other) const
    {
        return !operator==(other);
    }

    SymbolTypeRef FindMember(const String& name) const;
    bool FindMember(const String& name, SymbolTypeMember& out) const;
    bool FindMember(const String& name, SymbolTypeMember& out, uint32& outIndex) const;
    SymbolTypeRef FindMemberDeep(const String& name) const;
    bool FindMemberDeep(const String& name, SymbolTypeMember& out) const;
    bool FindMemberDeep(const String& name, SymbolTypeMember& out, uint32& outIndex) const;
    bool FindMemberDeep(const String& name, SymbolTypeMember& out, uint32& outIndex, uint32& outDepth) const;

    bool HasTrait(const SymbolTypeTrait& trait) const;
    bool HasTraitDeep(const SymbolTypeTrait& trait) const;

    bool IsOrHasBase(const SymbolType& baseType) const;
    /** Search the inheritance chain to see if the given type
        is a base of this type. */
    bool HasBase(const SymbolType& baseType) const;
    /** Find the root aliasee. If not an alias, just returns itself */
    SymbolTypeRef GetUnaliased() const;

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

    HashCode GetHashCode() const
    {
        HashSet<String> duplicateNames;

        return GetHashCodeWithDuplicateRemoval(duplicateNames);
    }

    SymbolTypeRef Clone() const
    {
        SymbolTypeRef result = SymbolTypeRef(new SymbolType());
        result->m_name = m_name;
        result->m_typeClass = m_typeClass;
        result->m_base = m_base;
        result->m_defaultValue = m_defaultValue;
        result->m_members = m_members;
        result->m_staticMembers = m_staticMembers;
        result->m_aliasInfo = m_aliasInfo;
        result->m_genericInstanceInfo = m_genericInstanceInfo;
        result->m_genericParamInfo = m_genericParamInfo;
        result->m_flags = m_flags;
        result->m_declScope = nullptr; // do not copy scope

        return result;
    }

    /*! \brief Copy the contents of another SymbolType into this one, mutating it.
        This is used when instantiating generic types to avoid having to re-cache
        the new instance. */
    void CopyMutate(const SymbolType& other)
    {
        m_name = std::move(other.m_name);
        m_typeClass = other.m_typeClass;
        m_base = std::move(other.m_base);
        m_defaultValue = std::move(other.m_defaultValue);
        m_members = std::move(other.m_members);
        m_staticMembers = std::move(other.m_staticMembers);
        m_aliasInfo = std::move(other.m_aliasInfo);
        m_genericInstanceInfo = std::move(other.m_genericInstanceInfo);
        m_genericParamInfo = std::move(other.m_genericParamInfo);
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
    SymbolTypeRef m_base;

    // if this is an alias of another type
    AliasTypeInfo m_aliasInfo;
    // if this is an instance of a generic type
    GenericInstanceTypeInfo m_genericInstanceInfo;
    // if this is a generic param
    GenericParameterTypeInfo m_genericParamInfo;

    SymbolTypeFlags m_flags;
    Scope* m_declScope;
};

} // namespace hyperion
