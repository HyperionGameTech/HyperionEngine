#ifndef SYMBOL_TYPE_HPP
#define SYMBOL_TYPE_HPP

#include <memory>
#include <string>
#include <vector>
#include <tuple>
#include <utility>

namespace hyperion::compiler {

template <typename T> using sp = std::shared_ptr<T>;
template <typename T> using wp = std::weak_ptr<T>;
template <typename T> using vec = std::vector<T>;

// forward declaration
class SymbolType;
class AstExpression;
class AstArgument;

using SymbolTypePtr_t = sp<SymbolType>;
using SymbolTypeWeakPtr_t = wp<SymbolType>;
using SymbolMember_t = std::tuple<std::string, SymbolTypePtr_t, sp<AstExpression>>;
using FunctionTypeSignature_t = std::pair<SymbolTypePtr_t, std::vector<std::shared_ptr<AstArgument>>>;

enum SymbolTypeClass {
    TYPE_BUILTIN,
    TYPE_USER_DEFINED,
    TYPE_ALIAS,
    TYPE_FUNCTION,
    TYPE_ARRAY,
    TYPE_GENERIC,
    TYPE_GENERIC_INSTANCE,
    TYPE_GENERIC_PARAMETER,
};

struct AliasTypeInfo {
    SymbolTypeWeakPtr_t m_aliasee;
};

struct FunctionTypeInfo {
    vec<SymbolTypePtr_t> m_param_types;
    SymbolTypePtr_t m_return_type;
};

struct GenericTypeInfo {
    int m_num_parameters; // -1 for variadic
    vec<SymbolTypePtr_t> m_params;
};

struct GenericInstanceTypeInfo {
    struct Arg {
        std::string m_name;
        SymbolTypePtr_t m_type;
        sp<AstExpression> m_default_value;
    };

    vec<Arg> m_generic_args;
};

struct GenericParameterTypeInfo {
    SymbolTypeWeakPtr_t m_substitution;
};

class SymbolType : public std::enable_shared_from_this<SymbolType> {
public:
    static SymbolTypePtr_t Alias(
        const std::string &name,
        const AliasTypeInfo &info
    );

    static SymbolTypePtr_t Primitive(
        const std::string &name, 
        const sp<AstExpression> &default_value
    );

    static SymbolTypePtr_t Primitive(
        const std::string &name,
        const sp<AstExpression> &default_value,
        const SymbolTypePtr_t &base
    );

    static SymbolTypePtr_t Object(
        const std::string &name,
        const vec<SymbolMember_t> &members
    );

    static SymbolTypePtr_t Object(
        const std::string &name,
        const vec<SymbolMember_t> &members,
        const SymbolTypePtr_t &base
    );

    /** A generic type template. Members may have the type class TYPE_GENERIC_PARAMETER.
        They will be substituted when an instance of the generic type is created.
    */
    static SymbolTypePtr_t Generic(
        const std::string &name,
        const vec<SymbolMember_t> &members, 
        const GenericTypeInfo &info,
        const SymbolTypePtr_t &base
    );
    
    static SymbolTypePtr_t Generic(
        const std::string &name, 
        const sp<AstExpression> &default_value, 
        const vec<SymbolMember_t> &members, 
        const GenericTypeInfo &info,
        const SymbolTypePtr_t &base
    );

    static SymbolTypePtr_t GenericInstance(
        const SymbolTypePtr_t &base,
        const GenericInstanceTypeInfo &info
    );

    static SymbolTypePtr_t GenericParameter(
        const std::string &name, 
        const SymbolTypePtr_t &substitution
    );
    
    static SymbolTypePtr_t Extend(
        const std::string &name,
        const SymbolTypePtr_t &base,
        const vec<SymbolMember_t> &members
    );
    
    static SymbolTypePtr_t Extend(
        const SymbolTypePtr_t &base,
        const vec<SymbolMember_t> &members
    );
    
