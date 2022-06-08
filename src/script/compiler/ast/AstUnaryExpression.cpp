#include <script/compiler/ast/AstUnaryExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstConstant.hpp>
#include <script/compiler/Operator.hpp>
#include <script/compiler/Optimizer.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>

namespace hyperion {
namespace compiler {

/** Attempts to evaluate the optimized expression at compile-time. */
static std::shared_ptr<AstConstant> ConstantFold(
    std::shared_ptr<AstExpression> &target, 
    Operators op_type,
    AstVisitor *visitor)
{
    std::shared_ptr<AstConstant> result;

    if (AstConstant *target_as_constant = dynamic_cast<AstConstant*>(target.get())) {
        result = target_as_constant->HandleOperator(op_type, nullptr);
    }

    return result;
}

AstUnaryExpression::AstUnaryExpression(const std::shared_ptr<AstExpression> &target,
    const Operator *op,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_target(target),
      m_op(op),
      m_folded(false)
{
}

void AstUnaryExpression::Visit(AstVisitor *visitor, Module *mod)
{
    m_target->Visit(visitor, mod);

    SymbolTypePtr_t type = m_target->GetSymbolType();
    
    if (m_op->GetType() & BITWISE) {
        // no bitwise operators on floats allowed.
        // do not allow right-hand side to be 'Any', because it might change the data type.
        visitor->Assert((type == BuiltinTypes::INT || 
            type == BuiltinTypes::NUMBER ||
            type == BuiltinTypes::ANY),
            CompilerError(
                LEVEL_ERROR,
                Msg_bitwise_operand_must_be_int,
                m_target->GetLocation(),
                type->GetName()
            )
        );
    } else if (m_op->GetType() & ARITHMETIC) {
        if (type != BuiltinTypes::ANY &&
            type != BuiltinTypes::INT &&
            type != BuiltinTypes::FLOAT &&
            type != BuiltinTypes::NUMBER) {
        
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_invalid_operator_for_type,
                m_target->GetLocation(),
                m_op->LookupStringValue(),
                type->GetName()
            )); 
        }

        visitor->Assert(type == BuiltinTypes::INT ||
            type == BuiltinTypes::FLOAT ||
            type == BuiltinTypes::NUMBER ||
            type == BuiltinTypes::ANY,
            CompilerError(
                LEVEL_ERROR,
                Msg_invalid_operator_for_type,
                m_target->GetLocation(),
                m_op->GetOperatorType(),
                type->GetName()
            )
        );
    }

    if (m_op->ModifiesValue()) {
        if (type->IsConstType()) {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_const_modified,
                m_target->GetLocation()
            ));
        }

        /*if (AstVariable *target_as_var = dynamic_cast<AstVariable*>(m_target.get())) {
            if (target_as_var->GetProperties().GetIdentifier() != nullptr) {
                // make sure we are not modifying a const
                if (target_as_var->GetProperties().GetIdentifier()->GetFlags() & FLAG_CONST) {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_const_modified,
                        m_target->GetLocation(),
                        target_as_var->GetName()
                    ));
                }
            }
        } else*/ if (!(m_target->GetAccessOptions() & AccessMode::ACCESS_MODE_STORE)) {
            // cannot modify an rvalue
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_cannot_modify_rvalue,
                m_target->GetLocation()
            ));
        }
    }
}

std::unique_ptr<Buildable> AstUnaryExpression::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    AssertThrow(m_target != nullptr);

    chunk->Append(m_target->Build(visitor, mod));

    if (!m_folded) {
        uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        if (m_op->GetType() == ARITHMETIC) {
            uint8_t opcode = 0;

            if (m_op->GetOperatorType() == Operators::OP_negative) {
                opcode = NEG;
            }

            auto oper = BytecodeUtil::Make<RawOperation<>>();
            oper->opcode = opcode;
            oper->Accept<uint8_t>(rp);
            chunk->Append(std::move(oper));
        } else if (m_op->GetType() == LOGICAL) {
            if (m_op->GetOperatorType() == Operators::OP_logical_not) {
                // the label to jump to the very end, and set the result to false
                LabelId false_label = chunk->NewLabel();
                LabelId true_label = chunk->NewLabel();;

                // compare lhs to 0 (false)
                chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, rp));

                // jump if they are not equal: i.e the value is true
                chunk->Append(BytecodeUtil::Make<Jump>(Jump::JE, true_label));

                // didn't skip past: load the false value
                chunk->Append(BytecodeUtil::Make<ConstBool>(rp, false));

                // now, jump to the very end so we don't load the true value.
                chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, false_label));

                // skip to here to load true
                chunk->Append(BytecodeUtil::Make<LabelMarker>(true_label));

                // get current register index
                rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
                
                // here is where the value is true
                chunk->Append(BytecodeUtil::Make<ConstBool>(rp, true));

                // skip to here to avoid loading 'true' into the register
                chunk->Append(BytecodeUtil::Make<LabelMarker>(false_label));
            }
        }
    }

    return std::move(chunk);
}

void AstUnaryExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    m_target->Optimize(visitor, mod);

    if (m_op->GetOperatorType() == Operators::OP_positive) {
        m_folded = true;
    } else if (auto constant_value = ConstantFold(
        m_target,
        m_op->GetOperatorType(),
        visitor
    )) {
        m_target = constant_value;
        m_folded = true;
    }
}

Pointer<AstStatement> AstUnaryExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstUnaryExpression::IsTrue() const
{
    if (m_folded) {
        return m_target->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstUnaryExpression::MayHaveSideEffects() const
{
    return m_target->MayHaveSideEffects();
}

SymbolTypePtr_t AstUnaryExpression::GetSymbolType() const
{
    return m_target->GetSymbolType();
}

} // namespace compiler
} // namespace hyperion
