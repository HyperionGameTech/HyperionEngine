#pragma once

#include <core/memory/RefCountedPtr.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatSet.hpp>
#include <core/containers/String.hpp>
#include <core/Types.hpp>

#include <memory>
#include <string>
#include <vector>
#include <tuple>
#include <utility>

namespace hyperion::compiler {

// forward declaration
class SymbolType;
class AstExpression;
class AstArgument;

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

enum SymbolTypeClass
{
    TYPE_BUILTIN,
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
    SYMBOL_TYPE_FLAGS_UNINSTANTIATED_GENERIC = 0x2,
    SYMBOL_TYPE_FLAGS_NATIVE = 0x4
};

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
        bool m_isRef = false;
        bool m_isConst = false;
    };

    Array<Arg> m_genericArgs;
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
public:
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

    static SymbolTypeRef GenericInstance(
        const SymbolTypeRef& base,
        const Array<SymbolTypeMember>& members,
        const Array<SymbolTypeMember>& staticMembers,
        const GenericInstanceTypeInfo& info);

    static SymbolTypeRef GenericInstance(
        const String& name,
        const SymbolTypeRef& base,
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

    static SymbolTypeRef GenericPromotion(
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

    Array<SymbolTypeMember>& GetStaticMembers()
    {
        return m_staticMembers;
    }

    const Array<SymbolTypeMember>& GetStaticMembers() const
    {
        return m_staticMembers;
    }

    void SetMembers(const Array<SymbolTypeMember>& members)
    {
        m_members = members;
    }

    void AddMember(const SymbolTypeMember& member)
    {
        m_members.PushBack(member);
    }

    AliasTypeInfo& GetAliasInfo()
    {
        return m_aliasInfo;
    }

    const AliasTypeInfo& GetAliasInfo() const
    {
        return m_aliasInfo;
    }

    FunctionTypeInfo& GetFunctionInfo()
    {
        return m_functionInfo;
    }

    const FunctionTypeInfo& GetFunctionInfo() const
    {
        return m_functionInfo;
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

    int GetId() const
    {
        return m_id;
    }
    void SetId(int id)
    {
        m_id = id;
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

    String ToString(bool includeParameterNames = false) const;

    bool IsAlias() const
    {
        return m_typeClass == TYPE_ALIAS;
    }

    bool TypeEqual(const SymbolType& other) const;
    bool TypeCompatible(
        const SymbolType& other,
        bool strictNumbers) const;

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

    SymbolTypeRef FindPrototypeMember(const String& name) const;
    bool FindPrototypeMember(const String& name, SymbolTypeMember& out) const;
    bool FindPrototypeMember(const String& name, SymbolTypeMember& out, uint32& outIndex) const;
    bool FindPrototypeMemberDeep(const String& name) const;
    bool FindPrototypeMemberDeep(const String& name, SymbolTypeMember& out) const;

    bool HasTrait(const SymbolTypeTrait& trait) const;
    bool HasTraitDeep(const SymbolTypeTrait& trait) const;

    bool IsOrHasBase(const SymbolType& baseType) const;
    /** Search the inheritance chain to see if the given type
        is a base of this type. */
    bool HasBase(const SymbolType& baseType) const;
    /** Find the root aliasee. If not an alias, just returns itself */
    SymbolTypeRef GetUnaliased();

    bool IsNumber() const;
    bool IsIntegral() const;
    bool IsSignedIntegral() const;
    bool IsUnsignedIntegral() const;
    bool IsFloat() const;
    bool IsBoolean() const;
    bool IsClass() const;
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
        FlatSet<String> duplicateNames;

        return GetHashCodeWithDuplicateRemoval(duplicateNames);
    }

    // if this is an instance of a generic type
    SymbolTypeClass m_typeClass;
    GenericInstanceTypeInfo m_genericInstanceInfo;

private:
    HashCode GetHashCodeWithDuplicateRemoval(FlatSet<String>& duplicateNames) const;

    String m_name;
    RC<AstExpression> m_defaultValue;
    Array<SymbolTypeMember> m_members;
    Array<SymbolTypeMember> m_staticMembers;

    // type that this type is based off of
    SymbolTypeRef m_base;

    // if this is an alias of another type
    AliasTypeInfo m_aliasInfo;
    // if this type is a function
    FunctionTypeInfo m_functionInfo;
    // if this is a generic param
    GenericParameterTypeInfo m_genericParamInfo;

    int m_id;
    SymbolTypeFlags m_flags;
};

} // namespace hyperion::compiler
