#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>

#include <core/containers/FlatSet.hpp>

#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

namespace hyperion::compiler {

SymbolType::SymbolType(
    const String& name,
    SymbolTypeClass typeClass,
    const SymbolTypeRef& base)
    : m_name(name),
      m_typeClass(typeClass),
      m_defaultValue(nullptr),
      m_base(base),
      m_flags(SYMBOL_TYPE_FLAGS_NONE),
      m_declScope(nullptr)
{
}

SymbolType::SymbolType(
    const String& name,
    SymbolTypeClass typeClass,
    const SymbolTypeRef& base,
    const RC<AstExpression>& defaultValue,
    const Array<SymbolTypeMember>& members,
    const Array<SymbolTypeMember>& staticMembers)
    : m_name(name),
      m_typeClass(typeClass),
      m_defaultValue(defaultValue),
      m_members(members),
      m_staticMembers(staticMembers),
      m_base(base),
      m_flags(SYMBOL_TYPE_FLAGS_NONE),
      m_declScope(nullptr)
{
}

bool SymbolType::TypeEqual(const SymbolType& other) const
{
    if (std::addressof(other) == this)
    {
        return true;
    }

    if (IsAlias() || other.IsAlias())
    {
        return GetUnaliased()->TypeEqual(*other.GetUnaliased());
    }

    return GetHashCode() == other.GetHashCode();

    if (m_name != other.m_name)
    {
        return false;
    }

    if (m_typeClass != other.m_typeClass)
    {
        return false;
    }

    switch (m_typeClass)
    {
    case TYPE_BUILTIN:
        return true;
    case TYPE_ALIAS:
        if (SymbolTypeRef sp = m_aliasInfo.m_aliasee.Lock())
        {
            return *sp == other;
        }

        return false;

    case TYPE_GENERIC_PARAMETER:
        return true;

    case TYPE_GENERIC_INSTANCE:
    {
        if (m_genericInstanceInfo.m_genericArgs.Size() != other.m_genericInstanceInfo.m_genericArgs.Size())
        {
            return false;
        }

        SymbolTypeRef base = m_base;
        Assert(base != nullptr);

        // check for compatibility between instances
        SymbolTypeRef otherBase = other.GetBaseType();
        Assert(otherBase != nullptr);

        if (!base->TypeEqual(*otherBase))
        {
            return false;
        }

        // check all params
        if (m_genericInstanceInfo.m_genericArgs.Size() != other.m_genericInstanceInfo.m_genericArgs.Size())
        {
            return false;
        }

        // check each substituted parameter
        for (SizeType i = 0; i < m_genericInstanceInfo.m_genericArgs.Size(); i++)
        {
            const SymbolTypeRef& instanceArgType = m_genericInstanceInfo.m_genericArgs[i].m_type;
            const SymbolTypeRef& otherArgType = other.m_genericInstanceInfo.m_genericArgs[i].m_type;

            Assert(instanceArgType != nullptr);
            Assert(otherArgType != nullptr);

            if (!instanceArgType->TypeEqual(*otherArgType))
            {
                return false; // have to do this for now to prevent infinte recursion
            }
        }

        break;
    }
    default:
        break;
    }

    // early out if members are not equal
    if (m_members.Size() != other.m_members.Size())
    {
        return false;
    }

    for (const SymbolTypeMember& leftMember : m_members)
    {
        const SymbolTypeRef& leftMemberType = leftMember.type;
        Assert(leftMemberType != nullptr);

        SymbolTypeMember rightMember;

        if (!other.FindMember(leftMember.name, rightMember))
        {
            // DebugLog(LogType::Debug,
            //     "SymbolType::TypeEqual: member '%s' not found in other type '%s'\n",
            //     leftMember.name.Data(),
            //     other.m_name.Data());

            return false;
        }

        Assert(rightMember.type != nullptr);

        // DebugLog(LogType::Debug,
        //     "SymbolType::TypeEqual: check member '%s' type '%s' == '%s'\n",
        //     leftMember.name.Data(),
        //     leftMemberType->ToString(true).Data(),
        //     rightMember.type->ToString(true).Data());

        if (!rightMember.type->TypeEqual(*leftMemberType))
        {
            // DebugLog(LogType::Debug,
            //     "SymbolType::TypeEqual: member '%s' type '%s' != '%s'\n",
            //     leftMember.name.Data(),
            //     leftMemberType->ToString(true).Data(),
            //     rightMember.type->ToString(true).Data());

            return false;
        }
    }

    return true;
}

bool SymbolType::TypeCompatible(
    const SymbolType& right,
    bool strictNumbers) const
{
    if (TypeEqual(*BuiltinTypes::UNDEFINED) || right.TypeEqual(*BuiltinTypes::UNDEFINED))
    {
        return false;
    }

    if (TypeEqual(right))
    {
        return true;
    }

    // check object inheritance (left is base of right)
    if (IsObject() && right.IsObject())
    {
        SymbolTypeRef base = right.GetBaseType();
        while (base != nullptr)
        {
            if (TypeEqual(*base))
            {
                return true;
            }

            base = base->GetBaseType();
        }

        return false;
    }

    if (IsAnyType() || right.IsAnyType())
    {
        return true;
    }

    // if (IsProxyClass()) {
    //     // TODO:
    //     // have proxy class declare which class it is a proxy for,
    //     // then check that the types match?
    //     return true;
    // }

    if (IsNullType())
    {
        return right.IsNullableType();
    }

    if (right.IsNullType())
    {
        return IsNullableType();
    }

    // if (IsGenericParameter() || right.IsGenericParameter())
    // {
    //     // no substitution yet, compatible
    //     return true;
    // }

    switch (m_typeClass)
    {
    case TYPE_ALIAS:
    {
        SymbolTypeRef sp = m_aliasInfo.m_aliasee.Lock();
        Assert(sp != nullptr);

        return sp->TypeCompatible(right, strictNumbers);
    }
    case TYPE_GENERIC_INSTANCE:
    {
        SymbolTypeRef base = m_base;
        Assert(base != nullptr);

        if (right.m_typeClass == TYPE_GENERIC_INSTANCE)
        {
            // check for compatibility between instances
            SymbolTypeRef otherBase = right.GetBaseType();
            Assert(otherBase != nullptr);

            if (!base->TypeEqual(*otherBase))
            {
                return false;
            }

            // check all params
            if (m_genericInstanceInfo.m_genericArgs.Size() != right.m_genericInstanceInfo.m_genericArgs.Size())
            {
                return false;
            }

            // check each substituted parameter
            for (SizeType i = 0; i < m_genericInstanceInfo.m_genericArgs.Size(); i++)
            {
                const SymbolTypeRef& paramType = m_genericInstanceInfo.m_genericArgs[i].m_type;
                const SymbolTypeRef& otherParamType = right.m_genericInstanceInfo.m_genericArgs[i].m_type;

                Assert(paramType != nullptr);
                Assert(otherParamType != nullptr);

                if (!paramType->TypeEqual(*otherParamType))
                {
                    return false;
                }
            }

            // check members
            if (m_members.Size() != right.m_members.Size())
            {
                return false;
            }

            for (const SymbolTypeMember& leftMember : m_members)
            {
                const SymbolTypeRef& leftMemberType = leftMember.type;
                Assert(leftMemberType != nullptr);

                SymbolTypeMember rightMember;

                if (!right.FindMember(leftMember.name, rightMember))
                {
                    return false;
                }

                Assert(rightMember.type != nullptr);

                if (!rightMember.type->TypeEqual(*leftMemberType))
                {
                    return false;
                }
            }

            // check static members
            if (m_staticMembers.Size() != right.m_staticMembers.Size())
            {
                return false;
            }

            for (const SymbolTypeMember& leftMember : m_staticMembers)
            {
                const SymbolTypeRef& leftMemberType = leftMember.type;
                Assert(leftMemberType != nullptr);

                SymbolTypeMember rightMember;

                if (!right.FindMember(leftMember.name, rightMember))
                {
                    return false;
                }

                Assert(rightMember.type != nullptr);

                if (!rightMember.type->TypeEqual(*leftMemberType))
                {
                    return false;
                }
            }

            return true;
        }
        else
        {
            return false;
        }

        break;
    }
    default:
        if (!strictNumbers && IsNumber() && right.IsNumber())
        {
            return true;
        }

        return false;
    }

    return false;
}

SymbolTypeRef SymbolType::FindMember(const String& name) const
{
    for (const SymbolTypeMember& member : m_members)
    {
        if (member.name == name)
        {
            return member.type;
        }
    }

    return nullptr;
}

bool SymbolType::FindMember(const String& name, SymbolTypeMember& out) const
{
    for (const SymbolTypeMember& member : m_members)
    {
        if (member.name == name)
        {
            out = member;
            return true;
        }
    }

    return false;
}

bool SymbolType::FindMember(const String& name, SymbolTypeMember& out, uint32& outIndex) const
{
    // get member index from name
    for (SizeType i = 0; i < m_members.Size(); i++)
    {
        const SymbolTypeMember& member = m_members[i];

        if (member.name == name)
        {
            // only set m_foundIndex if found in first level.
            // for members from base objects,
            // we load based on hash.
            outIndex = uint32(i);
            out = member;

            return true;
        }
    }

    return false;
}

SymbolTypeRef SymbolType::FindMemberDeep(const String& name) const
{
    SymbolTypeMember out;
    uint32 outIndex;
    uint32 outDepth;

    if (FindMemberDeep(name, out, outIndex, outDepth))
    {
        return out.type;
    }

    return nullptr;
}

bool SymbolType::FindMemberDeep(const String& name, SymbolTypeMember& out) const
{
    uint32 outIndex;
    uint32 outDepth;

    return FindMemberDeep(name, out, outIndex, outDepth);
}

bool SymbolType::FindMemberDeep(const String& name, SymbolTypeMember& out, uint32& outIndex) const
{
    uint32 outDepth;

    return FindMemberDeep(name, out, outIndex, outDepth);
}

bool SymbolType::FindMemberDeep(const String& name, SymbolTypeMember& out, uint32& outIndex, uint32& outDepth) const
{
    outDepth = 0;

    if (FindMember(name, out, outIndex))
    {
        return true;
    }

    outDepth++;

    SymbolTypeRef basePtr = GetBaseType();

    while (basePtr != nullptr)
    {
        if (basePtr->FindMember(name, out, outIndex))
        {
            return true;
        }

        basePtr = basePtr->GetBaseType();

        outDepth++;
    }

    return false;
}

SymbolTypeRef SymbolType::FindPrototypeMember(const String& name) const
{
    if (SymbolTypeRef protoType = FindMember("$proto"))
    {
        if (protoType->IsAnyType())
        {
            return BuiltinTypes::ANY;
        }

        return protoType->FindMember(name);
    }

    return nullptr;
}

bool SymbolType::FindPrototypeMember(const String& name, SymbolTypeMember& out) const
{
    if (SymbolTypeRef protoType = FindMember("$proto"))
    {
        return protoType->FindMember(name, out);
    }

    return false;
}

bool SymbolType::FindPrototypeMemberDeep(const String& name) const
{
    SymbolTypeMember out;

    return FindPrototypeMemberDeep(name, out);
}

bool SymbolType::FindPrototypeMemberDeep(const String& name, SymbolTypeMember& out) const
{
    if (FindPrototypeMember(name, out))
    {
        return true;
    }

    SymbolTypeRef basePtr = GetBaseType();

    while (basePtr != nullptr)
    {
        if (basePtr->FindPrototypeMember(name, out))
        {
            return true;
        }

        basePtr = basePtr->GetBaseType();
    }

    return false;
}

bool SymbolType::FindPrototypeMember(const String& name, SymbolTypeMember& out, uint32& outIndex) const
{
    bool found = false;

    // for instance members (do it last, so it can be overridden by instances)
    if (SymbolTypeRef protoType = FindMember("$proto"))
    {
        // get member index from name
        for (SizeType i = 0; i < protoType->GetMembers().Size(); i++)
        {
            const SymbolTypeMember& member = protoType->GetMembers()[i];

            if (member.name == name)
            {
                // only set m_foundIndex if found in first level.
                // for members from base objects,
                // we load based on hash.
                outIndex = uint32(i);
                out = member;

                found = true;

                break;
            }
        }
    }

    return found;
}

bool SymbolType::HasTrait(const SymbolTypeTrait& trait) const
{
    SymbolTypeMember member;

    // trait names are prefixed with '@'
    if (FindMember(trait.name, member))
    {
        return true;
    }

    return false;
}

bool SymbolType::HasTraitDeep(const SymbolTypeTrait& trait) const
{
    SymbolTypeMember member;

    if (FindMemberDeep(trait.name, member))
    {
        return true;
    }

    return false;
}

bool SymbolType::IsOrHasBase(const SymbolType& baseType) const
{
    return TypeEqual(baseType) || HasBase(baseType);
}

bool SymbolType::HasBase(const SymbolType& baseType) const
{
    if (SymbolTypeRef thisBase = GetBaseType())
    {
        if (thisBase->TypeEqual(baseType))
        {
            return true;
        }
        else
        {
            return thisBase->HasBase(baseType);
        }
    }

    return false;
}

SymbolTypeRef SymbolType::GetUnaliased() const
{
    if (m_typeClass == TYPE_ALIAS)
    {
        if (SymbolTypeRef aliasee = m_aliasInfo.m_aliasee.Lock())
        {
            if (aliasee.Get() == this)
            {
                return aliasee;
            }

            return aliasee->GetUnaliased();
        }
    }

    return RefCountedPtrFromThis();
}

bool SymbolType::IsNumber() const
{
    return IsOrHasBase(*BuiltinTypes::INT)
        || IsOrHasBase(*BuiltinTypes::UNSIGNED_INT)
        || IsOrHasBase(*BuiltinTypes::FLOAT);
}

bool SymbolType::IsIntegral() const
{
    return IsOrHasBase(*BuiltinTypes::INT)
        || IsOrHasBase(*BuiltinTypes::UNSIGNED_INT);
}

bool SymbolType::IsSignedIntegral() const
{
    return IsOrHasBase(*BuiltinTypes::INT);
}

bool SymbolType::IsUnsignedIntegral() const
{
    return IsOrHasBase(*BuiltinTypes::UNSIGNED_INT);
}

bool SymbolType::IsFloat() const
{
    return IsOrHasBase(*BuiltinTypes::FLOAT);
}

bool SymbolType::IsBoolean() const
{
    return IsOrHasBase(*BuiltinTypes::BOOLEAN);
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
    return IsOrHasBase(*BuiltinTypes::ANY);
}

bool SymbolType::IsPlaceholderType() const
{
    return IsOrHasBase(*BuiltinTypes::PLACEHOLDER);
}

bool SymbolType::IsNullType() const
{
    return IsOrHasBase(*BuiltinTypes::NULL_TYPE);
}

bool SymbolType::IsNullableType() const
{
    return IsOrHasBase(*BuiltinTypes::OBJECT);
}

bool SymbolType::IsVarArgsType() const
{
    return HasTraitDeep(BuiltinTypeTraits::variadic);
}

bool SymbolType::IsGenericParameter() const
{
    return m_typeClass == TYPE_GENERIC_PARAMETER;
}

bool SymbolType::IsGenericInstanceType() const
{
    return m_typeClass == TYPE_GENERIC_INSTANCE;
}

bool SymbolType::IsPrimitive() const
{
    return IsOrHasBase(*BuiltinTypes::PRIMITIVE_TYPE);
}

bool SymbolType::IsEnumType() const
{
    return IsOrHasBase(*BuiltinTypes::ENUM_TYPE);
}

SymbolTypeRef SymbolType::Alias(
    const String& name,
    const AliasTypeInfo& info)
{
    if (auto sp = info.m_aliasee.Lock())
    {
        SymbolTypeRef res(new SymbolType(
            name,
            TYPE_ALIAS,
            nullptr));

        res->m_aliasInfo = info;

        return res;
    }

    return nullptr;
}

SymbolTypeRef SymbolType::Placeholder(
    const String& name)
{
    return SymbolTypeRef(new SymbolType(
        name,
        TYPE_PLACEHOLDER,
        BuiltinTypes::PLACEHOLDER));
}

SymbolTypeRef SymbolType::Primitive(
    const String& name,
    const RC<AstExpression>& defaultValue)
{
    return SymbolTypeRef(new SymbolType(
        name,
        TYPE_BUILTIN,
        BuiltinTypes::PRIMITIVE_TYPE,
        defaultValue,
        {}, {}));
}

SymbolTypeRef SymbolType::Object(
    const String& name,
    const SymbolTypeRef& base,
    const Array<SymbolTypeMember>& members,
    const Array<SymbolTypeMember>& staticMembers)
{
    SymbolTypeRef symbolType(new SymbolType(
        name,
        TYPE_USER_DEFINED,
        base,
        nullptr,
        members,
        staticMembers));

    return symbolType;
}

String SymbolType::ToString(bool includeParameterNames) const
{
    String res = m_name;

    if (SymbolTypeRef sp = m_aliasInfo.m_aliasee.Lock())
    {
        res += " (aka " + sp->ToString() + ")";
    }

    switch (m_typeClass)
    {
    case TYPE_ALIAS:
    case TYPE_BUILTIN: // fallthrough
    case TYPE_USER_DEFINED:
    case TYPE_GENERIC_PARAMETER:
        break;
    case TYPE_GENERIC_INSTANCE:
    {
        res = m_name;

        const GenericInstanceTypeInfo& info = m_genericInstanceInfo;

        if (info.m_genericArgs.Any())
        {
            if (IsVarArgsType())
            {
                const SymbolTypeRef& heldType = info.m_genericArgs.Front().m_type;
                Assert(heldType != nullptr);

                return heldType->ToString() + "...";
            }
            else
            {
                res += "<";

                bool hasReturnType = false;
                String returnTypeName;

                for (SizeType i = 0; i < info.m_genericArgs.Size(); i++)
                {
                    const String& genericArgName = info.m_genericArgs[i].m_name;
                    const SymbolTypeRef& genericArgType = info.m_genericArgs[i].m_type;

                    Assert(genericArgType != nullptr);

                    if (genericArgName == "@return")
                    {
                        hasReturnType = true;
                        returnTypeName = genericArgType->ToString();
                    }
                    else
                    {
                        if (info.m_genericArgs[i].m_isConst)
                        {
                            res += "const ";
                        }

                        if (info.m_genericArgs[i].m_isRef)
                        {
                            res += "ref ";
                        }

                        if (includeParameterNames && genericArgName.Any())
                        {
                            res += genericArgName;
                            res += ": ";
                        }

                        res += genericArgType->ToString(includeParameterNames);

                        if (i != info.m_genericArgs.Size() - 1)
                        {
                            res += ", ";
                        }
                    }
                }

                res += ">";

                if (hasReturnType)
                {
                    res += " -> " + returnTypeName;
                }
            }
        }

        break;
    }
    default:
        break;
    }

    return res;
}

SymbolTypeRef SymbolType::SymbolType::Generic(
    const String& name,
    const Array<SymbolTypeMember>& members,
    const Array<SymbolTypeMember>& staticMembers,
    const GenericInstanceTypeInfo& info)
{
    return GenericInstance(
        name,
        nullptr,
        members,
        staticMembers,
        info);
}

SymbolTypeRef SymbolType::SymbolType::Generic(
    const String& name,
    const SymbolTypeRef& baseType,
    const Array<SymbolTypeMember>& members,
    const Array<SymbolTypeMember>& staticMembers,
    const GenericInstanceTypeInfo& info)
{
    SymbolTypeRef genericInstance = GenericInstance(
        name,
        nullptr,
        members,
        staticMembers,
        info);

    if (!genericInstance)
    {
        return nullptr;
    }

    genericInstance->SetBaseType(baseType);

    return genericInstance;
}

SymbolTypeRef SymbolType::GenericInstance(
    const SymbolTypeRef& genericType,
    const Array<SymbolTypeMember>& members,
    const Array<SymbolTypeMember>& staticMembers,
    const GenericInstanceTypeInfo& info)
{
    Assert(genericType != nullptr && genericType->IsGenericInstanceType());

    return GenericInstance(
        genericType->GetName(),
        genericType,
        members,
        staticMembers,
        info);
}

SymbolTypeRef SymbolType::GenericInstance(
    const String& name,
    const SymbolTypeRef& genericType,
    const Array<SymbolTypeMember>& members,
    const Array<SymbolTypeMember>& staticMembers,
    const GenericInstanceTypeInfo& info)
{
    Array<SymbolTypeMember> allMembers;
    allMembers.Reserve((genericType ? genericType->GetMembers().Size() : 0) + members.Size());

    Array<SymbolTypeMember> allStaticMembers;
    allStaticMembers.Reserve((genericType ? genericType->GetStaticMembers().Size() : 0) + staticMembers.Size());

    SymbolTypeRef baseType = genericType ? genericType->GetBaseType() : nullptr;
    SymbolTypeRef currentTarget = genericType;

    while (currentTarget != nullptr)
    {
        for (const SymbolTypeMember& member : currentTarget->GetMembers())
        {
            const auto overridenMemberIt = allMembers.FindIf([&member](const auto& otherMember)
                {
                    return otherMember.name == member.name;
                });

            if (overridenMemberIt != allMembers.End())
            {
                // if member is overriden, skip it
                continue;
            }

            // push copy (clone assignment value)
            allMembers.PushBack(SymbolTypeMember {
                member.name,
                member.type,
                CloneAstNode(member.expr) });
        }

        for (const SymbolTypeMember& staticMember : currentTarget->GetStaticMembers())
        {
            const auto overridenStaticMemberIt = allStaticMembers.FindIf([&staticMember](const auto& otherMember)
                {
                    return otherMember.name == staticMember.name;
                });

            if (overridenStaticMemberIt != allStaticMembers.End())
            {
                // if member is overriden, skip it
                continue;
            }

            // push copy (clone assignment value)
            allStaticMembers.PushBack(SymbolTypeMember {
                staticMember.name,
                staticMember.type,
                CloneAstNode(staticMember.expr) });
        }

        currentTarget = currentTarget->GetBaseType();
    }

    // add/override with members from this instance
    for (const SymbolTypeMember& member : members)
    {
        const auto overridenMemberIt = allMembers.FindIf([&member](const auto& otherMember)
            {
                return otherMember.name == member.name;
            });

        if (overridenMemberIt != allMembers.End())
        {
            // if member is overriden, skip it
            *overridenMemberIt = member;
        }
        else
        {
            // push copy (clone assignment value)
            allMembers.PushBack(SymbolTypeMember {
                member.name,
                member.type,
                CloneAstNode(member.expr) });
        }
    }

    // add/override with static members from this instance
    for (const SymbolTypeMember& staticMember : staticMembers)
    {
        const auto overridenStaticMemberIt = allStaticMembers.FindIf([&staticMember](const auto& otherMember)
            {
                return otherMember.name == staticMember.name;
            });

        if (overridenStaticMemberIt != allStaticMembers.End())
        {
            // if member is overriden, skip it
            *overridenStaticMemberIt = staticMember;
        }
        else
        {
            // push copy (clone assignment value)
            allStaticMembers.PushBack(SymbolTypeMember {
                staticMember.name,
                staticMember.type,
                CloneAstNode(staticMember.expr) });
        }
    }

    SymbolTypeRef res(new SymbolType(
        name,
        TYPE_GENERIC_INSTANCE,
        baseType,
        nullptr,
        allMembers,
        allStaticMembers));

    RC<AstExpression> defaultValue = genericType ? CloneAstNode(genericType->GetDefaultValue()) : nullptr;
    res->SetDefaultValue(defaultValue);
    res->SetFlags(genericType ? genericType->GetFlags() : SYMBOL_TYPE_FLAGS_NONE);
    res->m_genericInstanceInfo = info;

    return res;
}

SymbolTypeRef SymbolType::GenericParameter(
    const String& name)
{
    return SymbolTypeRef(new SymbolType(name, TYPE_GENERIC_PARAMETER, BuiltinTypes::CLASS_TYPE));
}

SymbolTypeRef SymbolType::Extend(
    const String& name,
    const SymbolTypeRef& base,
    const Array<SymbolTypeMember>& members,
    const Array<SymbolTypeMember>& staticMembers)
{
    Assert(base != nullptr);

    SymbolTypeRef symbolType(new SymbolType(
        name,
        base->GetTypeClass() == TYPE_BUILTIN
            ? TYPE_USER_DEFINED
            : base->GetTypeClass(),
        base,
        base->GetDefaultValue(),
        members,
        staticMembers));

    symbolType->m_genericInstanceInfo = base->m_genericInstanceInfo;
    symbolType->m_genericParamInfo = base->m_genericParamInfo;
    symbolType->m_functionInfo = base->m_functionInfo;

    return symbolType;
}

SymbolTypeRef SymbolType::TypePromotion(const SymbolTypeRef& lptr, const SymbolTypeRef& rptr)
{
    if (lptr == nullptr || rptr == nullptr)
    {
        return nullptr;
    }

    // compare pointer values
    if (lptr == rptr || lptr->TypeEqual(*rptr))
    {
        return lptr;
    }

    if (lptr->TypeEqual(*BuiltinTypes::UNDEFINED) || rptr->TypeEqual(*BuiltinTypes::UNDEFINED))
    {
        return BuiltinTypes::UNDEFINED;
    }
    else if (lptr->IsAnyType() || rptr->IsAnyType())
    {
        // Any + T = Any
        // T + Any = Any
        return BuiltinTypes::ANY;
    }
    else if (lptr->IsNumber() && rptr->IsNumber())
    {
        if (lptr->IsFloat() || rptr->IsFloat())
        {
            return BuiltinTypes::FLOAT;
        }
        else if (lptr->IsUnsignedIntegral() || rptr->IsUnsignedIntegral())
        {
            return BuiltinTypes::UNSIGNED_INT;
        }
        else
        {
            return BuiltinTypes::INT;
        }
    }

    // @TODO Check for common base

    return BuiltinTypes::UNDEFINED;
}

SymbolTypeRef SymbolType::SubstituteGenericParams(
    const SymbolTypeRef& lptr,
    const SymbolTypeRef& placeholder,
    const SymbolTypeRef& substitute)
{
    Assert(lptr != nullptr);
    Assert(placeholder != nullptr);
    Assert(substitute != nullptr);

    if (lptr->TypeEqual(*placeholder))
    {
        return substitute;
    }

    switch (lptr->GetTypeClass())
    {
    case TYPE_GENERIC_INSTANCE:
    {
        SymbolTypeRef baseType = lptr->GetBaseType();
        Assert(baseType != nullptr);

        Array<GenericInstanceTypeInfo::Arg> newGenericTypes;

        for (const GenericInstanceTypeInfo::Arg& arg : lptr->GetGenericInstanceInfo().m_genericArgs)
        {
            GenericInstanceTypeInfo::Arg newArg {};
            newArg.m_name = arg.m_name;
            newArg.m_defaultValue = CloneAstNode(arg.m_defaultValue);
            newArg.m_type = SubstituteGenericParams(arg.m_type, placeholder, substitute);
            newArg.m_isConst = arg.m_isConst;
            newArg.m_isRef = arg.m_isRef;

            newGenericTypes.PushBack(std::move(newArg));
        }

        return SymbolType::GenericInstance(
            baseType,
            lptr->GetMembers(),
            lptr->GetStaticMembers(),
            GenericInstanceTypeInfo { newGenericTypes });
    }
    }

    return lptr;
}

HashCode SymbolType::GetHashCodeWithDuplicateRemoval(FlatSet<String>& duplicateNames) const
{
    if (IsAlias())
    {
        if (SymbolTypeRef aliasee = m_aliasInfo.m_aliasee.Lock())
        {
            return aliasee->GetHashCodeWithDuplicateRemoval(duplicateNames);
        }
        else
        {
            return HashCode();
        }
    }

    if (duplicateNames.Contains(m_name))
    {
        return HashCode();
    }

    duplicateNames.Insert(m_name);

    HashCode hc;
    hc.Add(m_name);
    hc.Add(m_typeClass);
    hc.Add(m_base ? m_base->GetHashCodeWithDuplicateRemoval(duplicateNames) : HashCode());
    hc.Add(m_flags);

    switch (m_typeClass)
    {
    case TYPE_GENERIC_INSTANCE:
        for (const auto& arg : m_genericInstanceInfo.m_genericArgs)
        {
            hc.Add(arg.m_name);
            hc.Add(arg.m_type ? arg.m_type->GetHashCodeWithDuplicateRemoval(duplicateNames) : HashCode());
        }

        break;
    case TYPE_BUILTIN: // fallthrough
    case TYPE_GENERIC_PARAMETER:
    case TYPE_USER_DEFINED:
    default:
        break;
    }

    for (const SymbolTypeMember& member : m_members)
    {
        hc.Add(member.name);

        if (!member.type)
        {
            continue;
        }

        hc.Add(member.type->GetHashCodeWithDuplicateRemoval(duplicateNames));
    }

    for (const SymbolTypeMember& staticMember : m_staticMembers)
    {
        hc.Add(staticMember.name);

        if (!staticMember.type)
        {
            continue;
        }

        hc.Add(staticMember.type->GetHashCodeWithDuplicateRemoval(duplicateNames));
    }

    return hc;
}

} // namespace hyperion::compiler
