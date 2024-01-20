#ifndef SYMBOL_TYPE_HPP
#define SYMBOL_TYPE_HPP

#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/String.hpp>
#include <Types.hpp>

#include <memory>
#include <string>
#include <vector>
#include <tuple>
#include <utility>

namespace hyperion::compiler {

// forward declaration
class SymbolType;
class AstExpression;
class AstTypeObject;
class AstArgument;

using SymbolTypePtr_t = std::shared_ptr<SymbolType>;
using SymbolTypeWeakPtr_t = std::weak_ptr<SymbolType>;


// struct SymbolMember
// {
//     String name;
//     SymbolTypePtr_t type;
//     RC<AstExpression> expr;
// };

using SymbolMember_t = std::tuple<String, SymbolTypePtr_t, RC<AstExpression>>;
using FunctionTypeSignature_t = std::pair<SymbolTypePtr_t, Array<RC<AstArgument>>>;

enum SymbolTypeClass
{
    TYPE_BUILTIN,
    TYPE_USER_DEFINED,
    TYPE_ALIAS,
    TYPE_GENERIC,
    TYPE_GENERIC_INSTANCE,
    TYPE_GENERIC_PARAMETER
};

using SymbolTypeFlags = UInt32;

enum SymbolTypeFlagsBits : SymbolTypeFlags
{
    SYMBOL_TYPE_FLAGS_NONE = 0x0,
    SYMBOL_TYPE_FLAGS_PROXY = 0x1
};

struct AliasTypeInfo
{
    SymbolTypeWeakPtr_t m_aliasee;
};

struct FunctionTypeInfo
{
    Array<SymbolTypePtr_t>  m_param_types;
    SymbolTypePtr_t         m_return_type;
};

struct GenericTypeInfo
{
    int                     m_num_parameters; // -1 for variadic
    Array<SymbolTypePtr_t>  m_params;
};

struct GenericInstanceTypeInfo
{
    struct Arg
    {
        String              m_name;
        SymbolTypePtr_t     m_type;
        RC<AstExpression>   m_default_value;
        bool                m_is_ref = false;
        bool                m_is_const = false;
    };

    Array<Arg>  m_generic_args;
};

struct GenericParameterTypeInfo
{
};

class SymbolType : public std::enable_shared_from_this<SymbolType>
{
public:
    static SymbolTypePtr_t Alias(
        const String &name,
        const AliasTypeInfo &info
    );

    static SymbolTypePtr_t Primitive(
        const String &name, 
        const RC<AstExpression> &default_value
    );

    static SymbolTypePtr_t Primitive(
        const String &name,
        const RC<AstExpression> &default_value,
        const SymbolTypePtr_t &base
    );

    static SymbolTypePtr_t Object(
        const String &name,
        const Array<SymbolMember_t> &members
    );

    static SymbolTypePtr_t Object(
        const String &name,
        const Array<SymbolMember_t> &members,
        const SymbolTypePtr_t &base
    );

    /** A generic type template. Members may have the type class TYPE_GENERIC_PARAMETER.
        They will be substituted when an instance of the generic type is created.
    */
    static SymbolTypePtr_t Generic(
        const String &name,
        const Array<SymbolMember_t> &members, 
        const GenericTypeInfo &info,
        const SymbolTypePtr_t &base
    );
    
    static SymbolTypePtr_t Generic(
        const String &name, 
        const RC<AstExpression> &default_value, 
        const Array<SymbolMember_t> &members, 
        const GenericTypeInfo &info,
        const SymbolTypePtr_t &base
    );

    static SymbolTypePtr_t Function(
        const SymbolTypePtr_t &return_type,
        const Array<GenericInstanceTypeInfo::Arg> &params
    );

    static SymbolTypePtr_t GenericInstance(
        const SymbolTypePtr_t &base,
        const GenericInstanceTypeInfo &info,
        const Array<SymbolMember_t> &members
    );

    static SymbolTypePtr_t GenericInstance(
        const SymbolTypePtr_t &base,
        const GenericInstanceTypeInfo &info
    );

    static SymbolTypePtr_t GenericParameter(
        const String &name
    );
    
    static SymbolTypePtr_t Extend(
        const String &name,
        const SymbolTypePtr_t &base,
        const Array<SymbolMember_t> &members
    );
    
    static SymbolTypePtr_t PrototypedObject(
        const String &name,
        const SymbolTypePtr_t &base,
        const Array<SymbolMember_t> &prototype_members
    );

    static SymbolTypePtr_t TypePromotion(
        const SymbolTypePtr_t &lptr,
        const SymbolTypePtr_t &rptr
    );

    static SymbolTypePtr_t GenericPromotion(
        const SymbolTypePtr_t &lptr,
        const SymbolTypePtr_t &rptr
    );

    /** Substitute this or any generic parameters of this object which
        are the given generic type with the supplied substitution */
    static SymbolTypePtr_t SubstituteGenericParams(
        const SymbolTypePtr_t &lptr,
        const SymbolTypePtr_t &placeholder,
        const SymbolTypePtr_t &substitute
    );

public:
    SymbolType(
        const String &name, 
        SymbolTypeClass type_class, 
        const SymbolTypePtr_t &base
    );

    SymbolType(
        const String &name, 
        SymbolTypeClass type_class, 
        const SymbolTypePtr_t &base,
        const RC<AstExpression> &default_value,
        const Array<SymbolMember_t> &members
    );
        
