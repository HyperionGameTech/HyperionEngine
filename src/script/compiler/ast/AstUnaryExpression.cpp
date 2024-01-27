#include <script/compiler/ast/AstUnaryExpression.hpp>
#include <script/compiler/ast/AstBinaryExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstConstant.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/Operator.hpp>
#include <script/compiler/Optimizer.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

namespace hyperion::compiler {

/** Attempts to evaluate the optimized expression at compile-time. */
static RC<AstConstant> ConstantFold(
    RC<AstExpression> &target, 
    Operators op_type,
    AstVisitor *visitor
)
{
    RC<AstConstant> result;

    if (const AstConstant *target_as_constant = dynamic_cast<const AstConstant *>(target.Get())) {
        result = target_as_constant->HandleOperator(op_type, nullptr);
    }

    return result;
}

AstUnaryExpression::AstUnaryExpression(
    const RC<AstExpression> &target,
    const Operator *op,
    bool is_postfix_version,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_target(target),
    m_op(op),
    m_is_postfix_version(is_postfix_version),
    m_folded(false)
{
}

void AstUnaryExpression::Visit(AstVisitor *visitor, Module *mod)
{
    // TODO: Overloading to match binop

    // use a bin op for operators that modify their argument
    if (m_op->ModifiesValue()) {
        RC<AstExpression> expr;
        const Operator *bin_op = nullptr;

        switch (m_op->GetOperatorType()) {
        case OP_increment:
            expr.Reset(new AstInteger(1, m_location));
            bin_op = Operator::FindBinaryOperator(Operators::OP_add_assign);

            break;
        case OP_decrement:
            expr.Reset(new AstInteger(1, m_location));
            bin_op = Operator::FindBinaryOperator(Operators::OP_subtract_assign);

            break;
        default:
            AssertThrowMsg(false, "Unhandled operator type");
        }

        m_bin_expr.Reset(new AstBinaryExpression(
            m_target,
            expr,
            bin_op,
            m_location
        ));

        m_bin_expr->Visit(visitor, mod);

        return;
    }

    m_target->Visit(visitor, mod);

    SymbolTypePtr_t type = m_target->GetExprType();

    if (!type->IsAnyType() && !type->IsGenericParameter() && !type->IsPlaceholderType()) {
        if (m_op->GetType() & BITWISE) {
            // no bitwise operators on floats allowed.
            // do not allow right-hand side to be 'any', because it might change the data type.
            visitor->Assert(
                type->IsIntegral(),
                CompilerError(
                    LEVEL_ERROR,
                    Msg_bitwise_operand_must_be_int,
                    m_target->GetLocation(),
                    type->ToString()
                )
            );
        } else if (m_op->GetType() & ARITHMETIC) {
            visitor->Assert(
                type->IsNumber(),
                CompilerError(
                    LEVEL_ERROR,
                    Msg_invalid_operator_for_type,
                    m_target->GetLocation(),
                    m_op->GetOperatorType(),
                    type->ToString()
                )
            );
        }
    }

    if (m_op->ModifiesValue()) {
        if (!m_target->IsMutable()) {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_expression_cannot_be_modified,
                m_target->GetLocation()
            ));
        }

        if (!(m_target->GetAccessOptions() & AccessMode::ACCESS_MODE_STORE)) {
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
    InstructionStreamContextGuard context_guard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_DEFAULT
    );

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    UInt8 rp;

    if (m_bin_expr != nullptr) {
        if (m_is_postfix_version) {
            // for postfix version:
            //  - load var into register
            //  - do binary op
            //  - return value from original register

            AssertThrow(m_target != nullptr);
            chunk->Append(m_target->Build(visitor, mod));

            visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            chunk->Append(m_bin_expr->Build(visitor, mod));

            visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            return chunk;
        } else {
            return m_bin_expr->Build(visitor, mod);
        }
    }

    AssertThrow(m_target != nullptr);
    chunk->Append(m_target->Build(visitor, mod));

    if (!m_folded) {
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        if (m_op->GetType() & ARITHMETIC) {
            UInt8 opcode = 0;

            if (m_op->GetOperatorType() == Operators::OP_negative) {
                opcode = NEG;
            }

            auto oper = BytecodeUtil::Make<RawOperation<>>();
            oper->opcode = opcode;
            oper->Accept<UInt8>(rp);
            chunk->Append(std::move(oper));
        } else if (m_op->GetType() & LOGICAL) {
            if (m_op->GetOperatorType() == Operators::OP_logical_not) {
                // the label to jump to the very end, and set the result to false
                LabelId false_label = context_guard->NewLabel();
                chunk->TakeOwnershipOfLabel(false_label);

                LabelId true_label = context_guard->NewLabel();
                chunk->TakeOwnershipOfLabel(true_label);

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
            } else {
                AssertThrowMsg(false, "Operator not implemented: %s", Operator::FindUnaryOperator(m_op->GetOperatorType())->LookupStringValue().Data());
            }
        } else {
            AssertThrowMsg(false, "Operator not implemented %s", Operator::FindUnaryOperator(m_op->GetOperatorType())->LookupStringValue().Data());
        }
    }

    return chunk;
}

void AstUnaryExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    if (m_bin_expr != nullptr) {
        m_bin_expr->Optimize(visitor, mod);

        return;
    }

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

RC<AstStatement> AstUnaryExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstUnaryExpression::IsTrue() const
{
    if (m_bin_expr != nullptr) {
        return m_bin_expr->IsTrue();
    }

    if (m_folded) {
        return m_target->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstUnaryExpression::MayHaveSideEffects() const
{
    if (m_bin_expr != nullptr) {
        return m_bin_expr->MayHaveSideEffects();
    }

    return m_target->MayHaveSideEffects();
}

SymbolTypePtr_t AstUnaryExpression::GetExprType() const
{
    if (m_bin_expr != nullptr) {
        return m_bin_expr->GetExprType();
    }

    return m_target->GetExprType();
}

} // namespace hyperion::compiler
