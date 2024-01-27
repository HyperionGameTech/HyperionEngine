#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>
#include <script/Hasher.hpp>

#include <core/lib/String.hpp>

#include <iostream>

namespace hyperion::compiler {

AstHasExpression::AstHasExpression(
    const RC<AstStatement> &target,
    const String &field_name,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_target(target),
    m_field_name(field_name),
    m_has_member(TRI_INDETERMINATE),
    m_is_expr(false),
    m_has_side_effects(false)
{
}

void AstHasExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);
    m_target->Visit(visitor, mod);

    SymbolTypePtr_t target_type;

    if (auto *ident = dynamic_cast<AstIdentifier *>(m_target.Get())) {
        if (ident->GetProperties().GetIdentifierType() == IDENTIFIER_TYPE_VARIABLE) {
            m_is_expr = true;
        }

        target_type = ident->GetExprType();
        m_has_side_effects = ident->MayHaveSideEffects();
    } else if (auto *type_spec = dynamic_cast<AstPrototypeSpecification *>(m_target.Get())) {
        target_type = type_spec->GetHeldType();
    } else if (auto *expr = dynamic_cast<AstExpression *>(m_target.Get())) {
        target_type = expr->GetExprType();
        m_is_expr = true;
        m_has_side_effects = expr->MayHaveSideEffects();
    }

    AssertThrow(target_type != nullptr);

    if (target_type->IsAnyType() || target_type->IsPlaceholderType()) {
        m_has_member = TRI_INDETERMINATE;
    } else if (target_type->IsClass()) {
        SymbolTypeMember member;

        if (target_type->FindMemberDeep(m_field_name, member)) {
            m_has_member = TRI_TRUE;
        } else {
            // @TODO: If we have 'final' classes,
            // we could make this return false.
            // we have to do a run-time check as there could always be a deriving class
            // which has this member.
            m_has_member = TRI_INDETERMINATE;
        }
    } else if (target_type->IsPrimitive()) {
        m_has_member = TRI_FALSE;
    } else {
        m_has_member = TRI_INDETERMINATE;
    }
}

std::unique_ptr<Buildable> AstHasExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);

    InstructionStreamContextGuard context_guard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_DEFAULT
    );

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (!m_is_expr) {
        AssertThrowMsg(m_has_member != TRI_INDETERMINATE, "m_has_member should only be -1 for expression member checks.");
    }

    if (m_has_member != TRI_INDETERMINATE && !m_has_side_effects) {
        // get active register
        UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        if (m_has_member == TRI_TRUE) {
            // load value into register
            chunk->Append(BytecodeUtil::Make<ConstBool>(rp, true));
        } else if (m_has_member == TRI_FALSE) {
            // load value into register
            chunk->Append(BytecodeUtil::Make<ConstBool>(rp, false));
        }
    } else {
        // indeterminate at compile time.
        // check at runtime.
        const HashFNV1 hash = hash_fnv_1(m_field_name.Data());

        // the label to jump to the very end
        LabelId end_label = context_guard->NewLabel();
        chunk->TakeOwnershipOfLabel(end_label);

        // the label to jump to the else-part
        LabelId else_label = context_guard->NewLabel();
        chunk->TakeOwnershipOfLabel(else_label);
        
        chunk->Append(m_target->Build(visitor, mod));

        // get active register
        UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        { // compile in the instruction to check if it has the member
            auto instr_has_mem_hash = BytecodeUtil::Make<RawOperation<>>();
            instr_has_mem_hash->opcode = HAS_MEM_HASH;
            instr_has_mem_hash->Accept<UInt8>(rp);
            instr_has_mem_hash->Accept<UInt8>(rp);
            instr_has_mem_hash->Accept<UInt32>(hash);
            chunk->Append(std::move(instr_has_mem_hash));

            chunk->Append(BytecodeUtil::Make<Comment>("Check if object has member " + m_field_name));
        }

        const UInt8 found_member_reg = rp;

        // compare the found member to zero
        chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, found_member_reg));
        // jump if condition is false or zero.
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JE, else_label));
        // the member was found here, so load true
        chunk->Append(BytecodeUtil::Make<ConstBool>(rp, true));
        // jump to end after loading true
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, end_label));

        chunk->Append(BytecodeUtil::Make<LabelMarker>(else_label));
        // member was not found, so load false
        chunk->Append(BytecodeUtil::Make<ConstBool>(rp, false));
        chunk->Append(BytecodeUtil::Make<LabelMarker>(end_label));
    }

    return chunk;
}

void AstHasExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);

    m_target->Optimize(visitor, mod);
}

RC<AstStatement> AstHasExpression::Clone() const
{
    return CloneImpl();
}

SymbolTypePtr_t AstHasExpression::GetExprType() const
{
    return BuiltinTypes::BOOLEAN;
}

Tribool AstHasExpression::IsTrue() const
{
    return m_has_member;
}

bool AstHasExpression::MayHaveSideEffects() const
{
    return m_has_side_effects;
}

} // namespace hyperion::compiler
