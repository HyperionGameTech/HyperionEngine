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
#include <core/debug/Debug.hpp>

namespace hyperion::compiler {

/** Attempts to evaluate the optimized expression at compile-time. */
static RC<AstConstant> ConstantFold(
    RC<AstExpression>& target,
    Operators opType,
    AstVisitor* visitor)
{
    RC<AstConstant> result;

    if (const AstConstant* targetAsConstant = dynamic_cast<const AstConstant*>(target.Get()))
    {
        result = targetAsConstant->HandleOperator(opType, nullptr);
    }

    return result;
}

AstUnaryExpression::AstUnaryExpression(
    const RC<AstExpression>& target,
    const Operator* op,
    bool isPostfixVersion,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_target(target),
      m_op(op),
      m_isPostfixVersion(isPostfixVersion),
      m_folded(false)
{
}

void AstUnaryExpression::Visit(AstVisitor* visitor, Module* mod)
{
    // TODO: Operator overloading

    // use a bin op for operators that modify their argument
    if (m_op->ModifiesValue())
    {
        RC<AstExpression> expr;
        const Operator* binOp = nullptr;

        switch (m_op->GetOperatorType())
        {
        case OP_increment:
            expr.Reset(new AstInteger(1, m_location));
            binOp = Operator::FindBinaryOperator(Operators::OP_add_assign);

            break;
        case OP_decrement:
            expr.Reset(new AstInteger(1, m_location));
            binOp = Operator::FindBinaryOperator(Operators::OP_subtract_assign);

            break;
        default:
            Assert(false, "Unhandled operator type");
        }

        m_binExpr.Reset(new AstBinaryExpression(
            m_target,
            expr,
            binOp,
            m_location));

        m_binExpr->Visit(visitor, mod);

        return;
    }

    m_target->Visit(visitor, mod);

    SymbolTypeRef type = m_target->GetExprType();

    if (!type->IsAnyType() && !type->IsGenericParameter())
    {
        if (m_op->GetType() & BITWISE)
        {
            // no bitwise operators on floats allowed.
            // do not allow right-hand side to be 'any', because it might change the data type.
            visitor->AddErrorIfFalse(
                type->IsIntegral(),
                CompilerError(
                    LEVEL_ERROR,
                    Msg_bitwise_operand_must_be_int,
                    m_target->GetLocation(),
                    type->ToString()));
        }
        else if (m_op->GetType() & ARITHMETIC)
        {
            visitor->AddErrorIfFalse(
                type->IsNumber(),
                CompilerError(
                    LEVEL_ERROR,
                    Msg_invalid_operator_for_type,
                    m_target->GetLocation(),
                    m_op->GetOperatorType(),
                    type->ToString()));
        }
    }

    if (m_op->ModifiesValue())
    {
        if (!m_target->IsMutable())
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_expression_cannot_be_modified,
                m_target->GetLocation()));
        }

        if (!(m_target->GetAccessOptions() & AccessMode::ACCESS_MODE_STORE))
        {
            // cannot modify an rvalue
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_cannot_modify_rvalue,
                m_target->GetLocation()));
        }
    }
}

UniquePtr<Buildable> AstUnaryExpression::Build(AstVisitor* visitor, Module* mod)
{
    InstructionStreamContextGuard contextGuard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_DEFAULT);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    uint8 rp;

    if (m_binExpr != nullptr)
    {
        if (m_isPostfixVersion)
        {
            // for postfix version:
            //  - load var into register
            //  - do binary op
            //  - return value from original register

            /// @FIXME: Issues with array access.
            Assert(m_target != nullptr);
            chunk->Append(m_target->Build(visitor, mod));

            visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            chunk->Append(m_binExpr->Build(visitor, mod));

            visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            return chunk;
        }
        else
        {
            return m_binExpr->Build(visitor, mod);
        }
    }

    Assert(m_target != nullptr);
    chunk->Append(m_target->Build(visitor, mod));

    if (!m_folded)
    {
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        if (m_op->GetType() & ARITHMETIC)
        {
            uint8 opcode = 0;

            if (m_op->GetOperatorType() == Operators::OP_negative)
            {
                opcode = NEG;
            }

            auto oper = BytecodeUtil::Make<RawOperation<>>();
            oper->opcode = opcode;
            oper->Accept<uint8>(rp);
            chunk->Append(std::move(oper));
        }
        else if (m_op->GetType() & LOGICAL)
        {
            if (m_op->GetOperatorType() == Operators::OP_logical_not)
            {
                // the label to jump to the very end, and set the result to false
                LabelId falseLabel = contextGuard->NewLabel();
                chunk->TakeOwnershipOfLabel(falseLabel);

                LabelId trueLabel = contextGuard->NewLabel();
                chunk->TakeOwnershipOfLabel(trueLabel);

                // compare lhs to 0 (false)
                chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, rp));

                // jump if they are not equal: i.e the value is true
                chunk->Append(BytecodeUtil::Make<Jump>(Jump::JE, trueLabel));

                // didn't skip past: load the false value
                chunk->Append(BytecodeUtil::Make<ConstBool>(rp, false));

                // now, jump to the very end so we don't load the true value.
                chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, falseLabel));

                // skip to here to load true
                chunk->Append(BytecodeUtil::Make<LabelMarker>(trueLabel));

                // get current register index
                rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

                // here is where the value is true
                chunk->Append(BytecodeUtil::Make<ConstBool>(rp, true));

                // skip to here to avoid loading 'true' into the register
                chunk->Append(BytecodeUtil::Make<LabelMarker>(falseLabel));
            }
            else
            {
                Assert(false, "Operator not implemented: %s", Operator::FindUnaryOperator(m_op->GetOperatorType())->LookupStringValue().Data());
            }
        }
        else
        {
            Assert(false, "Operator not implemented %s", Operator::FindUnaryOperator(m_op->GetOperatorType())->LookupStringValue().Data());
        }
    }

    return chunk;
}

void AstUnaryExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_binExpr != nullptr)
    {
        m_binExpr->Optimize(visitor, mod);

        return;
    }

    m_target->Optimize(visitor, mod);

    if (m_op->GetOperatorType() == Operators::OP_positive)
    {
        m_folded = true;
    }
    else if (auto constantValue = ConstantFold(
                 m_target,
                 m_op->GetOperatorType(),
                 visitor))
    {
        m_target = constantValue;
        m_folded = true;
    }
}

RC<AstStatement> AstUnaryExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstUnaryExpression::IsTrue() const
{
    if (m_binExpr != nullptr)
    {
        return m_binExpr->IsTrue();
    }

    if (m_folded)
    {
        return m_target->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstUnaryExpression::MayHaveSideEffects() const
{
    if (m_binExpr != nullptr)
    {
        return m_binExpr->MayHaveSideEffects();
    }

    return m_target->MayHaveSideEffects();
}

SymbolTypeRef AstUnaryExpression::GetExprType() const
{
    if (m_binExpr != nullptr)
    {
        return m_binExpr->GetExprType();
    }

    return m_target->GetExprType();
}

} // namespace hyperion::compiler
