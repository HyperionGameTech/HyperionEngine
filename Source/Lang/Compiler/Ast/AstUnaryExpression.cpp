#include <Lang/Compiler/Ast/AstUnaryExpression.hpp>
#include <Lang/Compiler/Ast/AstBinaryExpression.hpp>
#include <Lang/Compiler/Ast/AstVariable.hpp>
#include <Lang/Compiler/Ast/AstVariableDeclaration.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>
#include <Lang/Compiler/Ast/AstConstant.hpp>
#include <Lang/Compiler/Ast/AstInteger.hpp>
#include <Lang/Compiler/Ast/AstAsExpression.hpp>
#include <Lang/Compiler/Ast/AstTypeRef.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/Operator.hpp>
#include <Lang/Compiler/Optimizer.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/Configuration.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>

#include <AstUnaryExpression.generated.inl>

namespace Hyperion {

AstUnaryExpression::AstUnaryExpression(
    const SharedPtr<AstExpression>& expr,
    const Operator* op,
    bool isPostfixVersion,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr),
      m_op(op),
      m_isPostfixVersion(isPostfixVersion),
      m_folded(false)
{
}

void AstUnaryExpression::Visit(AstVisitor* visitor, Module* mod)
{
    // use a bin op for operators that modify their argument
    if (m_op->ModifiesValue())
    {
        SharedPtr<AstExpression> expr;
        const Operator* binOp = nullptr;

        switch (m_op->GetOperatorType())
        {
        case OP_increment:
            expr.Reset(new AstInteger(1, CBS_32, m_location));
            binOp = Operator::FindBinaryOperator(Operators::OP_add_assign);

            break;
        case OP_decrement:
            expr.Reset(new AstInteger(1, CBS_32, m_location));
            binOp = Operator::FindBinaryOperator(Operators::OP_subtract_assign);

            break;
        default:
            HYP_UNREACHABLE();
        }

        m_overrideBlock.Reset(new AstBlock(m_location));

        static const String tempVarName = "$__tempPostfixOperand";

        if (m_isPostfixVersion)
        {
            // need to preserve the original value as a temporary variable
            SharedPtr<AstVariableDeclaration> tempVarDecl(new AstVariableDeclaration(
                tempVarName,
                nullptr,
                CloneAstNode(m_expr),
                IdentifierFlags::NONE,
                m_location));

            // add the variable declaration to the block so  it gets stored
            m_overrideBlock->AddChild(tempVarDecl);
        }

        m_binExpr.Reset(new AstBinaryExpression(m_expr, expr, binOp, m_location));

        m_overrideBlock->AddChild(m_binExpr);

        if (m_isPostfixVersion)
        {
            // return the temp variable
            m_overrideBlock->AddChild(SharedPtr<AstVariable>(new AstVariable(tempVarName, m_location)));
        }

        m_overrideBlock->Visit(visitor, mod);

        if (!m_expr->IsMutable())
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_expression_cannot_be_modified,
                m_expr->GetLocation()));
        }

        if (!(m_expr->GetAccessOptions() & AccessMode::ACCESS_MODE_STORE))
        {
            // cannot modify an rvalue
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_cannot_modify_rvalue,
                m_expr->GetLocation()));
        }

        return;
    }

    m_expr->Visit(visitor, mod);

    const SymbolType* type = m_expr->GetExprType();
    Assert(type != nullptr);

    const SymbolType* unaliasedType = type->GetUnaliased();

    if (m_op->GetType() & BITWISE)
    {
        // no bitwise operators on floats allowed.
        visitor->AddErrorIfFalse(
            unaliasedType->IsAnyType() || unaliasedType->IsIntegral() || unaliasedType->IsEnumType(),
            CompilerError(
                LEVEL_ERROR,
                Msg_bitwise_operand_must_be_int,
                m_location,
                type->ToString()));
    }
    else if (m_op->GetType() & ARITHMETIC)
    {
        visitor->AddErrorIfFalse(
            unaliasedType->IsAnyType() || unaliasedType->IsNumber(),
            CompilerError(
                LEVEL_ERROR,
                Msg_invalid_operator_for_type,
                m_expr->GetLocation(),
                m_op->GetOperatorType(),
                type->ToString()));
    }
    else if (m_op->GetType() & LOGICAL)
    {
        // @TODO
        // If it is not boolean, we need to cast it to boolean via an AS expr

        // if (!type->IsBoolean())
        // {
        //     m_expr = SharedPtr<AstAsExpression>(new AstAsExpression(
        //         CloneAstNode(m_expr),
        //         SharedPtr<AstTypeSpecifier>(new AstTypeSpecifier(
        //             SharedPtr<AstTypeRef>(new AstTypeRef(BuiltinTypes::s_boolType, m_location)),
        //             m_location)),
        //         m_location));

        //     m_expr->Visit(visitor, mod);
        // }

        // For now just ensure it's a boolean or any
        visitor->AddErrorIfFalse(
            unaliasedType->IsAnyType() || unaliasedType->IsBoolean(),
            CompilerError(
                LEVEL_ERROR,
                Msg_invalid_operator_for_type,
                m_expr->GetLocation(),
                m_op->GetOperatorType(),
                type->ToString()));
    }
}

