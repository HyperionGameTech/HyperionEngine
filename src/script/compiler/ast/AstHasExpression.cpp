#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstTypeSpecification.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>
#include <script/Hasher.hpp>

#include <iostream>

namespace hyperion {
namespace compiler {

AstHasExpression::AstHasExpression(
    const std::shared_ptr<AstStatement> &target,
    const std::string &field_name,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_target(target),
      m_field_name(field_name),
      m_has_member(-1),
      m_is_expr(false),
      m_has_side_effects(false)
{
}

void AstHasExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);
    m_target->Visit(visitor, mod);

    SymbolTypePtr_t target_type;

    if (auto *ident = dynamic_cast<AstIdentifier*>(m_target.get())) {
        if (ident->GetProperties().GetIdentifierType() == IDENTIFIER_TYPE_VARIABLE) {
            m_is_expr = true;
        }

        target_type = ident->GetSymbolType();
        m_has_side_effects = ident->MayHaveSideEffects();
    } else if (auto *type_spec = dynamic_cast<AstTypeSpecification*>(m_target.get())) {
        target_type = type_spec->GetSymbolType();
    } else if (auto *expr = dynamic_cast<AstExpression*>(m_target.get())) {
        target_type = ident->GetSymbolType();
        m_is_expr = true;
        m_has_side_effects = expr->MayHaveSideEffects();
    }

    AssertThrow(target_type != nullptr);
    if (target_type != BuiltinTypes::ANY) {
        if (SymbolTypePtr_t member_type = target_type->FindMember(m_field_name)) {
            m_has_member = 1;
        } else {
            m_has_member = 0;
        }
    } else {
        m_has_member = -1;
    }
}

std::unique_ptr<Buildable> AstHasExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (!m_is_expr) {
        AssertThrowMsg(m_has_member != -1, "m_has_member should only be -1 for expression member checks.");
    }

    if (m_has_member != -1) {
        // get active register
        uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        if (m_has_member == 1) {
            // load value into register
            chunk->Append(BytecodeUtil::Make<ConstBool>(rp, true));
        } else if (m_has_member == 0) {
            // load value into register
            chunk->Append(BytecodeUtil::Make<ConstBool>(rp, false));
        }

        // build in the target only if it has side effects.
        // this is for compatibility with the runtime check,
        // as the runtime check has to be evaluated
        if (m_has_side_effects) {
            if (auto *expr = dynamic_cast<AstExpression*>(m_target.get())) {
                chunk->Append(m_target->Build(visitor, mod));
            }
        }
    } else {
        // indeterminate at compile time.
        // check at runtime.
        const uint32_t hash = hash_fnv_1(m_field_name.c_str());

        int found_member_reg = -1;

        // the label to jump to the very end
        LabelId end_label = chunk->NewLabel();
        // the label to jump to the else-part
        LabelId else_label = chunk->NewLabel();
        
        chunk->Append(m_target->Build(visitor, mod));

        // get active register
        uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        { // compile in the instruction to check if it has the member
            auto instr_has_mem_hash = BytecodeUtil::Make<RawOperation<>>();
            instr_has_mem_hash->opcode = HAS_MEM_HASH;
            instr_has_mem_hash->Accept<uint8_t>(rp);
            instr_has_mem_hash->Accept<uint8_t>(rp);
            instr_has_mem_hash->Accept<uint32_t>(hash);
            chunk->Append(std::move(instr_has_mem_hash));
        }

        found_member_reg = rp;

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

    return std::move(chunk);
}

void AstHasExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);

    m_target->Optimize(visitor, mod);
}

Pointer<AstStatement> AstHasExpression::Clone() const
{
    return CloneImpl();
}

SymbolTypePtr_t AstHasExpression::GetSymbolType() const
{
    return BuiltinTypes::BOOLEAN;
}

Tribool AstHasExpression::IsTrue() const
{
    return Tribool((Tribool::TriboolValue)m_has_member);
}

bool AstHasExpression::MayHaveSideEffects() const
{
    return m_has_side_effects;
}

} // namespace compiler
} // namespace hyperion
