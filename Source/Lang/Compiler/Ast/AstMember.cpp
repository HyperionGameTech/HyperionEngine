#include <Lang/Compiler/Ast/AstMember.hpp>
#include <Lang/Compiler/Ast/AstVariable.hpp>
#include <Lang/Compiler/Ast/AstNil.hpp>
#include <Lang/Compiler/Ast/AstIdentifier.hpp>
#include <Lang/Compiler/Ast/AstCallExpression.hpp>
#include <Lang/Compiler/Ast/AstTypeRef.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/Ast/AstModuleAccess.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/SemanticAnalyzer.hpp>
#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/Configuration.hpp>
#include <Lang/Compiler/Keywords.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>
#include <Lang/Compiler/Emit/StorageOperation.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>
#include <Core/HashCode.hpp>

#include <iostream>

namespace Hyperion {

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
      m_isStaticMethod(false),
      m_isConst(false)
{
}

void AstMember::Visit(AstVisitor* visitor, Module* mod)
{
    m_symbolType = BuiltinTypes::s_errorType;

    {
        // For STORE operation we should load a reference into register.
        if (m_accessMode == ACCESS_MODE_STORE)
        {
            mod->scopeTree.Open(SCOPE_TYPE_NORMAL, REF_VARIABLE_FLAG);
        }

        Assert(m_target != nullptr);
        m_target->Visit(visitor, mod);

        // Reference scope ends
        if (m_accessMode == ACCESS_MODE_STORE)
        {
            mod->scopeTree.Close();
        }
    }

    bool isProxyClass = false;

    m_accessOptions = m_target->GetAccessOptions();

    bool isStaticMemberAccess = false;

    if (const SymbolType* heldType = m_target->GetHeldType())
    {
        // static member access
        m_targetType = heldType;

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
            m_typeRef.Reset(new AstTypeRef(m_targetType, m_location));
            m_typeRef->Visit(visitor, mod);

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
                if (m_targetType == BuiltinTypes::s_classType)
                {
                    if ((findMemberResult = m_targetType->FindMember(m_fieldName, member, fieldIndex)))
                    {
                        isStaticMemberAccess = false;
                    }
                }
                else
                {
                    findMemberResult = m_targetType->FindStaticMember(m_fieldName, member, fieldIndex);
                }
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

                m_isConst = member.IsConst();

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

        if (const SymbolType* base = m_targetType->GetBaseType())
        {
            // continue up the base type chain.
            m_targetType = base->GetUnaliased();
        }
        else if (m_targetType->IsObject() && !m_targetType->TypeEqual(*BuiltinTypes::s_classType))
        {
            // Finally, allow for member calls to the base Class type. Used for stuff like (GetName(), CreateInstance(), etc.)
            m_targetType = BuiltinTypes::s_classType;
        }
        else
        {
            break;
        }
    }

    Assert(m_targetType != nullptr);

    const bool isStaticMember = m_isStaticField || m_isStaticMethod;
    if (!m_typeRef && (isStaticMember || m_targetType->IsClassType()))
    {
        // Same as above for targetType. Do the same for static members.
        // For STORE operation we should load a reference into register.
        if (m_accessMode == ACCESS_MODE_STORE)
        {
            mod->scopeTree.Open(SCOPE_TYPE_NORMAL, REF_VARIABLE_FLAG);
        }

        m_typeRef.Reset(new AstTypeRef(m_targetType, m_location));
        m_typeRef->Visit(visitor, mod);

        if (m_accessMode == ACCESS_MODE_STORE)
        {
            mod->scopeTree.Close();
        }
    }

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
    Assert(m_targetType != nullptr);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const bool isStaticMember = m_isStaticField || m_isStaticMethod;

    AstMember* targetAsMember = nullptr;
    if (m_accessMode == ACCESS_MODE_STORE && m_target != nullptr)
    {
        targetAsMember = dynamic_cast<AstMember*>(m_target.Get());
    }

    const bool needsWriteback = targetAsMember != nullptr;

    if (needsWriteback)
    {
        // Skip normal target build – we will generate custom bytecode below
        // that preserves the base object or ClassRef in rp while loading the
        // target member value into rp+1, enabling a writeback after the store.
    }
    else if (m_typeRef != nullptr)
    {
        chunk->Append(m_typeRef->Build(visitor, mod));
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

        if (needsWriteback)
        {
            uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            Assert(rp > 0); // rp-1 must hold the source value from the binary assignment

            const HashCode::ValueType targetFieldHash = HashCode::GetHashCode(targetAsMember->m_fieldName.Data()).Value();

            // Step 1: Load the base (ClassRef for static, object for instance) into rp
            if (targetAsMember->m_typeRef != nullptr)
            {
                chunk->Append(targetAsMember->m_typeRef->Build(visitor, mod));
            }
            else if (targetAsMember->m_target != nullptr)
            {
                chunk->Append(targetAsMember->m_target->Build(visitor, mod));
            }

            // Step 2: Load the target member value into rp+1 (preserving the base in rp)
            chunk->Append(BytecodeUtil::Make<Comment>("Load member " + targetAsMember->m_fieldName + " for writeback"));
            auto instrLoadTargetMember = BytecodeUtil::Make<StorageOperation>();
            instrLoadTargetMember->GetBuilder().Load(rp + 1).Member(rp).ByHash(targetFieldHash);
            chunk->Append(std::move(instrLoadTargetMember));

            // Step 3: Store into this member's field on the loaded value in rp+1
            auto instrStoreMember = BytecodeUtil::Make<StorageOperation>();
            instrStoreMember->GetBuilder().Store(rp - 1).Member(rp + 1).ByHash(hash);
            chunk->Append(std::move(instrStoreMember));

            // Step 4: Write back the modified value to the target member via the base in rp
            chunk->Append(BytecodeUtil::Make<Comment>("Writeback to member " + targetAsMember->m_fieldName));
            auto instrWriteback = BytecodeUtil::Make<StorageOperation>();
            instrWriteback->GetBuilder().Store(rp + 1).Member(rp).ByHash(targetFieldHash);
            chunk->Append(std::move(instrWriteback));
        }
        else
        {
            chunk->Append(Compiler::StoreMemberFromHash(visitor, mod, hash));
        }

        break;
    default:
        HYP_UNREACHABLE();
        break;
    }

    return chunk;
}

void AstMember::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_typeRef != nullptr)
    {
        m_typeRef->Optimize(visitor, mod);

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
    if (m_typeRef != nullptr && m_typeRef->MayHaveSideEffects())
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

const SymbolType* AstMember::GetTargetType() const
{
    return m_targetType;
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
    Assert(m_target != nullptr);

    if (m_isConst)
    {
        return false;
    }

    if (m_isStaticField)
    {
        return true;
    }

    return m_target->IsMutable();
}

} // namespace Hyperion
