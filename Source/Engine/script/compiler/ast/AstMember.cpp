#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/compiler/ast/AstModuleAccess.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>
#include <core/HashCode.hpp>

#include <iostream>

namespace Hyperion {

// Class names to use for overrides for built in types.
static const String s_stringClassName = "String";
static const String s_nameClassName = "Name";

static const String& GetClassNameForType(const SymbolType* type)
{
    if (!type)
    {
        return String::empty;
    }

    // Handle builtin types
    if (type->IsString())
    {
        return s_stringClassName;
    }

    if (type->IsName())
    {
        return s_nameClassName;
    }

    return type->GetName();
}

AstMember::AstMember(
    const String& fieldName,
    const RC<AstExpression>& target,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD | ACCESS_MODE_STORE),
      m_fieldName(fieldName),
      m_target(target),
      m_symbolType(nullptr),
      m_targetType(nullptr),
      m_heldType(nullptr),
      m_foundIndex(~0u),
      m_isStaticField(false),
      m_isStaticMethod(false)
{
}

void AstMember::Visit(AstVisitor* visitor, Module* mod)
{
    m_symbolType = BuiltinTypes::s_errorType;

    Assert(m_target != nullptr);
    m_target->Visit(visitor, mod);

    bool isProxyClass = false;

    m_accessOptions = m_target->GetAccessOptions();

    bool isStaticMemberAccess = false;

    if (const SymbolType* heldType = m_target->GetHeldType())
    {
        // static member access
        m_targetType = heldType;

        // disable store access for static member access since we don't support it currently
        m_accessOptions &= ~ACCESS_MODE_STORE;

        isStaticMemberAccess = true;
    }
    else
    {
        m_targetType = m_target->GetExprType();
    }

    Assert(m_targetType != nullptr);

    const SymbolType* resolvedType = SemanticAnalyzer::Helpers::ResolvePlaceholderType(
        visitor,
        mod,
        m_targetType,
        m_location);

    Assert(resolvedType != nullptr);
    resolvedType->Register(visitor->GetCompilationUnit());

    m_targetType = resolvedType;

    if (mod->IsInScopeOfType(SCOPE_TYPE_NORMAL, REF_VARIABLE_FLAG, /* thisScopeOnly */ false))
    {
        // TODO: implement
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_internal_error,
            m_location));
    }

    const SymbolType* originalType = m_targetType;

    // start looking at the target type,
    // iterate through base type
    const SymbolType* fieldType = nullptr;
    SymbolTypeMember member;

    for (uint32 depth = 0; fieldType == nullptr && m_targetType != nullptr; depth++)
    {
        Assert(m_targetType != nullptr);
        m_targetType = m_targetType->GetUnaliased();

        if (m_targetType->IsAnyType())
        {
            fieldType = BuiltinTypes::s_anyType;

            break;
        }

        isProxyClass = m_targetType->IsProxyClass();

        if (isProxyClass)
        {
            // load the type by name
            m_typeSpec.Reset(new AstTypeSpecifier(RC<AstTypeRef>(new AstTypeRef(m_targetType, m_location)), m_location));
            m_typeSpec->Visit(visitor, mod);

            // if it is a proxy class,
            // convert thing.DoThing()
            // to ThingProxy.DoThing(thing)
            if (m_targetType->FindMember(m_fieldName, member, m_foundIndex))
            {
                fieldType = member.GetType();
            }

            break;
        }

        {
            uint32 fieldIndex = ~0u;
            bool findMemberResult = false;

            if (isStaticMemberAccess)
            {
                findMemberResult = m_targetType->FindStaticMember(m_fieldName, member, fieldIndex);
            }
            else
            {
                findMemberResult = m_targetType->FindMember(m_fieldName, member, fieldIndex);
            }

            if (findMemberResult)
            {
                if (isStaticMemberAccess)
                {
                    if (member.GetType() != nullptr && member.GetType()->HasBase(*BuiltinTypes::s_functionBaseType))
                    {
                        m_isStaticMethod = true;
                    }
                    else
                    {
                        m_isStaticField = true;
                    }
                }

                // only set m_foundIndex if found in first level.
                // for members from base objects,
                // we load based on hash.
                if (depth == 0)
                {
                    m_foundIndex = fieldIndex;
                }

                fieldType = member.GetType();

                break;
            }
        }

        if (!isStaticMemberAccess)
        {
            // continue up the base type chain for non-static member access
            if (const SymbolType* base = m_targetType->GetBaseType())
            {
                m_targetType = base->GetUnaliased();

                continue;
            }
        }

        break;
    }

    Assert(m_targetType != nullptr);

    if (fieldType != nullptr)
    {
        m_symbolType = fieldType;

        return;
    }

    if (isStaticMemberAccess)
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_static_member_not_found,
            m_location,
            m_fieldName,
            originalType->ToString()));
    }
    else
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_instance_member_not_found,
            m_location,
            m_fieldName,
            originalType->ToString()));
    }
}