    static SymbolTypePtr_t PrototypedObject(
        const std::string &name,
        const SymbolTypePtr_t &base,
        const vec<SymbolMember_t> &prototype_members
    );

    static SymbolTypePtr_t TypePromotion(
        const SymbolTypePtr_t &lptr,
        const SymbolTypePtr_t &rptr,
        bool use_number
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
        const std::string &name, 
        SymbolTypeClass type_class, 
        const SymbolTypePtr_t &base
    );

    SymbolType(
        const std::string &name, 
        SymbolTypeClass type_class, 
        const SymbolTypePtr_t &base,
        const sp<AstExpression> &default_value,
        const vec<SymbolMember_t> &members
    );
        
    SymbolType(const SymbolType &other);

    inline const std::string &GetName() const { return m_name; }
    inline SymbolTypeClass GetTypeClass() const { return m_type_class; }
    inline SymbolTypePtr_t GetBaseType() const { return m_base.lock(); }

    inline const sp<AstExpression> &GetDefaultValue() const
        { return m_default_value; }
    inline void SetDefaultValue(const sp<AstExpression> &default_value)
        { m_default_value = default_value; }
    
    inline vec<SymbolMember_t> &GetMembers()
        { return m_members; }
    inline const vec<SymbolMember_t> &GetMembers() const
        { return m_members; }

    inline AliasTypeInfo &GetAliasInfo()
        { return m_alias_info; }
    inline const AliasTypeInfo &GetAliasInfo() const
        { return m_alias_info; }

    inline FunctionTypeInfo &GetFunctionInfo()
        { return m_function_info; }
    inline const FunctionTypeInfo &GetFunctionInfo() const
        { return m_function_info; }

    inline GenericTypeInfo &GetGenericInfo()
        { return m_generic_info; }
    inline const GenericTypeInfo &GetGenericInfo() const
        { return m_generic_info; }

    inline GenericInstanceTypeInfo &GetGenericInstanceInfo()
        { return m_generic_instance_info; }
    inline const GenericInstanceTypeInfo &GetGenericInstanceInfo() const
        { return m_generic_instance_info; }

    inline GenericParameterTypeInfo &GetGenericParameterInfo()
        { return m_generic_param_info; }
    inline const GenericParameterTypeInfo &GetGenericParameterInfo() const
        { return m_generic_param_info; }

    inline int GetId() const { return m_id; }
    inline void SetId(int id) { m_id = id; }

    bool TypeEqual(const SymbolType &other) const;
    bool TypeCompatible(const SymbolType &other,
        bool strict_numbers,
        bool strict_const = false) const;

    inline bool operator==(const SymbolType &other) const { return TypeEqual(other); }
    inline bool operator!=(const SymbolType &other) const { return !operator==(other); }
    const SymbolTypePtr_t FindMember(const std::string &name) const;
    bool FindMember(const std::string &name, SymbolMember_t &out) const;
    const SymbolTypePtr_t FindPrototypeMember(const std::string &name) const;
    bool FindPrototypeMember(const std::string &name, SymbolMember_t &out) const;
    const sp<AstExpression> GetPrototypeValue() const;

    /** Search the inheritance chain to see if the given type
        is a base of this type. */
    bool HasBase(const SymbolType &base_type) const;
    /** Find the root aliasee. If not an alias, just returns itself */
    SymbolTypePtr_t GetUnaliased();
    
    bool IsArrayType() const;
    bool IsConstType() const;
    bool IsBoxedType() const;
    /** Is is an uninstantiated generic parameter? (e.g T) */
    bool IsGenericParameter() const;

private:
    std::string m_name;
    SymbolTypeClass m_type_class;
    sp<AstExpression> m_default_value;
    vec<SymbolMember_t> m_members;

    // type that this type is based off of
    SymbolTypeWeakPtr_t m_base;

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
};

} // namespace hyperion::compiler

#endif
