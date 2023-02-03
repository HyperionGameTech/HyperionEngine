#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/ast/AstObject.hpp>
#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>

#include <system/Debug.hpp>
#include <util/UTF8.hpp>

namespace hyperion::compiler {

SymbolType::SymbolType(
    const std::string &name, 
    SymbolTypeClass type_class, 
    const SymbolTypePtr_t &base
) : m_name(name),
    m_type_class(type_class),
    m_default_value(nullptr),
    m_base(base),
    m_id(-1),
    m_flags(SYMBOL_TYPE_FLAGS_NONE)
{
}

SymbolType::SymbolType(
    const std::string &name, 
    SymbolTypeClass type_class,
    const SymbolTypePtr_t &base,
    const RC<AstExpression> &default_value,
    const Array<SymbolMember_t> &members
) : m_name(name),
    m_type_class(type_class),
    m_default_value(default_value),
    m_members(members),
    m_base(base),
    m_id(0),
    m_flags(SYMBOL_TYPE_FLAGS_NONE)
{
}

SymbolType::SymbolType(const SymbolType &other)
    : m_name(other.m_name),
      m_type_class(other.m_type_class),
      m_default_value(other.m_default_value),
      m_members(other.m_members),
      m_base(other.m_base),
      m_id(other.m_id),
      m_flags(other.m_flags)
{
}

bool SymbolType::TypeEqual(const SymbolType &other) const
{
    if (std::addressof(other) == this) {
        return true;
    }

    if (m_name != other.m_name) {
        return false;
    }

    if (m_type_class != other.m_type_class) {
        return false;
    }

    switch (m_type_class) {
        case TYPE_BUILTIN:
            return true;
        case TYPE_ALIAS:
            if (SymbolTypePtr_t sp = m_alias_info.m_aliasee.lock()) {
                return *sp == other;
            }

            return false;

        case TYPE_FUNCTION:
            if (!m_function_info.m_return_type || !other.m_function_info.m_return_type) {
                return false;
            }

            if (!(*m_function_info.m_return_type == *other.m_function_info.m_return_type)) {
                return false;
            }

            for (const SymbolTypePtr_t &i : m_function_info.m_param_types) {
                if (i == nullptr) {
                    return false;
                }

                for (const SymbolTypePtr_t &j : other.m_function_info.m_param_types) {
                    if (j == nullptr || !(*i == *j)) {
                        return false;
                    }
                }
            }

            break;

        case TYPE_GENERIC:
            if (m_generic_info.m_num_parameters != other.m_generic_info.m_num_parameters) {
                return false;
            }

            return true;
            
        case TYPE_GENERIC_INSTANCE:
            if (m_generic_instance_info.m_generic_args.Size() != other.m_generic_instance_info.m_generic_args.Size()) {
                return false;
            }

            // check each substituted parameter
            for (size_t i = 0; i < m_generic_instance_info.m_generic_args.Size(); i++) {
                const SymbolTypePtr_t &instance_arg_type = m_generic_instance_info.m_generic_args[i].m_type;
                const SymbolTypePtr_t &other_arg_type = other.m_generic_instance_info.m_generic_args[i].m_type;

                AssertThrow(instance_arg_type != nullptr);
                AssertThrow(other_arg_type != nullptr);

                if (instance_arg_type != other_arg_type) {
                    return false; // have to do this for now to prevent infinte recursion
                }

                // if (instance_arg_type == other_arg_type) {
                //     continue;
                // }

                // if (!instance_arg_type->TypeEqual(*other_arg_type)) {
                //     return false;
                // }
            }

            // do not go to end (where members are compared)
            return true;

        default:
            break;
    }

    if (m_members.Size() != other.m_members.Size()) {
        return false;
    }

    for (const SymbolMember_t &i : m_members) {
        auto &left_type = std::get<1>(i);
        AssertThrow(left_type != nullptr);

        bool right_member_found = false;

        for (const SymbolMember_t &j : other.m_members) {
            auto &right_type = std::get<1>(j);
            AssertThrow(right_type != nullptr);

            if (left_type == right_type || left_type->TypeEqual(*right_type)) {
                right_member_found = true;

                continue;
            }
        }

        if (!right_member_found) {
            // none found. return false.
            return false;
        }
    }

    return true;
}

bool SymbolType::TypeCompatible(
    const SymbolType &right,
    bool strict_numbers,
    bool strict_const
) const
{
    if (TypeEqual(*BuiltinTypes::UNDEFINED) || right.TypeEqual(*BuiltinTypes::UNDEFINED)) {
        return false;
    }

    if (TypeEqual(right)) {
        return true;
    }

    if (IsAnyType() || right.IsAnyType()) {
        return true;
    }

    if (IsProxyClass()) {
        // TODO:
        // have proxy class declare which class it is a proxy for,
        // then check that the types match?
        return true;
    }

    if (IsNullType()) {
        return right.IsNullableType();
    }

    if (right.IsNullType()) {
        return IsNullableType();
    }

    if (right.IsGenericParameter()) {
        // no substitution yet, compatible
        if (right.GetGenericParameterInfo().m_substitution.lock() == nullptr) {
            return true;
        }
    }

    switch (m_type_class) {
        case TYPE_ALIAS: {
            SymbolTypePtr_t sp = m_alias_info.m_aliasee.lock();
            AssertThrow(sp != nullptr);

            return sp->TypeCompatible(right, strict_numbers);
        }
        case TYPE_GENERIC: {
            if (right.m_type_class == TYPE_GENERIC || right.m_type_class == TYPE_GENERIC_INSTANCE) {
                if (auto right_base = right.GetBaseType()) {
                    if (TypeCompatible(*right_base, strict_numbers)) {
                        return true;
                    }
                }
            }

            return false;
        }
        case TYPE_GENERIC_INSTANCE: {
            SymbolTypePtr_t base = m_base.lock();
            AssertThrow(base != nullptr);

            if (right.m_type_class == TYPE_GENERIC_INSTANCE) {
                // check for compatibility between instances
                SymbolTypePtr_t other_base = right.GetBaseType();
                AssertThrow(other_base != nullptr);

                if (!TypeCompatible(*other_base, strict_numbers) && !base->TypeCompatible(*other_base, strict_numbers)) {
                    return false;
                } 

                // check all params
                if (m_generic_instance_info.m_generic_args.Size() != right.m_generic_instance_info.m_generic_args.Size()) {
                    return false;
                }

                // check each substituted parameter
                for (size_t i = 0; i < m_generic_instance_info.m_generic_args.Size(); i++) {
                    const SymbolTypePtr_t &param_type = m_generic_instance_info.m_generic_args[i].m_type;
                    const SymbolTypePtr_t &other_param_type = right.m_generic_instance_info.m_generic_args[i].m_type;

                    AssertThrow(param_type != nullptr);
                    AssertThrow(other_param_type != nullptr);

                    if (param_type == other_param_type) {
                        continue;
                    } else if (param_type->TypeEqual(*other_param_type)) {
                        continue;
                    } else if (param_type == BuiltinTypes::ANY || other_param_type == BuiltinTypes::ANY) {
                        continue;
                    } else {
                        return false;
                    }
                }

                return true;
            } else {
                // allow boxing/unboxing for 'Maybe(T)' type
                if (base->TypeEqual(*BuiltinTypes::MAYBE)) {
                    if (right.TypeEqual(*BuiltinTypes::NULL_TYPE)) {
                        return true;
                    } else {
                        const SymbolTypePtr_t &held_type = m_generic_instance_info.m_generic_args[0].m_type;
                        AssertThrow(held_type != nullptr);
                        return held_type->TypeCompatible(
                            right,
                            strict_numbers
                        );
                    }
                }
                // allow boxing/unboxing for 'Const(T)' type
                else if (base->TypeEqual(*BuiltinTypes::CONST_TYPE)) {
                    // strict_const means you can't assign a const (left) to non-const (right)
                    if (strict_const) {
                        return false;
                    }

                    const SymbolTypePtr_t &held_type = m_generic_instance_info.m_generic_args[0].m_type;
                    AssertThrow(held_type != nullptr);
                    return held_type->TypeCompatible(
                        right,
                        strict_numbers
                    );
                }

                return false;
            }

            break;
        }

        case TYPE_GENERIC_PARAMETER: {
            if (auto sp = m_generic_param_info.m_substitution.lock()) {
                return sp->TypeCompatible(right, strict_numbers);
            }

            // uninstantiated generic parameters are compatible with anything
            return true;
        }

        default:
            if (TypeEqual(*BuiltinTypes::NUMBER)) {
                return (right.TypeEqual(*BuiltinTypes::INT) ||
                        right.TypeEqual(*BuiltinTypes::UNSIGNED_INT) ||
                        right.TypeEqual(*BuiltinTypes::FLOAT));
            } else if (!strict_numbers) {
                if (TypeEqual(*BuiltinTypes::INT) || TypeEqual(*BuiltinTypes::UNSIGNED_INT) || TypeEqual(*BuiltinTypes::FLOAT)) {
                    return (right.TypeEqual(*BuiltinTypes::NUMBER) ||
                            right.TypeEqual(*BuiltinTypes::FLOAT) ||
                            right.TypeEqual(*BuiltinTypes::UNSIGNED_INT) ||
                            right.TypeEqual(*BuiltinTypes::INT));
                }
            }

            return false;
    }

    return true;
}

const SymbolTypePtr_t SymbolType::FindMember(const std::string &name) const
{
    for (const SymbolMember_t &member : m_members) {
        if (std::get<0>(member) == name) {
            return std::get<1>(member);
        }
    }

    return nullptr;
}

bool SymbolType::FindMember(const std::string &name, SymbolMember_t &out) const
{
    for (const SymbolMember_t &member : m_members) {
        if (std::get<0>(member) == name) {
            out = member;
            return true;
        }
    }

    return false;
}

bool SymbolType::FindMemberDeep(const std::string &name, SymbolMember_t &out) const
{
    if (FindMember(name, out)) {
        return true;
    }

    SymbolTypePtr_t base_ptr = GetBaseType();

    while (base_ptr != nullptr) {
        if (base_ptr->FindMember(name, out)) {
            return true;
        }

        base_ptr = base_ptr->GetBaseType();
    }

    return false;
}

const SymbolTypePtr_t SymbolType::FindPrototypeMember(const std::string &name) const
{
    if (SymbolTypePtr_t proto_type = FindMember("$proto")) {
        if (proto_type->IsAnyType()) {
            return BuiltinTypes::ANY;
        }

        return proto_type->FindMember(name);
    }

    return nullptr;
}


bool SymbolType::FindPrototypeMember(const std::string &name, SymbolMember_t &out) const
{
    if (SymbolTypePtr_t proto_type = FindMember("$proto")) {
        return proto_type->FindMember(name, out);
    }

    return false;
}

bool SymbolType::FindPrototypeMemberDeep(const std::string &name, SymbolMember_t &out) const
{
    if (FindPrototypeMember(name, out)) {
        return true;
    }

    SymbolTypePtr_t base_ptr = GetBaseType();

    while (base_ptr != nullptr) {
        if (base_ptr->FindPrototypeMember(name, out)) {
            return true;
        }

        base_ptr = base_ptr->GetBaseType();
    }

    return false;
}

bool SymbolType::FindPrototypeMember(const std::string &name, SymbolMember_t &out, UInt &out_index) const
{
    bool found = false;

    // for instance members (do it last, so it can be overridden by instances)
    if (SymbolTypePtr_t proto_type = FindMember("$proto")) {
        // get member index from name
        for (SizeType i = 0; i < proto_type->GetMembers().Size(); i++) {
            const SymbolMember_t &member = proto_type->GetMembers()[i];

            if (std::get<0>(member) == name) {
                // only set m_found_index if found in first level.
                // for members from base objects,
                // we load based on hash.
                out_index = UInt(i);
                out = member;

                found = true;

                break;
            }
        }
    }

    return found;
}

bool SymbolType::IsOrHasBase(const SymbolType &base_type) const
{
    return TypeEqual(base_type) || HasBase(base_type);
}

bool SymbolType::HasBase(const SymbolType &base_type) const
{
    if (SymbolTypePtr_t this_base = GetBaseType()) {
        if (this_base->TypeEqual(base_type)) {
            return true;
        } else {
            return this_base->HasBase(base_type);
        }
    }

    return false;
}

SymbolTypePtr_t SymbolType::GetUnaliased()
{
    if (m_type_class == TYPE_ALIAS) {
        if (const SymbolTypePtr_t aliasee = m_alias_info.m_aliasee.lock()) {
            if (aliasee.get() == this) {
                return aliasee;
            }

            return aliasee->GetUnaliased();
        }
    }

    return shared_from_this();
}

bool SymbolType::IsClass() const
{
    return IsOrHasBase(*BuiltinTypes::CLASS_TYPE);
}

bool SymbolType::IsObject() const
{
    return IsOrHasBase(*BuiltinTypes::OBJECT);
}

bool SymbolType::IsAnyType() const
{
    return IsOrHasBase(*BuiltinTypes::ANY) || IsOrHasBase(*BuiltinTypes::ANY_TYPE);
}

bool SymbolType::IsNullType() const
{
    return IsOrHasBase(*BuiltinTypes::NULL_TYPE);
}

bool SymbolType::IsNullableType() const
{
    return IsOrHasBase(*BuiltinTypes::OBJECT);
}

bool SymbolType::IsArrayType() const
{
    // compare directly to ARRAY type
    if (this == BuiltinTypes::ARRAY.get()) {
        return true;
    } else if (m_type_class == TYPE_GENERIC_INSTANCE) {
        // type is not Array, so check base class if it is a generic instance
        // e.g Array(Int)
        if (const SymbolTypePtr_t base = m_base.lock()) {
            if (base == BuiltinTypes::ARRAY || base == BuiltinTypes::VAR_ARGS) {
                return true;
            }
        }
    }

    return false;
}

bool SymbolType::IsBoxedType() const
{
    if (SymbolTypePtr_t base_type = GetBaseType()) {
        if (m_type_class == TYPE_GENERIC_INSTANCE) {
            return base_type->GetBaseType() == BuiltinTypes::BOXED_TYPE;
        } else if (m_type_class == TYPE_GENERIC) {
            return base_type == BuiltinTypes::BOXED_TYPE;
        }
    }

    return false;
}

bool SymbolType::IsGenericParameter() const
{
    if (m_type_class == TYPE_GENERIC_PARAMETER && m_generic_param_info.m_substitution.lock() == nullptr) {
        // right is a generic paramter that has not yet been substituted
        return true;
    }

    return false;
}

bool SymbolType::IsGeneric() const
{
    if (IsOrHasBase(*BuiltinTypes::GENERIC_VARIABLE_TYPE)) {
        return true;
    }

    if (GetTypeClass() == TYPE_GENERIC) {
        return true;
    }

    SymbolTypePtr_t top = GetBaseType();

    while (top != nullptr) {
        if (top->GetTypeClass() == TYPE_GENERIC) {
            return true;
        }

        top = top->GetBaseType();
    }
    
    return false;
}

bool SymbolType::IsPrimitive() const
{
    return IsOrHasBase(*BuiltinTypes::PRIMITIVE_TYPE);
}

SymbolTypePtr_t SymbolType::Alias(
    const std::string &name,
    const AliasTypeInfo &info
)
{
    if (auto sp = info.m_aliasee.lock()) {
        SymbolTypePtr_t res(new SymbolType(
            name,
            TYPE_ALIAS,
            nullptr
        ));

        res->m_alias_info = info;
        res->SetId(sp->GetId());

        return res;
    }

    return nullptr;
}

SymbolTypePtr_t SymbolType::Primitive(
    const std::string &name, 
    const RC<AstExpression> &default_value
)
{
    return SymbolTypePtr_t(new SymbolType(
        name,
        TYPE_BUILTIN,
        BuiltinTypes::PRIMITIVE_TYPE,
        default_value,
        {}
    ));
}

SymbolTypePtr_t SymbolType::Primitive(
    const std::string &name,
    const RC<AstExpression> &default_value,
    const SymbolTypePtr_t &base
)
{
    return SymbolTypePtr_t(new SymbolType(
        name,
        TYPE_BUILTIN,
        base,
        default_value,
        {}
    ));
}

SymbolTypePtr_t SymbolType::Object(
    const std::string &name,
    const Array<SymbolMember_t> &members
)
{
    SymbolTypePtr_t symbol_type(new SymbolType(
        name,
        TYPE_USER_DEFINED,
        BuiltinTypes::CLASS_TYPE,
        nullptr,
        members
    ));

    symbol_type->SetDefaultValue(RC<AstObject>(
        new AstObject(symbol_type, SourceLocation::eof)
    ));
    
    return symbol_type;
}

SymbolTypePtr_t SymbolType::Object(const std::string &name,
    const Array<SymbolMember_t> &members,
    const SymbolTypePtr_t &base)
{
    SymbolTypePtr_t symbol_type(new SymbolType(
        name,
        TYPE_USER_DEFINED,
        base,
        nullptr,
        members
    ));

    symbol_type->SetDefaultValue(RC<AstObject>(
        new AstObject(symbol_type, SourceLocation::eof)
    ));
    
    return symbol_type;
}

SymbolTypePtr_t SymbolType::Generic(const std::string &name,
    const Array<SymbolMember_t> &members, 
    const GenericTypeInfo &info,
    const SymbolTypePtr_t &base)
{
    SymbolTypePtr_t res(new SymbolType(
        name,
        TYPE_GENERIC,
        base,
        nullptr,
        members
    ));
    
    res->m_generic_info = info;
    
    return res;
}

SymbolTypePtr_t SymbolType::Generic(const std::string &name,
    const RC<AstExpression> &default_value,
    const Array<SymbolMember_t> &members, 
    const GenericTypeInfo &info,
    const SymbolTypePtr_t &base)
{
    SymbolTypePtr_t res(new SymbolType(
        name,
        TYPE_GENERIC,
        base,
        default_value,
        members
    ));
    
    res->m_generic_info = info;
    
    return res;
}

SymbolTypePtr_t SymbolType::Function(
    const SymbolTypePtr_t &return_type,
    const Array<GenericInstanceTypeInfo::Arg> &params
)
{
    Array<RC<AstParameter>> parameters; // TODO
    RC<AstBlock> block(new AstBlock(SourceLocation::eof));
    RC<AstFunctionExpression> value(new AstFunctionExpression(
        parameters, nullptr, block, SourceLocation::eof
    ));

    value->SetReturnType(return_type);

    Array<GenericInstanceTypeInfo::Arg> generic_param_types;
    generic_param_types.Reserve(params.Size() + 1);

    generic_param_types.PushBack({
        "@return", return_type
    });

    for (auto &it : params) {
        generic_param_types.PushBack(it);
    }

    auto function_type = GenericInstance(
        BuiltinTypes::FUNCTION,
        GenericInstanceTypeInfo{generic_param_types}
    );

    AssertThrow(function_type != nullptr);

    function_type->SetDefaultValue(value);

    return function_type;
}

SymbolTypePtr_t SymbolType::GenericInstance(
    const SymbolTypePtr_t &base,
    const GenericInstanceTypeInfo &info)
{
    AssertThrow(base != nullptr);
    AssertThrow(base->IsGeneric());

    std::string name;
    std::string return_type_name;
    bool has_return_type = false;

    if (info.m_generic_args.Any()) {
        if (base == BuiltinTypes::ARRAY) {
            const SymbolTypePtr_t &held_type = info.m_generic_args.Front().m_type;
            AssertThrow(held_type != nullptr);

            name = held_type->GetName() + "[]";
        } else if (base == BuiltinTypes::VAR_ARGS) {
            const SymbolTypePtr_t &held_type = info.m_generic_args.Front().m_type;
            AssertThrow(held_type != nullptr);

            name = held_type->GetName() + "...";
        } else {
            name = base->GetName() + "(";

            for (size_t i = 0; i < info.m_generic_args.Size(); i++) {
                const std::string &generic_arg_name = info.m_generic_args[i].m_name;
                const SymbolTypePtr_t &generic_arg_type = info.m_generic_args[i].m_type;

                AssertThrow(generic_arg_type != nullptr);

                if (generic_arg_name == "@return") {
                    has_return_type = true;
                    return_type_name = generic_arg_type->GetName();
                } else {
                    if (!generic_arg_name.empty()) {
                        name += generic_arg_name;
                        name += ": ";
                    }

                    name += generic_arg_type->GetName();
                    if (i != info.m_generic_args.Size() - 1) {
                        name += ", ";
                    }
                }
            }

            name += ")";

            if (has_return_type) {
                name += " -> " + return_type_name;
            }
        }
    } else {
        name = base->GetName() + "()";
    }

    Array<SymbolMember_t> members;
    members.Reserve(base->GetMembers().Size());

    for (const SymbolMember_t &member : base->GetMembers()) {
        bool is_substituted = false;

        if (std::get<1>(member)->GetTypeClass() == TYPE_GENERIC_PARAMETER) {
            // if members of the generic/template class are of the type T (generic parameter)
            // we need to make sure that the number of parameters supplied are equal.
            AssertThrow(base->GetGenericInfo().m_params.Size() == info.m_generic_args.Size());
            
            // find parameter and substitute it
            for (size_t i = 0; i < base->GetGenericInfo().m_params.Size(); i++) {
                SymbolTypePtr_t &it = base->GetGenericInfo().m_params[i];

                if (it->GetName() == std::get<1>(member)->GetName()) {
                    RC<AstExpression> default_value;

                    if ((default_value = std::get<2>(member))) {
                        default_value = CloneAstNode(default_value);
                    }

                    members.PushBack(SymbolMember_t(
                        std::get<0>(member),
                        info.m_generic_args[i].m_type,
                        default_value
                    ));

                    is_substituted = true;
                    break;
                }
            }

            if (!is_substituted) {
                // substitution error, set type to be undefined
                members.PushBack(SymbolMember_t(
                    std::get<0>(member),
                    BuiltinTypes::UNDEFINED,
                    std::get<2>(member)
                ));
            }
        } else {
            // push copy (clone assignment value)
            members.PushBack(SymbolMember_t(
                std::get<0>(member),
                std::get<1>(member),
                CloneAstNode(std::get<2>(member))
            ));
        }
    }

    // if the generic's default value is nullptr,
    // create a new default value for the instance of type AstObject
    // the reason we do this is so that a new 'Type' is generated for user-defined
    // generics, but built-in generics like Function and Array can play by
    // their own rules

    SymbolTypePtr_t res(new SymbolType(
        name,
        TYPE_GENERIC_INSTANCE,
        base,
        nullptr,
        members
    ));

    auto default_value = base->GetDefaultValue();

    res->SetId(base->GetId());
    res->SetDefaultValue(default_value);
    res->m_generic_instance_info = info;

    return res;
}

SymbolTypePtr_t SymbolType::GenericParameter(
    const std::string &name, 
    const SymbolTypePtr_t &substitution)
{
    SymbolTypePtr_t res(new SymbolType(
        name,
        TYPE_GENERIC_PARAMETER,
        BuiltinTypes::CLASS_TYPE
    ));

    res->m_generic_param_info.m_substitution = substitution;
    
    return res;
}

SymbolTypePtr_t SymbolType::Extend(
    const std::string &name,
    const SymbolTypePtr_t &base,
    const Array<SymbolMember_t> &members
)
{
    SymbolTypePtr_t symbol_type(new SymbolType(
        name,
        base->GetTypeClass() == TYPE_BUILTIN
            ? TYPE_USER_DEFINED
            : base->GetTypeClass(),
        base,
        base->GetDefaultValue(),
        members
    ));

    symbol_type->SetDefaultValue(RC<AstObject>(
        new AstObject(symbol_type, SourceLocation::eof)
    ));
    
    return symbol_type;
}

SymbolTypePtr_t SymbolType::Extend(
    const SymbolTypePtr_t &base,
    const Array<SymbolMember_t> &members)
{
    SymbolTypePtr_t symbol_type(new SymbolType(
        base->GetName(),
        base->GetTypeClass() == TYPE_BUILTIN
            ? TYPE_USER_DEFINED
            : base->GetTypeClass(),
        base,
        base->GetDefaultValue(),
        members
    ));

    symbol_type->SetDefaultValue(RC<AstObject>(
        new AstObject(symbol_type, SourceLocation::eof)
    ));
    
    return symbol_type;
}
    
SymbolTypePtr_t PrototypedObject(
    const std::string &name,
    const SymbolTypePtr_t &base,
    const Array<SymbolMember_t> &prototype_members
)
{
    return SymbolType::Extend(
        name,
        base,
        Array<SymbolMember_t> {
            SymbolMember_t {
                "$proto",
                SymbolType::Object(
                    name + "Instance",
                    prototype_members
                ),
                nullptr
            }
        }
    );
}

SymbolTypePtr_t SymbolType::TypePromotion(
    const SymbolTypePtr_t &lptr,
    const SymbolTypePtr_t &rptr,
    bool use_number)
{
    if (lptr == nullptr || rptr == nullptr) {
        return nullptr;
    }

    // compare pointer values
    if (lptr == rptr || lptr->TypeEqual(*rptr)) {
        return lptr;
    }

    if (lptr->TypeEqual(*BuiltinTypes::UNDEFINED) ||
        rptr->TypeEqual(*BuiltinTypes::UNDEFINED))
    {
        return BuiltinTypes::UNDEFINED;
    } else if (lptr->TypeEqual(*BuiltinTypes::ANY) || rptr->TypeEqual(*BuiltinTypes::ANY)) {
        // Any + T = Any
        // T + Any = Any
        return BuiltinTypes::ANY;
    } else if (lptr->TypeEqual(*BuiltinTypes::NUMBER)) {
        return rptr->TypeEqual(*BuiltinTypes::INT) ||
               rptr->TypeEqual(*BuiltinTypes::FLOAT) ||
               rptr->TypeEqual(*BuiltinTypes::UNSIGNED_INT)
               ? BuiltinTypes::NUMBER
               : BuiltinTypes::UNDEFINED;
    } else if (lptr->TypeEqual(*BuiltinTypes::INT)) {
        if (rptr->TypeEqual(*BuiltinTypes::UNSIGNED_INT)) {
            return BuiltinTypes::UNSIGNED_INT;
        }

        return rptr->TypeEqual(*BuiltinTypes::NUMBER) || rptr->TypeEqual(*BuiltinTypes::FLOAT)
               ? (use_number ? BuiltinTypes::NUMBER : rptr)
               : BuiltinTypes::UNDEFINED;
    } else if (lptr->TypeEqual(*BuiltinTypes::FLOAT)) {
        return rptr->TypeEqual(*BuiltinTypes::NUMBER) ||
               rptr->TypeEqual(*BuiltinTypes::INT) ||
               rptr->TypeEqual(*BuiltinTypes::UNSIGNED_INT)
               ? (use_number ? BuiltinTypes::NUMBER : lptr)
               : BuiltinTypes::UNDEFINED;
    } else if (rptr->TypeEqual(*BuiltinTypes::NUMBER)) {
        return lptr->TypeEqual(*BuiltinTypes::INT) ||
               lptr->TypeEqual(*BuiltinTypes::FLOAT) ||
               rptr->TypeEqual(*BuiltinTypes::UNSIGNED_INT)
               ? BuiltinTypes::NUMBER
               : BuiltinTypes::UNDEFINED;
    } else if (rptr->TypeEqual(*BuiltinTypes::INT)) {
        if (lptr->TypeEqual(*BuiltinTypes::UNSIGNED_INT)) {
            return BuiltinTypes::UNSIGNED_INT;
        }

        return lptr->TypeEqual(*BuiltinTypes::NUMBER) ||
               lptr->TypeEqual(*BuiltinTypes::FLOAT)
               ? (use_number ? BuiltinTypes::NUMBER : lptr)
               : BuiltinTypes::UNDEFINED;
    } else if (rptr->TypeEqual(*BuiltinTypes::FLOAT)) {
        return lptr->TypeEqual(*BuiltinTypes::NUMBER) ||
               lptr->TypeEqual(*BuiltinTypes::INT) ||
               lptr->TypeEqual(*BuiltinTypes::UNSIGNED_INT)
               ? (use_number ? BuiltinTypes::NUMBER : rptr)
               : BuiltinTypes::UNDEFINED;
    }

    return BuiltinTypes::UNDEFINED;
}

SymbolTypePtr_t SymbolType::GenericPromotion(
    const SymbolTypePtr_t &lptr,
    const SymbolTypePtr_t &rptr)
{
    AssertThrow(lptr != nullptr);
    AssertThrow(rptr != nullptr);

    switch (lptr->GetTypeClass()) {
        case TYPE_GENERIC:
            switch (rptr->GetTypeClass()) {
                case TYPE_GENERIC_INSTANCE: {
                    auto right_base = rptr->GetBaseType();

                    while (right_base != nullptr) {
                        if (lptr->TypeEqual(*right_base)) {
                            // left-hand side is the base of the right hand side,
                            // so upgrade left to the derived type.
                            return rptr;
                        }
                        right_base = right_base->GetBaseType();
                    }
                }
                    // fallthrough
                default:
                    // check if left is a Boxed type so we can upgrade the inner type
                    if (auto left_base = lptr->GetBaseType()) {
                        if (left_base == BuiltinTypes::BOXED_TYPE) {
                            // if left is a Boxed type and still generic (no params),
                            // e.i Const or Maybe, fill it with the assignment type
                            Array<GenericInstanceTypeInfo::Arg> generic_types {
                                { "", rptr }
                            };
                            
                            return SymbolType::GenericInstance(
                                lptr,
                                GenericInstanceTypeInfo {
                                    generic_types
                                }
                            );
                        }
                    }

                    break;
            }

            break;
        
        case TYPE_GENERIC_INSTANCE: {
            if (lptr->IsBoxedType()) {
                // if left is a Boxed type and generic instance,
                // perform promotion on the inner type.
                const SymbolTypePtr_t &inner_type = lptr->GetGenericInstanceInfo().m_generic_args[0].m_type;
                AssertThrow(inner_type != nullptr);

                Array<GenericInstanceTypeInfo::Arg> new_generic_types {
                    { "", SymbolType::GenericPromotion(inner_type, rptr) }
                };
                
                return SymbolType::GenericInstance(
                    lptr->GetBaseType(),
                    GenericInstanceTypeInfo {
                        new_generic_types
                    }
                );
            }

            break;
        }
    }

    // no promotion
    return lptr;
}

SymbolTypePtr_t SymbolType::SubstituteGenericParams(
    const SymbolTypePtr_t &lptr,
    const SymbolTypePtr_t &placeholder,
    const SymbolTypePtr_t &substitute)
{
    AssertThrow(lptr != nullptr);
    AssertThrow(placeholder != nullptr);
    AssertThrow(substitute != nullptr);

    if (lptr->TypeEqual(*placeholder)) {
        return substitute;
    }

    switch (lptr->GetTypeClass()) {
        case TYPE_GENERIC_INSTANCE: {
            SymbolTypePtr_t base_type = lptr->GetBaseType();
            AssertThrow(base_type != nullptr);

            Array<GenericInstanceTypeInfo::Arg> new_generic_types;

            for (const GenericInstanceTypeInfo::Arg &arg : lptr->GetGenericInstanceInfo().m_generic_args) {
                const SymbolTypePtr_t &arg_type = arg.m_type;
                AssertThrow(arg_type != nullptr);

                GenericInstanceTypeInfo::Arg new_arg;
                new_arg.m_name = arg.m_name;
                new_arg.m_default_value = arg.m_default_value;

                // perform substitution
                SymbolTypePtr_t arg_type_substituted = SymbolType::SubstituteGenericParams(
                    arg_type,
                    placeholder,
                    substitute
                );

                AssertThrow(arg_type_substituted != nullptr);
                new_arg.m_type = arg_type_substituted;

                new_generic_types.PushBack(new_arg);
            }

            return SymbolType::GenericInstance(
                base_type,
                GenericInstanceTypeInfo {
                    new_generic_types
                }
            );

            break;
        }
    }

    return lptr;
}

} // namespace hyperion::compiler