    SymbolType(const SymbolType &other);

    const String &GetName() const { return m_name; }
    SymbolTypeClass GetTypeClass() const { return m_type_class; }
    const SymbolTypePtr_t &GetBaseType() const { return m_base; }

    const RC<AstExpression> &GetDefaultValue() const
        { return m_default_value; }

    void SetDefaultValue(const RC<AstExpression> &default_value)
        { m_default_value = default_value; }
    
    Array<SymbolMember_t> &GetMembers()
        { return m_members; }

    const Array<SymbolMember_t> &GetMembers() const
        { return m_members; }

    void SetMembers(const Array<SymbolMember_t> &members)
        { m_members = members; }

    void AddMember(const SymbolMember_t &member)
        { m_members.PushBack(member); }

    AliasTypeInfo &GetAliasInfo()
        { return m_alias_info; }
    const AliasTypeInfo &GetAliasInfo() const
        { return m_alias_info; }

    FunctionTypeInfo &GetFunctionInfo()
        { return m_function_info; }
    const FunctionTypeInfo &GetFunctionInfo() const
        { return m_function_info; }

    GenericTypeInfo &GetGenericInfo()
        { return m_generic_info; }
    const GenericTypeInfo &GetGenericInfo() const
        { return m_generic_info; }

    GenericInstanceTypeInfo &GetGenericInstanceInfo()
        { return m_generic_instance_info; }
    const GenericInstanceTypeInfo &GetGenericInstanceInfo() const
        { return m_generic_instance_info; }

    GenericParameterTypeInfo &GetGenericParameterInfo()
        { return m_generic_param_info; }
    const GenericParameterTypeInfo &GetGenericParameterInfo() const
        { return m_generic_param_info; }

    int GetId() const { return m_id; }
    void SetId(int id) { m_id = id; }

    SymbolTypeFlags GetFlags() const { return m_flags; }
    SymbolTypeFlags &GetFlags() { return m_flags; }

    String ToString(Bool include_parameter_names = false) const;

    bool IsAlias() const { return m_type_class == TYPE_ALIAS; }

    bool TypeEqual(const SymbolType &other) const;
    bool TypeCompatible(
        const SymbolType &other,
        bool strict_numbers,
        bool strict_const = false
    ) const;

    bool operator==(const SymbolType &other) const { return TypeEqual(other); }
    bool operator!=(const SymbolType &other) const { return !operator==(other); }

    const SymbolTypePtr_t FindMember(const String &name) const;
    bool FindMember(const String &name, SymbolMember_t &out) const;
    bool FindMember(const String &name, SymbolMember_t &out, UInt &out_index) const;
    bool FindMemberDeep(const String &name, SymbolMember_t &out) const;
    bool FindMemberDeep(const String &name, SymbolMember_t &out, UInt &out_index) const;
    bool FindMemberDeep(const String &name, SymbolMember_t &out, UInt &out_index, UInt &out_depth) const;

    const SymbolTypePtr_t FindPrototypeMember(const String &name) const;
    bool FindPrototypeMember(const String &name, SymbolMember_t &out) const;
    bool FindPrototypeMember(const String &name, SymbolMember_t &out, UInt &out_index) const;
    bool FindPrototypeMemberDeep(const String &name, SymbolMember_t &out) const;

    const RC<AstTypeObject> &GetTypeObject() const
        { return m_type_object; }

    void SetTypeObject(const RC<AstTypeObject> &type_object)
        { m_type_object = type_object; }

    bool IsOrHasBase(const SymbolType &base_type) const;
    /** Search the inheritance chain to see if the given type
        is a base of this type. */
    bool HasBase(const SymbolType &base_type) const;
    /** Find the root aliasee. If not an alias, just returns itself */
    SymbolTypePtr_t GetUnaliased();
    
    bool IsNumber() const;
    bool IsIntegral() const;
    bool IsUnsignedIntegral() const;
    bool IsFloat() const;
    bool IsClass() const;
    bool IsObject() const;
    bool IsAnyType() const;
    bool IsPlaceholderType() const;
    bool IsNullType() const;
    bool IsNullableType() const;
    bool IsArrayType() const;

    /*! \brief Is this type an uninstantiated generic parameter? (e.g. T) */
    bool IsGenericParameter() const;

    /*! \brief Is this type an uninstantiated generic type? (e.g. List<T>) */
    bool IsGeneric() const;

    /*! \brief Is this type a primitive type? (e.g. Int, Float) */
    bool IsPrimitive() const;

    /*! \brief Is this an enum type? */
    bool IsEnumType() const;

    bool IsProxyClass() const
        { return m_flags & SYMBOL_TYPE_FLAGS_PROXY; }

private:
    String m_name;
    SymbolTypeClass m_type_class;
    RC<AstExpression> m_default_value;
    Array<SymbolMember_t> m_members;

    // type that this type is based off of
    SymbolTypePtr_t m_base;

    RC<AstTypeObject> m_type_object;

    // if this is an alias of another type
    AliasTypeInfo m_alias_info;
    // if this type is a function
    FunctionTypeInfo m_function_info;
    // if this is a generic type
    GenericTypeInfo m_generic_info;
    // if this is an instance of a generic type
    GenericInstanceTypeInfo m_generic_instance_info;
    // if this is a generic param
    GenericParameterTypeInfo m_generic_param_info;

    int m_id;
    SymbolTypeFlags m_flags;
};

} // namespace hyperion::compiler

#endif