UniquePtr<Buildable> AstMember::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_typeSpec != nullptr)
    {
        chunk->Append(m_typeSpec->Build(visitor, mod));
    }

    const bool isStaticMember = m_isStaticField || m_isStaticMethod;

    if (isStaticMember)
    {
        Assert(m_targetType != nullptr);

        const uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        const String& className = GetClassNameForType(m_targetType);

        chunk->Append(BytecodeUtil::Make<LoadClass>(rp, StringHash(className)));
    }
    else if (m_target != nullptr)
    {
        chunk->Append(m_target->Build(visitor, mod));
    }

    const HashCode::ValueType hash = HashCode::GetHashCode(m_fieldName.Data()).Value();

    switch (m_accessMode)
    {
    case ACCESS_MODE_LOAD:
        chunk->Append(BytecodeUtil::Make<Comment>((isStaticMember ? "Load static member " : "Load member ") + m_fieldName));
        chunk->Append(Compiler::LoadMemberFromHash(visitor, mod, hash));
        break;
    case ACCESS_MODE_STORE:
        chunk->Append(BytecodeUtil::Make<Comment>((isStaticMember ? "Store static member " : "Store member ") + m_fieldName));
        chunk->Append(Compiler::StoreMemberFromHash(visitor, mod, hash));
        break;
    default:
        HYP_UNREACHABLE();
        break;
    }

    return chunk;
}

void AstMember::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_typeSpec != nullptr)
    {
        m_typeSpec->Optimize(visitor, mod);

        // return;
    }

    Assert(m_target != nullptr);

    m_target->Optimize(visitor, mod);
}

RC<AstStatement> AstMember::Clone() const
{
    return CloneImpl();
}

Tribool AstMember::IsTrue() const
{
    return Tribool::Indeterminate();
}

bool AstMember::MayHaveSideEffects() const
{
    if (m_typeSpec != nullptr && m_typeSpec->MayHaveSideEffects())
    {
        return true;
    }

    Assert(m_target != nullptr);

    return m_target->MayHaveSideEffects() || m_accessMode == ACCESS_MODE_STORE;
}

const SymbolType* AstMember::GetExprType() const
{
    return m_symbolType;
}

const SymbolType* AstMember::GetHeldType() const
{
    if (m_heldType)
    {
        return m_heldType;
    }

    return AstExpression::GetHeldType();
}

const AstExpression* AstMember::GetValueOf() const
{
    return AstExpression::GetValueOf();
}

const AstExpression* AstMember::GetDeepValueOf() const
{
    return AstExpression::GetDeepValueOf();
}

AstExpression* AstMember::GetTarget() const
{
    if (m_target != nullptr)
    {
        return m_target.Get();
    }

    return AstExpression::GetTarget();
}

bool AstMember::IsMutable() const
{
    if (m_typeSpec != nullptr && !m_typeSpec->IsMutable())
    {
        return false;
    }

    Assert(m_target != nullptr);
    Assert(m_symbolType != nullptr);

    if (!m_target->IsMutable())
    {
        return false;
    }

    return true;
}

} // namespace Hyperion
