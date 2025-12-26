#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

#include <core/HashCode.hpp>

#include <core/containers/String.hpp>

#include <iostream>

namespace Hyperion {

AstHasExpression::AstHasExpression(
    const RC<AstStatement>& target,
    const String& fieldName,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_target(target),
      m_fieldName(fieldName),
      m_hasMember(TRI_INDETERMINATE),
      m_isExpr(false),
      m_hasSideEffects(false)
{
}

void AstHasExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_target != nullptr);
    m_target->Visit(visitor, mod);

    const SymbolType* targetType = nullptr;

    if (auto* ident = dynamic_cast<AstIdentifier*>(m_target.Get()))
    {
        if (ident->GetProperties().GetIdentifierType() == IDENTIFIER_TYPE_VARIABLE)
        {
            m_isExpr = true;
        }

        targetType = ident->GetExprType();
        m_hasSideEffects = ident->MayHaveSideEffects();
    }
    else if (auto* typeSpec = dynamic_cast<AstTypeSpecifier*>(m_target.Get()))
    {
        targetType = typeSpec->GetHeldType();
    }
    else if (auto* expr = dynamic_cast<AstExpression*>(m_target.Get()))
    {
        targetType = expr->GetExprType();
        m_isExpr = true;
        m_hasSideEffects = expr->MayHaveSideEffects();
    }

    Assert(targetType != nullptr);

    if (targetType->IsAnyType())
    {
        m_hasMember = TRI_INDETERMINATE;
    }
    else if (targetType->IsPrimitive())
    {
        m_hasMember = TRI_FALSE;
    }
    else
    {
        SymbolTypeMember member;

        if (targetType->FindMemberDeep(m_fieldName, member))
        {
            m_hasMember = TRI_TRUE;
        }
        else
        {
            /*! \todo : If we have 'final' classes, we could make this return false. we have to do a run-time check as there could always be a deriving class which has this member. */
            m_hasMember = TRI_INDETERMINATE;
        }
    }
}

UniquePtr<Buildable> AstHasExpression::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_target != nullptr);

    InstructionStreamContextGuard contextGuard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_DEFAULT);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (!m_isExpr)
    {
        Assert(m_hasMember != TRI_INDETERMINATE, "m_hasMember should only be -1 for expression member checks.");
    }

    if (m_hasMember != TRI_INDETERMINATE && !m_hasSideEffects)
    {
        // get active register
        uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        if (m_hasMember == TRI_TRUE)
        {
            // load value into register
            chunk->Append(BytecodeUtil::Make<ConstBool>(rp, true));
        }
        else if (m_hasMember == TRI_FALSE)
        {
            // load value into register
            chunk->Append(BytecodeUtil::Make<ConstBool>(rp, false));
        }
    }
    else
    {
        // indeterminate at compile time.
        // check at runtime.
        const uint64 hash = HashCode::GetHashCode(m_fieldName.Data()).Value();

        // the label to jump to the very end
        LabelId endLabel = contextGuard->NewLabel();
        chunk->TakeOwnershipOfLabel(endLabel);

        // the label to jump to the else-part
        LabelId elseLabel = contextGuard->NewLabel();
        chunk->TakeOwnershipOfLabel(elseLabel);

        chunk->Append(m_target->Build(visitor, mod));

        // get active register
        uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        { // compile in the instruction to check if it has the member
            auto instrHasMemHash = BytecodeUtil::Make<RawOperation<>>();
            instrHasMemHash->opcode = CHECK_HAS_MEMBER;
            instrHasMemHash->Accept<uint8>(rp);
            instrHasMemHash->Accept<uint8>(rp);
            instrHasMemHash->Accept<uint64>(hash);
            chunk->Append(std::move(instrHasMemHash));

            chunk->Append(BytecodeUtil::Make<Comment>("Check if object has member " + m_fieldName));
        }

        const uint8 foundMemberReg = rp;

        // compare the found member to zero
        chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, foundMemberReg));
        // jump if condition is false or zero.
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JE, elseLabel));
        // the member was found here, so load true
        chunk->Append(BytecodeUtil::Make<ConstBool>(rp, true));
        // jump to end after loading true
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, endLabel));

        chunk->Append(BytecodeUtil::Make<LabelMarker>(elseLabel));
        // member was not found, so load false
        chunk->Append(BytecodeUtil::Make<ConstBool>(rp, false));
        chunk->Append(BytecodeUtil::Make<LabelMarker>(endLabel));
    }

    return chunk;
}

void AstHasExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_target != nullptr);

    m_target->Optimize(visitor, mod);
}

RC<AstStatement> AstHasExpression::Clone() const
{
    return CloneImpl();
}

const SymbolType* AstHasExpression::GetExprType() const
{
    return BuiltinTypes::s_boolType;
}

Tribool AstHasExpression::IsTrue() const
{
    return m_hasMember;
}

bool AstHasExpression::MayHaveSideEffects() const
{
    return m_hasSideEffects;
}

} // namespace Hyperion
