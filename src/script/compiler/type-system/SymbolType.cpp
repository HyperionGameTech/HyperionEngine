#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>

#include <script/compiler/CompilationUnit.hpp>

#include <core/threading/Mutex.hpp>

#include <core/memory/allocator/SlabAllocator.hpp>

#include <core/debug/Debug.hpp>
#include <core/debug/StackDump.hpp>

#include <core/logging/Logger.hpp>

namespace Hyperion {

SlabAllocator& GetSymbolTypeAllocator()
{
    static SlabAllocator g_symbolTypeAllocator(
        sizeof(SymbolType),
        alignof(SymbolType),
        1024);

    return g_symbolTypeAllocator;
}

#if defined(HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG) && HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG

HYP_DECLARE_LOG_CHANNEL(Script);

static HashSet<SymbolType*>& GetUnfreedSymbolTypes()
{
    static HashSet<SymbolType*> s_unfreedSymbolTypes;
    return s_unfreedSymbolTypes;
}

static Mutex& GetUnfreedSymbolTypesMutex()
{
    static Mutex s_unfreedSymbolTypesMutex;
    return s_unfreedSymbolTypesMutex;
}

void CheckUnfreedSymbolTypes()
{
    Mutex::Guard guard(GetUnfreedSymbolTypesMutex());

    HashSet<SymbolType*>& unfreedSymbolTypes = GetUnfreedSymbolTypes();

    if (unfreedSymbolTypes.Empty())
    {
        return;
    }

    String message = HYP_FORMAT("WARNING! Detected {} unfreed SymbolType pointers:\n", unfreedSymbolTypes.Size());
    for (SymbolType* type : unfreedSymbolTypes)
    {
        // Cannot use ToString(), as it may reference other SymbolTypes that have been deleted
        message += HYP_FORMAT(" - {} (type: {}) @  {}\n",
            type->GetName(),
            SymbolTypeClassToString(type->GetTypeClass()),
            (void*)type);
    }

    HYP_LOG(Script, Warning, "{}", message);
}

#endif

#pragma region GenericInstanceTypeInfo

HashCode GenericInstanceTypeInfo::Arg::GetHashCode() const
{
    HashCode hc;
    hc.Add(m_name);
    hc.Add(m_type ? m_type->GetHashCode() : HashCode());
    hc.Add(m_defaultValue ? m_defaultValue->GetHashCode() : HashCode());
    hc.Add(m_isRef);
    hc.Add(m_isConst);

    return hc;
}

HashCode GenericInstanceTypeInfo::GetHashCode() const
{
    HashCode hc;

    for (const Arg& arg : m_genericArgs)
    {
        hc.Add(HashCode::GetHashCode(arg));
    }

    return hc;
}

#pragma endregion GenericInstanceTypeInfo

#pragma region SymbolTypeRegistration

SymbolTypeRegistration::SymbolTypeRegistration(SymbolType* symbolType)
    : symbolType(symbolType),
      isDestructing(false)
{
    if (symbolType)
    {
        Assert(symbolType->m_registration == nullptr, "SymbolType already registered!");

        symbolType->m_registration = this;

#if defined(HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG) && HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG
        // will not be unfreed if we own it:
        Mutex::Guard guard(GetUnfreedSymbolTypesMutex());
        GetUnfreedSymbolTypes().Erase(symbolType);
#endif
    }
}

SymbolTypeRegistration::~SymbolTypeRegistration()
{
    isDestructing = true;

    if (symbolType)
    {
        Assert(symbolType->m_registration == this);

        delete symbolType;

#if defined(HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG) && HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG
        // will not be unfreed if we own it:
        Mutex::Guard guard(GetUnfreedSymbolTypesMutex());
        Assert(!GetUnfreedSymbolTypes().Contains(symbolType));
#endif

        symbolType = nullptr;
    }
}

#pragma endregion SymbolTypeRegistration

SymbolType::SymbolType()
    : m_name("<temp>"),
      m_typeClass(TYPE_INVALID),
      m_base(nullptr),
      m_defaultValue(nullptr),
      m_constantBitSize(CBS_INVALID),
      m_flags(STF_NONE),
      m_declScope(nullptr),
      m_registration(nullptr),
      m_cacheCounter(0)
{
#if defined(HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG) && HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG
    Mutex::Guard guard(GetUnfreedSymbolTypesMutex());
    GetUnfreedSymbolTypes().Insert(this);

#if defined(HYP_SYMBOL_TYPE_ALLOCATION_TRACE) && HYP_SYMBOL_TYPE_ALLOCATION_TRACE
    allocationTrace = StackDump(2, 5).ToString();
#endif
#endif
}

SymbolType::SymbolType(
    const String& name,
    SymbolTypeClass typeClass,
    const SymbolType* base)
    : m_name(name),
      m_typeClass(typeClass),
      m_defaultValue(nullptr),
      m_base(base ? base->GetUnaliased() : nullptr),
      m_constantBitSize(CBS_INVALID),
      m_flags(STF_NONE),
      m_declScope(nullptr),
      m_registration(nullptr),
      m_cacheCounter(0)
{
#if defined(HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG) && HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG
    Mutex::Guard guard(GetUnfreedSymbolTypesMutex());
    GetUnfreedSymbolTypes().Insert(this);

#if defined(HYP_SYMBOL_TYPE_ALLOCATION_TRACE) && HYP_SYMBOL_TYPE_ALLOCATION_TRACE
    allocationTrace = StackDump(2, 5).ToString();
#endif
#endif
}

SymbolType::SymbolType(
    const String& name,
    SymbolTypeClass typeClass,
    const SymbolType* base,
    const RC<AstExpression>& defaultValue,
    Array<SymbolTypeMember>&& members,
    Array<SymbolTypeMember>&& staticMembers)
    : m_name(name),
      m_typeClass(typeClass),
      m_defaultValue(defaultValue),
      m_members(std::move(members)),
      m_staticMembers(std::move(staticMembers)),
      m_base(base ? base->GetUnaliased() : nullptr),
      m_constantBitSize(CBS_INVALID),
      m_flags(STF_NONE),
      m_declScope(nullptr),
      m_registration(nullptr),
      m_cacheCounter(0)
{
#if defined(HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG) && HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG
    Mutex::Guard guard(GetUnfreedSymbolTypesMutex());
    GetUnfreedSymbolTypes().Insert(this);

#if defined(HYP_SYMBOL_TYPE_ALLOCATION_TRACE) && HYP_SYMBOL_TYPE_ALLOCATION_TRACE
    allocationTrace = StackDump(2, 5).ToString();
#endif
#endif
}

SymbolType::~SymbolType()
{
    AssertDebug(!IsCached(), "SymbolType {} deleted while in cache!", m_name);

    // would cause a leak if this happens:
    if (m_registration != nullptr)
    {
        if (!m_registration->isDestructing)
        {
            HYP_FAIL("SymbolType {} deleted while still registered! This would cause a dangling pointer.", m_name);
        }
    }
    else if (m_registration == nullptr)
    {
        DeleteReferencedTypes();
    }

#if defined(HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG) && HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG
    Mutex::Guard guard(GetUnfreedSymbolTypesMutex());
    GetUnfreedSymbolTypes().Erase(this);

#if 0
    for (const SymbolTypeMember& mem : m_members)
    {
        if (!mem.GetType())
        {
            continue;
        }

        mem.GetType()->AssertRegistered();
    }

    for (const SymbolTypeMember& mem : m_staticMembers)
    {
        if (!mem.GetType())
        {
            continue;
        }

        mem.GetType()->AssertRegistered();
    }

    if (IsGenericInstanceType())
    {
        for (const GenericInstanceTypeInfo::Arg& arg : m_genericInstanceInfo.m_genericArgs)
        {
            if (!arg.m_type)
            {
                continue;
            }

            arg.m_type->AssertRegistered();
        }
    }
#endif
#endif
}

void SymbolType::SetMembers(Array<SymbolTypeMember>&& members)
{
    AssertDebug(!IsRegistered());

    for (auto& member : m_members)
    {
        if (member.GetType() != nullptr)
        {
            AssertDebug(member.GetType()->IsRegistered());
        }
    }

    m_members = std::move(members);
}

void SymbolType::SetStaticMembers(Array<SymbolTypeMember>&& staticMembers)
{
    AssertDebug(!IsRegistered());

    for (auto& member : m_staticMembers)
    {
        if (member.GetType() != nullptr)
        {
            AssertDebug(member.GetType()->IsRegistered());
        }
    }

    m_staticMembers = std::move(staticMembers);
}

bool SymbolType::TypeEqual(const SymbolType& other) const
{
    if (this == &other)
    {
        return true;
    }

    if (IsAlias() || other.IsAlias())
    {
        return GetUnaliased()->TypeEqual(*other.GetUnaliased());
    }

    if (m_name != other.m_name)
    {
        return false;
    }

    if (m_typeClass != other.m_typeClass)
    {
        return false;
    }

    return GetHashCode() == other.GetHashCode();

#if 0
    switch (m_typeClass)
    {
    case TYPE_BUILTIN:
        return true;
    case TYPE_ALIAS:
        if (SymbolType* aliasee = m_aliasInfo.m_aliasee)
        {
            return *aliasee == other;
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

        SymbolType* base = m_base;
        Assert(base != nullptr);

        // check for compatibility between instances
        SymbolType* otherBase = other.GetBaseType();
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
            SymbolType* instanceArgType = m_genericInstanceInfo.m_genericArgs[i].m_type;
            SymbolType* otherArgType = other.m_genericInstanceInfo.m_genericArgs[i].m_type;

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
        SymbolType* leftMemberType = leftMember.type;
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
#endif
}

#define ADD_INCOMPATIBILITY(type, details)                 \
    if (outIncompatibilities != nullptr)                   \
    {                                                      \
        outIncompatibilities->PushBack({ type, details }); \
    }

HYP_DISABLE_OPTIMIZATION;
bool SymbolType::TypeCompatible(
    const SymbolType& right,
    bool strictNumbers,
    bool strictAny,
    bool strictEnum,
    bool strictNull,
    SymbolTypeIncompatibilities* outIncompatibilities) const
{
    if (m_typeClass == TYPE_ALIAS)
    {
        const SymbolType* aliasee = m_aliasInfo.m_aliasee;

        if (!aliasee)
        {
            ADD_INCOMPATIBILITY(IT_UNKNOWN, "Internal error occurred while attempting to resolve alias type of left-hand side");

            return false;
        }

        return aliasee->TypeCompatible(
            right,
            strictNumbers,
            strictAny,
            strictEnum,
            strictNull,
            outIncompatibilities);
    }

    if (right.m_typeClass == TYPE_ALIAS)
    {
        const SymbolType* aliasee = right.m_aliasInfo.m_aliasee;

        if (!aliasee)
        {
            ADD_INCOMPATIBILITY(IT_UNKNOWN, "Internal error occurred while attempting to resolve alias type of right-hand side");

            return false;
        }

        return TypeCompatible(
            *aliasee,
            strictNumbers,
            strictAny,
            strictEnum,
            strictNull,
            outIncompatibilities);
    }

    if (TypeEqual(*BuiltinTypes::s_errorType) || right.TypeEqual(*BuiltinTypes::s_errorType))
    {
        ADD_INCOMPATIBILITY(IT_UNDEFINED_TYPE, "one of the types was the result of an errored expression");

        return false;
    }

    if (TypeEqual(right))
    {
        return true;
    }

    // check object inheritance (left is base of right)
    if (IsObject() && right.IsObject())
    {
        if (right.IsOrHasBase(*this))
        {
            return true;
        }

        ADD_INCOMPATIBILITY(IT_BASE_MISMATCH, "left-hand side type '" + GetName() + "' is not a base of right-hand side type '" + right.GetName() + "'");

        return false;
    }

    if (IsAnyType())
    {
        return true;
    }
    else if (right.IsAnyType())
    {
        if (strictAny)
        {
            ADD_INCOMPATIBILITY(IT_IMPLICIT_ANY, "right-hand side of expression is 'any' and must be explicitly cast to " + ToString(false) + " using the as operator, e.g `<expr> as " + ToString(false) + "`");

            return false;
        }

        // can assign to RHS any if not strictAny
        return true;
    }

    if (IsVarArgsType())
    {
        // cannot assign anything to varargs without explicit cast or unpacking.
        ADD_INCOMPATIBILITY(IT_VARARGS, "left-hand side of expression is variadic and cannot be used for direct assignment");

        return false;
    }

    if (right.IsVarArgsType())
    {
        // Allow passing VarArgs as Array
        if (IsArrayType())
        {
            // Check element type compatibility
            if (m_genericInstanceInfo.m_genericArgs.Size() == 1 && right.GetGenericInstanceInfo().m_genericArgs.Size() == 1)
            {
                const SymbolType* arrayElemType = m_genericInstanceInfo.m_genericArgs[0].m_type;
                const SymbolType* varArgsElemType = right.GetGenericInstanceInfo().m_genericArgs[0].m_type;

                if (arrayElemType && varArgsElemType)
                {
                    return arrayElemType->TypeCompatible(*varArgsElemType, strictNumbers, strictAny, strictEnum, strictNull, outIncompatibilities);
                }
            }
        }

        // cannot assign anything from varargs without explicit cast or unpacking.
        ADD_INCOMPATIBILITY(IT_VARARGS, "right-hand side of expression is variadic and cannot be used for direct assignment");

        return false;
    }

    // if (IsProxyClass()) {
    //     // TODO:
    //     // have proxy class declare which class it is a proxy for,
    //     // then check that the types match?
    //     return true;
    // }

    if (IsNullType())
    {
        if (strictNull && !right.IsNullableType())
        {
            ADD_INCOMPATIBILITY(IT_NULLABLE_MISMATCH, "null is only compatible with nullable types");

            return false;
        }

        return true;
    }

    if (right.IsNullType())
    {
        if (strictNull && !IsNullableType())
        {
            ADD_INCOMPATIBILITY(IT_NULLABLE_MISMATCH, "null is only compatible with nullable types");

            return false;
        }

        return true;
    }

    if (IsEnumType() && !right.IsEnumType())
    {
        if (strictEnum)
        {
            ADD_INCOMPATIBILITY(IT_TYPE_CLASS_MISMATCH, "Cannot directly assign enum type to non-enum type in this context");

            return false;
        }

        // use the underlying type for compatibility checks
        const SymbolType* underlyingType = m_base;
        Assert(underlyingType != nullptr);

        return underlyingType->TypeCompatible(
            right,
            strictNumbers,
            strictAny,
            strictEnum,
            strictNull,
            outIncompatibilities);
    }

    if (right.IsEnumType() && !IsEnumType())
    {
        if (strictEnum)
        {
            ADD_INCOMPATIBILITY(IT_TYPE_CLASS_MISMATCH, "Cannot directly assign non-enum type to enum type in this context");

            return false;
        }

        // use the underlying type for compatibility checks
        const SymbolType* underlyingType = right.GetBaseType();
        Assert(underlyingType != nullptr);

        return TypeCompatible(
            *underlyingType,
            strictNumbers,
            strictAny,
            strictEnum,
            strictNull,
            outIncompatibilities);
    }

    if (m_typeClass != right.m_typeClass)
    {
        return false;
    }

    switch (m_typeClass)
    {
    case TYPE_ALIAS:
        // should not hit here due to earlier checks
        HYP_UNREACHABLE();
    case TYPE_GENERIC_INSTANCE:
    {
        // check all params
        if (m_genericInstanceInfo.m_genericArgs.Size() != right.m_genericInstanceInfo.m_genericArgs.Size())
        {
            ADD_INCOMPATIBILITY(IT_GENERIC_ARG_MISMATCH, "Generic argument count does not match");

            return false;
        }

        // check each substituted parameter
        for (SizeType i = 0; i < m_genericInstanceInfo.m_genericArgs.Size(); i++)
        {
            const SymbolType* paramType = m_genericInstanceInfo.m_genericArgs[i].m_type;
            Assert(paramType != nullptr);

            const SymbolType* otherParamType = right.m_genericInstanceInfo.m_genericArgs[i].m_type;
            Assert(otherParamType != nullptr);

            if (!paramType->TypeEqual(*otherParamType))
            {
                ADD_INCOMPATIBILITY(IT_GENERIC_ARG_MISMATCH, "Generic parameter types do not match: " + paramType->ToString(false) + " != " + otherParamType->ToString(false));

                return false;
            }
        }

        // check members
        if (m_members.Size() != right.m_members.Size() && !outIncompatibilities)
        {
            // short circuit if sizes don't match (and we're not collecting incompatibilities)
            return false;
        }

        for (const SymbolTypeMember& leftMember : m_members)
        {
            const SymbolType* leftMemberType = leftMember.GetType();
            Assert(leftMemberType != nullptr);

            SymbolTypeMember rightMember;

            if (!right.FindMember(leftMember.GetName(), rightMember))
            {
                ADD_INCOMPATIBILITY(IT_MEMBER_MISMATCH, "Member '" + leftMember.GetName() + "' not found in " + right.GetName());

                return false;
            }

            Assert(rightMember.GetType() != nullptr);

            if (!rightMember.GetType()->TypeEqual(*leftMemberType))
            {
                ADD_INCOMPATIBILITY(IT_MEMBER_MISMATCH, "Member '" + leftMember.GetName() + "' type mismatch: " + leftMemberType->ToString(false) + " != " + rightMember.GetType()->ToString(false));

                return false;
            }
        }

        // check static members
        if (m_staticMembers.Size() != right.m_staticMembers.Size() && !outIncompatibilities)
        {
            // short circuit if sizes don't match (and we're not collecting incompatibilities)
            return false;
        }

        for (const SymbolTypeMember& leftMember : m_staticMembers)
        {
            const SymbolType* leftMemberType = leftMember.GetType();
            Assert(leftMemberType != nullptr);

            SymbolTypeMember rightMember;

            if (!right.FindMember(leftMember.GetName(), rightMember))
            {
                ADD_INCOMPATIBILITY(IT_STATIC_MEMBER_MISMATCH, "Static member '" + leftMember.GetName() + "' not found in " + right.GetName());

                return false;
            }

            Assert(rightMember.GetType() != nullptr);

            if (!rightMember.GetType()->TypeEqual(*leftMemberType))
            {
                ADD_INCOMPATIBILITY(IT_STATIC_MEMBER_MISMATCH, "Static member '" + leftMember.GetName() + "' type mismatch: " + leftMemberType->ToString(false) + " != " + rightMember.GetType()->ToString(false));

                return false;
            }
        }

        return true;
    }
    default:
        if (IsNumber() && right.IsNumber())
        {
            if (!strictNumbers)
            {
                return true;
            }

            // check if right can be promoted to left
            const SymbolType* promotedType = TypePromotion(this, &right);

            if (promotedType != nullptr && promotedType->TypeEqual(*this))
            {
                // can promote right to left
                return true;
            }

            if (promotedType->IsFloat() && IsFloat())
            {
                // allow implicit conversion between float types
                return true;
            }

            if (right.IsUnsignedIntegral() && IsSignedIntegral())
            {
                ADD_INCOMPATIBILITY(IT_DATA_LOSS, "Conversion may cause a signed integer overflow. Use the `as` operator to perform an explicit cast, e.g. `<expr> as " + ToString(false) + "`");

                return false;
            }

            if (right.GetConstantBitSize() > GetConstantBitSize())
            {
                ADD_INCOMPATIBILITY(IT_DATA_LOSS, "Conversion may cause data loss. Use the `as` operator to perform an explicit cast, e.g. `<expr> as " + ToString(false) + "`");

                return false;
            }

            return true;
        }

        break;
    }

    return false;
}
HYP_ENABLE_OPTIMIZATION;

const SymbolType* SymbolType::FindMember(UTF8StringView name) const
{
    for (const SymbolTypeMember& member : m_members)
    {
        if (member.GetName() == name)
        {
            return member.GetType();
        }
    }

    return nullptr;
}

bool SymbolType::FindMember(UTF8StringView name, SymbolTypeMember& out) const
{
    for (const SymbolTypeMember& member : m_members)
    {
        if (member.GetName() == name)
        {
            out = member;
            return true;
        }
    }

    return false;
}

bool SymbolType::FindMember(UTF8StringView name, SymbolTypeMember& out, uint32& outIndex) const
{
    // get member index from name
    for (SizeType i = 0; i < m_members.Size(); i++)
    {
        const SymbolTypeMember& member = m_members[i];

        if (member.GetName() == name)
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

const SymbolType* SymbolType::FindMemberDeep(UTF8StringView name) const
{
    SymbolTypeMember out;
    uint32 outIndex;
    uint32 outDepth;

    if (FindMemberDeep(name, out, outIndex, outDepth))
    {
        return out.GetType();
    }

    return nullptr;
}

bool SymbolType::FindMemberDeep(UTF8StringView name, SymbolTypeMember& out) const
{
    uint32 outIndex;
    uint32 outDepth;

    return FindMemberDeep(name, out, outIndex, outDepth);
}

bool SymbolType::FindMemberDeep(UTF8StringView name, SymbolTypeMember& out, uint32& outIndex) const
{
    uint32 outDepth;

    return FindMemberDeep(name, out, outIndex, outDepth);
}

bool SymbolType::FindMemberDeep(UTF8StringView name, SymbolTypeMember& out, uint32& outIndex, uint32& outDepth) const
{
    outDepth = 0;

    if (FindMember(name, out, outIndex))
    {
        return true;
    }

    outDepth++;

    const SymbolType* base = GetBaseType();

    while (base != nullptr)
    {
        if (base->FindMember(name, out, outIndex))
        {
            return true;
        }

        base = base->GetBaseType();

        outDepth++;
    }

    return false;
}

const SymbolType* SymbolType::FindStaticMember(UTF8StringView name) const
{
    for (const SymbolTypeMember& member : m_staticMembers)
    {
        if (member.GetName() == name)
        {
            return member.GetType();
        }
    }

    return nullptr;
}

bool SymbolType::FindStaticMember(UTF8StringView name, SymbolTypeMember& out) const
{
    for (const SymbolTypeMember& member : m_staticMembers)
    {
        if (member.GetName() == name)
        {
            out = member;
            return true;
        }
    }

    return false;
}

bool SymbolType::FindStaticMember(UTF8StringView name, SymbolTypeMember& out, uint32& outIndex) const
{
    // get member index from name
    for (SizeType i = 0; i < m_staticMembers.Size(); i++)
    {
        const SymbolTypeMember& member = m_staticMembers[i];

        if (member.GetName() == name)
        {
            outIndex = uint32(i);
            out = member;

            return true;
        }
    }

    return false;
}

bool SymbolType::IsOrHasBase(const SymbolType& baseType) const
{
    return TypeEqual(baseType) || HasBase(baseType);
}

bool SymbolType::HasBase(const SymbolType& baseType) const
{
    const SymbolType* base = m_base;

    while (base != nullptr)
    {
        if (base->TypeEqual(baseType))
        {
            return true;
        }

        base = base->GetBaseType();
    }

    return false;
}

SymbolType* SymbolType::GetUnaliased()
{
    if (m_typeClass == TYPE_ALIAS)
    {
        if (m_aliasInfo.m_aliasee != nullptr)
        {
            // prevent infinite recursion
            if (m_aliasInfo.m_aliasee == this)
            {
                return this;
            }

            // hack
            return const_cast<SymbolType*>(m_aliasInfo.m_aliasee->GetUnaliased());
        }
    }

    return this;
}

bool SymbolType::IsVoidType() const
{
    return TypeEqual(*BuiltinTypes::s_voidType);
}

bool SymbolType::IsNumber() const
{
    return IsIntegral() || IsFloat();
}

bool SymbolType::IsIntegral() const
{
    return IsSignedIntegral() || IsUnsignedIntegral();
}

bool SymbolType::IsSignedIntegral() const
{
    return TypeEqual(*BuiltinTypes::s_int8Type)
        || TypeEqual(*BuiltinTypes::s_int16Type)
        || TypeEqual(*BuiltinTypes::s_int32Type)
        || TypeEqual(*BuiltinTypes::s_int64Type);
}

bool SymbolType::IsUnsignedIntegral() const
{
    return TypeEqual(*BuiltinTypes::s_uint8Type)
        || TypeEqual(*BuiltinTypes::s_uint16Type)
        || TypeEqual(*BuiltinTypes::s_uint32Type)
        || TypeEqual(*BuiltinTypes::s_uint64Type);
}

bool SymbolType::IsFloat() const
{
    return TypeEqual(*BuiltinTypes::s_floatType)
        || TypeEqual(*BuiltinTypes::s_doubleType);
}

bool SymbolType::IsBoolean() const
{
    return TypeEqual(*BuiltinTypes::s_boolType);
}

bool SymbolType::IsObject() const
{
    return IsOrHasBase(*BuiltinTypes::s_objectType);
}

bool SymbolType::IsAnyType() const
{
    return TypeEqual(*BuiltinTypes::s_anyType);
}

bool SymbolType::IsPlaceholderType() const
{
    return IsOrHasBase(*BuiltinTypes::s_placeholderType);
}

bool SymbolType::IsNullType() const
{
    return TypeEqual(*BuiltinTypes::s_nullType);
}

bool SymbolType::IsNullableType() const
{
    return IsOrHasBase(*BuiltinTypes::s_objectType)
        || IsOrHasBase(*BuiltinTypes::s_functionBaseType)
        || IsOrHasBase(*BuiltinTypes::s_arrayBaseType)
        || IsOrHasBase(*BuiltinTypes::s_mapBaseType)
        || IsOrHasBase(*BuiltinTypes::s_stringType);
}

bool SymbolType::IsVarArgsType() const
{
    return IsOrHasBase(*BuiltinTypes::s_varArgsBaseType);
}

bool SymbolType::IsArrayType() const
{
    return IsOrHasBase(*BuiltinTypes::s_arrayBaseType);
}

bool SymbolType::IsString() const
{
    return TypeEqual(*BuiltinTypes::s_stringType);
}

bool SymbolType::IsName() const
{
    return TypeEqual(*BuiltinTypes::s_nameType);
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
    return IsOrHasBase(*BuiltinTypes::s_primitiveType);
}

bool SymbolType::IsEnumType() const
{
    return m_typeClass == TYPE_ENUM;
}

SymbolType* SymbolType::Alias(const String& name, const AliasTypeInfo& info)
{
    Assert(info.m_aliasee != nullptr);

    SymbolType* result = new SymbolType(
        name,
        TYPE_ALIAS,
        nullptr);

    result->m_aliasInfo = info;

    return result;
}

SymbolType* SymbolType::Placeholder(const String& name)
{
    return new SymbolType(name, TYPE_PLACEHOLDER, BuiltinTypes::s_placeholderType);
}

SymbolType* SymbolType::Primitive(
    const String& name,
    const RC<AstExpression>& defaultValue,
    ConstantBitSize bitSize,
    Array<SymbolTypeMember>&& members,
    Array<SymbolTypeMember>&& staticMembers)
{
    SymbolType* symbolType = new SymbolType(
        name,
        TYPE_BUILTIN,
        BuiltinTypes::s_primitiveType,
        defaultValue,
        std::move(members),
        std::move(staticMembers));

    symbolType->m_constantBitSize = bitSize;

    return symbolType;
}

SymbolType* SymbolType::Enum(
    const String& name,
    const SymbolType* underlyingType,
    Array<SymbolTypeMember>&& enumMembers)
{
    Assert(underlyingType != nullptr);
    Assert(underlyingType->IsIntegral());

    SymbolType* symbolType = new SymbolType(
        name,
        TYPE_ENUM,
        underlyingType,
        nullptr,
        {},
        std::move(enumMembers));

    return symbolType;
}

SymbolType* SymbolType::Object(
    const String& name,
    const SymbolType* baseType,
    Array<SymbolTypeMember>&& members,
    Array<SymbolTypeMember>&& staticMembers)
{
    SymbolType* symbolType = new SymbolType(
        name,
        TYPE_USER_DEFINED,
        baseType,
        nullptr,
        std::move(members),
        std::move(staticMembers));

    return symbolType;
}

void SymbolType::Register(CompilationUnit* compilationUnit) const
{
    Assert(compilationUnit != nullptr);

    compilationUnit->RegisterType(const_cast<SymbolType*>(this));
}

String SymbolType::ToString(bool includeParameterNames) const
{
    String res = m_name;

    if (const SymbolType* aliasee = m_aliasInfo.m_aliasee)
    {
        res += " (aka " + aliasee->ToString() + ")";
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
                const SymbolType* heldType = info.m_genericArgs.Front().m_type;
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
                    const SymbolType* genericArgType = info.m_genericArgs[i].m_type;
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

SymbolType* SymbolType::SymbolType::Generic(
    const String& name,
    Array<SymbolTypeMember>&& members,
    Array<SymbolTypeMember>&& staticMembers,
    const GenericInstanceTypeInfo& info)
{
    return GenericInstance(
        name,
        nullptr,
        std::move(members),
        std::move(staticMembers),
        info);
}

SymbolType* SymbolType::SymbolType::Generic(
    const String& name,
    const SymbolType* baseType,
    Array<SymbolTypeMember>&& members,
    Array<SymbolTypeMember>&& staticMembers,
    const GenericInstanceTypeInfo& info)
{
    SymbolType* genericInstance = GenericInstance(
        name,
        nullptr,
        std::move(members),
        std::move(staticMembers),
        info);

    if (!genericInstance)
    {
        return nullptr;
    }

    genericInstance->SetBaseType(baseType);

    return genericInstance;
}

SymbolType* SymbolType::GenericInstance(
    const SymbolType* genericType,
    Array<SymbolTypeMember>&& members,
    Array<SymbolTypeMember>&& staticMembers,
    const GenericInstanceTypeInfo& info)
{
    Assert(genericType != nullptr && genericType->IsGenericInstanceType());

    return GenericInstance(
        genericType->GetName(),
        genericType,
        std::move(members),
        std::move(staticMembers),
        info);
}

SymbolType* SymbolType::GenericInstance(
    const String& name,
    const SymbolType* genericType,
    Array<SymbolTypeMember>&& members,
    Array<SymbolTypeMember>&& staticMembers,
    const GenericInstanceTypeInfo& info)
{
    Array<SymbolTypeMember> allMembers;
    allMembers.Reserve((genericType ? genericType->GetMembers().Size() : 0) + members.Size());

    Array<SymbolTypeMember> allStaticMembers;
    allStaticMembers.Reserve((genericType ? genericType->GetStaticMembers().Size() : 0) + staticMembers.Size());

    const SymbolType* base = genericType ? genericType->GetBaseType() : nullptr;

    if (genericType != nullptr)
    {
        for (const SymbolTypeMember& member : genericType->GetMembers())
        {
            const auto overridenMemberIt = allMembers.FindIf([&member](const SymbolTypeMember& otherMember)
                {
                    return otherMember.GetName() == member.GetName();
                });

            if (overridenMemberIt != allMembers.End())
            {
                // if member is overriden, skip it
                continue;
            }

            Assert(member.GetType() != nullptr);

            allMembers.PushBack(SymbolTypeMember {
                member.GetName(),
                const_cast<SymbolType*>(member.GetType()),
                CloneAstNode(member.GetExpr()) });
        }

        for (const SymbolTypeMember& staticMember : genericType->GetStaticMembers())
        {
            const auto overridenStaticMemberIt = allStaticMembers.FindIf([&staticMember](const auto& otherMember)
                {
                    return otherMember.GetName() == staticMember.GetName();
                });

            if (overridenStaticMemberIt != allStaticMembers.End())
            {
                // if member is overriden, skip it
                continue;
            }

            Assert(staticMember.GetType() != nullptr);

            // push copy (clone assignment value)
            allStaticMembers.PushBack(SymbolTypeMember {
                staticMember.GetName(),
                const_cast<SymbolType*>(staticMember.GetType()),
                CloneAstNode(staticMember.GetExpr()) });
        }
    }

    // add/override with members from this instance
    for (SymbolTypeMember& member : members)
    {
        const auto overridenMemberIt = allMembers.FindIf([&member](const SymbolTypeMember& otherMember)
            {
                return otherMember.GetName() == member.GetName();
            });

        if (overridenMemberIt != allMembers.End())
        {
            *overridenMemberIt = member;
        }
        else
        {
            // push copy (clone assignment value)
            allMembers.PushBack(SymbolTypeMember {
                member.GetName(),
                member.GetType(),
                CloneAstNode(member.GetExpr()) });
        }
    }

    // add/override with static members from this instance
    for (SymbolTypeMember& staticMember : staticMembers)
    {
        const auto overridenStaticMemberIt = allStaticMembers.FindIf([&staticMember](const SymbolTypeMember& otherMember)
            {
                return otherMember.GetName() == staticMember.GetName();
            });

        if (overridenStaticMemberIt != allStaticMembers.End())
        {
            *overridenStaticMemberIt = staticMember;
        }
        else
        {
            // push copy (clone assignment value)
            allStaticMembers.PushBack(SymbolTypeMember {
                staticMember.GetName(),
                staticMember.GetType(),
                CloneAstNode(staticMember.GetExpr()) });
        }
    }

    SymbolType* result = new SymbolType(
        name,
        TYPE_GENERIC_INSTANCE,
        base,
        nullptr,
        std::move(allMembers),
        std::move(allStaticMembers));

    if (genericType)
    {
        RC<AstExpression> defaultValue = CloneAstNode(genericType->GetDefaultValue());
        result->SetDefaultValue(defaultValue);

        result->SetFlags(genericType->GetFlags());
    }

    result->m_genericInstanceInfo = info;

    return result;
}

SymbolType* SymbolType::GenericParameter(const String& name)
{
    return new SymbolType(name, TYPE_GENERIC_PARAMETER, nullptr);
}

SymbolType* SymbolType::Extend(
    const String& name,
    const SymbolType* baseType,
    Array<SymbolTypeMember>&& members,
    Array<SymbolTypeMember>&& staticMembers)
{
    Assert(baseType != nullptr);

    SymbolType* symbolType = new SymbolType(
        name,
        baseType->GetTypeClass(),
        baseType,
        baseType->GetDefaultValue(),
        std::move(members),
        std::move(staticMembers));

    symbolType->m_genericInstanceInfo = baseType->m_genericInstanceInfo;
    symbolType->m_genericParamInfo = baseType->m_genericParamInfo;

    return symbolType;
}

const SymbolType* SymbolType::TypePromotion(const SymbolType* lptr, const SymbolType* rptr)
{
    if (!lptr || !rptr)
    {
        return nullptr;
    }

    // compare pointer values
    if (lptr == rptr || lptr->TypeEqual(*rptr))
    {
        return lptr;
    }

    if (lptr->TypeEqual(*BuiltinTypes::s_errorType) || rptr->TypeEqual(*BuiltinTypes::s_errorType))
    {
        return BuiltinTypes::s_errorType;
    }
    else if (lptr->IsAnyType() || rptr->IsAnyType())
    {
        // Any + T = Any
        // T + Any = Any
        return BuiltinTypes::s_anyType;
    }
    else if (lptr->IsNumber() && rptr->IsNumber())
    {
        if (lptr->IsFloat() || rptr->IsFloat())
        {
            // Float promotion - use the larger bit size, minimum 32-bit
            ConstantBitSize leftSize = lptr->GetConstantBitSize();
            ConstantBitSize rightSize = rptr->GetConstantBitSize();
            ConstantBitSize resultSize = MathUtil::Max(leftSize, rightSize, CBS_32);

            if (resultSize == CBS_64)
            {
                return BuiltinTypes::s_doubleType;
            }
            else
            {
                return BuiltinTypes::s_floatType;
            }
        }
        else if (lptr->GetConstantBitSize() < CBS_32 && rptr->GetConstantBitSize() < CBS_32)
        {
            // Both sides are < 32-bit, promote to int32
            return BuiltinTypes::s_int32Type;
        }
        else
        {
            // Integer promotion - use the larger bit size
            ConstantBitSize leftSize = lptr->GetConstantBitSize();
            ConstantBitSize rightSize = rptr->GetConstantBitSize();
            ConstantBitSize resultSize = MathUtil::Max(leftSize, rightSize);

            bool isLeftUnsigned = lptr->IsUnsignedIntegral();
            bool isRightUnsigned = rptr->IsUnsignedIntegral();

            if (isLeftUnsigned || isRightUnsigned)
            {
                if (resultSize == CBS_8)
                {
                    return BuiltinTypes::s_uint8Type;
                }
                else if (resultSize == CBS_16)
                {
                    return BuiltinTypes::s_uint16Type;
                }
                else if (resultSize == CBS_32)
                {
                    return BuiltinTypes::s_uint32Type;
                }
                else if (resultSize == CBS_64)
                {
                    return BuiltinTypes::s_uint64Type;
                }
            }
            else
            {
                if (resultSize == CBS_8)
                {
                    return BuiltinTypes::s_int8Type;
                }
                else if (resultSize == CBS_16)
                {
                    return BuiltinTypes::s_int16Type;
                }
                else if (resultSize == CBS_32)
                {
                    return BuiltinTypes::s_int32Type;
                }
                else if (resultSize == CBS_64)
                {
                    return BuiltinTypes::s_int64Type;
                }
            }
        }
    }

    /// \todo Check for common base
    return BuiltinTypes::s_errorType;
}

void SymbolType::Assign(const SymbolType& other)
{
    if (this == &other)
    {
        return;
    }

    if (!IsRegistered())
    {
        DeleteReferencedTypes();
    }

    m_name = other.m_name;
    m_typeClass = other.m_typeClass;
    m_base = other.m_base;
    m_defaultValue = other.m_defaultValue;
    m_members = other.m_members;
    m_staticMembers = other.m_staticMembers;
    m_aliasInfo = other.m_aliasInfo;
    m_genericInstanceInfo = other.m_genericInstanceInfo;
    m_genericParamInfo = other.m_genericParamInfo;
    m_constantBitSize = other.m_constantBitSize;
    m_flags = other.m_flags;
    m_declScope = nullptr; // do not copy scope

    if (IsRegistered())
    {
        for (auto& it : m_members)
        {
            if (!it.GetType())
                continue;

            it.GetType()->AssertRegistered();
        }

        for (auto& it : m_staticMembers)
        {
            if (!it.GetType())
                continue;

            it.GetType()->AssertRegistered();
        }

        if (IsGenericInstanceType())
        {
            for (auto& it : m_genericInstanceInfo.m_genericArgs)
            {
                if (!it.m_type)
                    continue;

                it.m_type->AssertRegistered();
            }
        }
    }
}

void SymbolType::Assign(SymbolType&& other)
{
    if (!IsRegistered())
    {
        DeleteReferencedTypes();
    }

    m_name = other.m_name;
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

    if (IsRegistered())
    {
        for (auto& it : m_members)
        {
            if (!it.GetType())
                continue;

            it.GetType()->AssertRegistered();
        }

        for (auto& it : m_staticMembers)
        {
            if (!it.GetType())
                continue;

            it.GetType()->AssertRegistered();
        }

        if (IsGenericInstanceType())
        {
            for (auto& it : m_genericInstanceInfo.m_genericArgs)
            {
                if (!it.m_type)
                    continue;

                it.m_type->AssertRegistered();
            }
        }
    }
}

void SymbolType::DeleteReferencedTypes()
{
    HashSet<const SymbolType*> toDelete;

    for (SymbolTypeMember& mem : m_members)
    {
        if (!mem.GetType())
        {
            continue;
        }

        if (!mem.GetType()->IsRegistered())
        {
            toDelete.Insert(mem.GetType());
        }

        m_members.Clear();
    }

    for (SymbolTypeMember& mem : m_staticMembers)
    {
        if (!mem.GetType())
        {
            continue;
        }

        if (!mem.GetType()->IsRegistered())
        {
            toDelete.Insert(mem.GetType());
            mem.SetType(nullptr);
        }

        m_staticMembers.Clear();
    }

    if (IsGenericInstanceType())
    {
        for (GenericInstanceTypeInfo::Arg& arg : m_genericInstanceInfo.m_genericArgs)
        {
            if (!arg.m_type)
            {
                continue;
            }

            if (!arg.m_type->IsRegistered())
            {
                toDelete.Insert(arg.m_type);
            }
        }

        m_genericInstanceInfo.m_genericArgs.Clear();
    }

    if (IsAlias())
    {
        if (const SymbolType* aliasee = m_aliasInfo.m_aliasee)
        {
            if (!aliasee->IsRegistered())
            {
                toDelete.Insert(aliasee);
            }
        }

        m_aliasInfo.m_aliasee = nullptr;
    }

    if (toDelete.Any())
    {
        for (const SymbolType* type : toDelete)
        {
            delete type;
        }
    }
}

HashCode SymbolType::GetHashCodeWithDuplicateRemoval(HashSet<String>& duplicateNames) const
{
    if (IsAlias())
    {
        if (const SymbolType* aliasee = m_aliasInfo.m_aliasee)
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
    hc.Add(m_constantBitSize);
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

#if 0 // HACK for same classes returning not equal because of resolving placeholder types causing new SymbolType to be created.
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
#endif

    return hc;
}

} // namespace Hyperion