UniquePtr<Buildable> AstUnaryExpression::Build(AstVisitor* visitor, Module* mod)
{
    InstructionStreamContextGuard contextGuard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_DEFAULT);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_overrideBlock != nullptr)
    {
        return m_overrideBlock->Build(visitor, mod);
    }

    Assert(m_expr != nullptr);
    chunk->Append(m_expr->Build(visitor, mod));

    if (!m_folded)
    {
        uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        if (m_op->GetType() & LOGICAL)
        {
            switch (m_op->GetOperatorType())
            {
            case Operators::OP_logical_not:
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

                break;
            }
            default:
                HYP_NOT_IMPLEMENTED();
            }
        }
        else
        {
            uint8 opcode = 0;

            switch (m_op->GetOperatorType())
            {
            case Operators::OP_negative:
                opcode = NEG;
                break;
            case Operators::OP_bitwise_complement:
                opcode = NOT;
                break;
            default:
                HYP_NOT_IMPLEMENTED();
            }

            auto oper = BytecodeUtil::Make<RawOperation<>>();
            oper->opcode = opcode;
            oper->Accept<uint8>(rp);
            chunk->Append(std::move(oper));
        }
    }

    return chunk;
}

void AstUnaryExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_overrideBlock != nullptr)
    {
        m_overrideBlock->Optimize(visitor, mod);

        return;
    }

    m_expr->Optimize(visitor, mod);

    if (!m_folded)
    {
        // only try ConstantFold if we aren't already folded
        // otherwise, we may double fold and apply the op twice (e.g. -5 with the `-` applied again, becomes 5)

        if (m_op->GetOperatorType() == Operators::OP_positive)
        {
            // nothing to do for unary plus; just consider it folded

            m_folded = true;
        }
        else if (SharedPtr<AstConstant> constantValue = Optimizer::ConstantFold(m_expr, nullptr, m_op->GetOperatorType(), visitor))
        {
            m_expr = constantValue;

            m_folded = true;
        }
    }
}

SharedPtr<AstStatement> AstUnaryExpression::Clone() const
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
        return m_expr->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstUnaryExpression::MayHaveSideEffects() const
{
    if (m_binExpr != nullptr)
    {
        return m_binExpr->MayHaveSideEffects();
    }

    return m_expr->MayHaveSideEffects();
}

ConstantValue AstUnaryExpression::GetConstantValue() const
{
    if (m_binExpr != nullptr)
    {
        return m_binExpr->GetConstantValue();
    }

    if (m_folded)
    {
        return m_expr->GetConstantValue();
    }

    return ConstantValue(INVALID_CONSTANT_NUMBER);
}

const SymbolType* AstUnaryExpression::GetExprType() const
{
    if (m_binExpr != nullptr)
    {
        return m_binExpr->GetExprType();
    }

    return m_expr->GetExprType();
}

} // namespace Hyperion
