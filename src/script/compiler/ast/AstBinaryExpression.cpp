#include <script/compiler/ast/AstBinaryExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/ast/AstMemberCallExpression.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstTernaryExpression.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/Operator.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Optimizer.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Module.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>

namespace hyperion {

AstBinaryExpression::AstBinaryExpression(
    const RC<AstExpression>& left,
    const RC<AstExpression>& right,
    const Operator* op,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_left(left),
      m_right(right),
      m_op(op),
      m_enableOverrideExpr(true)
{
}

void AstBinaryExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_left != nullptr);
    Assert(m_right != nullptr);

#if HYP_SCRIPT_ENABLE_LAZY_DECLARATIONS
    // check for lazy declaration first
    if ((m_variableDeclaration = CheckLazyDeclaration(visitor, mod)))
    {
        m_variableDeclaration->Visit(visitor, mod);
        // return, our work here is done
        return;
    }
#endif

    if (m_op->ModifiesValue())
    {
        visitor->AddErrorIfFalse(
            m_left->GetAccessOptions() & AccessMode::ACCESS_MODE_STORE,
            CompilerError(
                LEVEL_ERROR,
                Msg_cannot_modify_rvalue,
                m_location));

        m_left->SetAccessMode(AccessMode::ACCESS_MODE_STORE);
    }

    m_left->Visit(visitor, mod);

    // ensure we can modify the left hand side
    if (m_op->ModifiesValue())
    {
        visitor->AddErrorIfFalse(
            m_left->IsMutable(),
            CompilerError(
                LEVEL_ERROR,
                Msg_expression_cannot_be_modified,
                m_location));
    }

    // operator overloading
    if (m_enableOverrideExpr)
    {
        if (m_op->ModifiesValue() && m_op->GetOperatorType() != OP_assign)
        {
            // transform a += b into a = a + b if not a number type
            const Operator* nonAssignmentOp = Operator::GetNonAssignmentVariant(m_op);
            Assert(nonAssignmentOp != nullptr);

            if (nonAssignmentOp != m_op)
            {
                // ensure it won't cause infinite recursion due to creating nested override exprs:
                Assert(!nonAssignmentOp->ModifiesValue());

                RC<AstBinaryExpression> subBinExpr(new AstBinaryExpression(
                    CloneAstNode(m_left),
                    RC<AstBinaryExpression>(new AstBinaryExpression(
                        CloneAstNode(m_left),
                        CloneAstNode(m_right),
                        nonAssignmentOp,
                        m_location)),
                    Operator::FindBinaryOperator(Operators::OP_assign),
                    m_location));

                m_overrideExpr = std::move(subBinExpr);
            }
        }
        else if (m_op->SupportsOverloading())
        {
            // look for operator overloading
            SymbolTypeRef targetType = m_left->GetExprType();
            Assert(targetType != nullptr);
            targetType = targetType->GetUnaliased();

            const String operatorString = m_op->LookupStringValue();
            const String overloadFunctionName = "operator" + operatorString;

            RC<AstExpression> callOperatorOverloadExpr;

            callOperatorOverloadExpr.Reset(new AstMemberCallExpression(
                overloadFunctionName,
                CloneAstNode(m_left),
                RC<AstArgumentList>(new AstArgumentList(
                    { RC<AstArgument>(new AstArgument(
                        CloneAstNode(m_right),
                        false,
                        false,
                        false,
                        false,
                        "other",
                        m_location)) },
                    m_location)), // use right hand side as arg
                m_location));

            if (targetType->IsProxyClass() && targetType->FindMember(overloadFunctionName))
            {
                m_overrideExpr.Reset(new AstCallExpression(
                    RC<AstMember>(new AstMember(
                        overloadFunctionName,
                        CloneAstNode(m_left),
                        m_location)),
                    { RC<AstArgument>(new AstArgument(
                        CloneAstNode(m_right),
                        false,
                        false,
                        false,
                        false,
                        "other",
                        m_location)) },
                    true,
                    m_location));
            }

            // for ANY type we conditionally build in a check
            // also, for proxy class that does not have the operator overloaded,
            // we build in the condition as well
            else if (targetType->IsAnyType())
            {
                RC<AstBinaryExpression> subBinExpr = Clone().CastUnchecked<AstBinaryExpression>();
                subBinExpr->SetEnableOverrideExpr(false);

                m_overrideExpr.Reset(new AstTernaryExpression(
                    RC<AstHasExpression>(new AstHasExpression(
                        CloneAstNode(m_left),
                        overloadFunctionName,
                        m_location)),
                    callOperatorOverloadExpr,
                    subBinExpr,
                    m_location));
            }
            else if (targetType->FindMemberDeep(overloadFunctionName))
            {
                m_overrideExpr = callOperatorOverloadExpr;
            }
        }
    }

    if (m_overrideExpr != nullptr)
    {
        m_overrideExpr->Visit(visitor, mod);

        return;
    }

    // not overloading an operator from this point on,
    // but still have to be aware of Any types.
    m_right->Visit(visitor, mod);

    SymbolTypeRef leftType = m_left->GetExprType();
    Assert(leftType != nullptr);
    leftType = leftType->GetUnaliased();

    SymbolTypeRef rightType = m_right->GetExprType();
    Assert(rightType != nullptr);
    rightType = rightType->GetUnaliased();

    if (m_op->GetType() & BITWISE)
    {
        // no bitwise operators on floats allowed.
        visitor->AddErrorIfFalse(
            (leftType->IsAnyType() || leftType->IsIntegral() || leftType->IsEnumType())
                && (rightType->IsAnyType() || rightType->IsIntegral() || rightType->IsEnumType()),
            CompilerError(
                LEVEL_ERROR,
                Msg_bitwise_operands_must_be_int,
                m_location,
                leftType->GetName(),
                rightType->GetName()));
    }
    else if (m_op->GetType() & ARITHMETIC)
    {
        // for string '+' handling, we need handle the edgecase of `<any> + <string>`, allowing typecheck to be defered to runtime
        const bool isStringConcat = m_op->GetOperatorType() == OP_add
            && (leftType->IsString() || leftType->IsAnyType()) && rightType->IsString();

        // arithmetic operators are only for numbers
        visitor->AddErrorIfFalse(
            ((leftType->IsAnyType() || leftType->IsNumber()) && (rightType->IsAnyType() || rightType->IsNumber())) || isStringConcat,
            CompilerError(
                LEVEL_ERROR,
                Msg_arithmetic_operands_must_be_numbers,
                m_location,
                m_op->LookupStringValue(),
                leftType->GetName(),
                rightType->GetName()));
    }

    if (m_op->ModifiesValue())
    {
        SemanticAnalyzer::Helpers::EnsureTypeAssignmentCompatibility(
            visitor,
            mod,
            leftType,
            rightType,
            /* strictEnum */ true,
            m_location);
    }
    else
    {
        // compare both sides because assignment does not matter in this case
        if (!leftType->TypeCompatible(
                *rightType,
                /* strictNumbers */ false,
                /* strictAny */ false,
                /* strictEnum */ false))
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_mismatched_types,
                m_location,
                leftType->GetName(),
                rightType->GetName()));
        }
    }
}

UniquePtr<Buildable> AstBinaryExpression::Build(AstVisitor* visitor, Module* mod)
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->Build(visitor, mod);
    }

#if HYP_SCRIPT_ENABLE_LAZY_DECLARATIONS
    if (m_variableDeclaration != nullptr)
    {
        return m_variableDeclaration->Build(visitor, mod);
    }
#endif

    InstructionStreamContextGuard contextGuard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_DEFAULT);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    chunk->Append(BytecodeUtil::Make<Comment>("Binary expression: " + ToString()));

    Compiler::ExprInfo info {
        m_left.Get(),
        m_right.Get()
    };

    if (bool(m_op->GetType() & (ARITHMETIC | BITWISE)) && !(m_op->GetType() & ASSIGNMENT))
    {
        uint8 opcode = 0;

        switch (m_op->GetOperatorType())
        {
        case Operators::OP_add:
            opcode = ADD;
            break;
        case Operators::OP_subtract:
            opcode = SUB;
            break;
        case Operators::OP_multiply:
            opcode = MUL;
            break;
        case Operators::OP_divide:
            opcode = DIV;
            break;
        case Operators::OP_modulus:
            opcode = MOD;
            break;
        case Operators::OP_bitwise_and:
            opcode = AND;
            break;
        case Operators::OP_bitwise_or:
            opcode = OR;
            break;
        case Operators::OP_bitwise_xor:
            opcode = XOR;
            break;
        case Operators::OP_bitshift_left:
            opcode = SHL;
            break;
        case Operators::OP_bitshift_right:
            opcode = SHR;
            break;
        }

        chunk->Append(Compiler::BuildBinOp(opcode, visitor, mod, info));

        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
    }
    else if (m_op->GetType() & LOGICAL)
    {
        RC<AstExpression> first = nullptr;
        RC<AstExpression> second = nullptr;

        AstBinaryExpression* leftAsBinop = dynamic_cast<AstBinaryExpression*>(m_left.Get());
        AstBinaryExpression* rightAsBinop = dynamic_cast<AstBinaryExpression*>(m_right.Get());

        if (leftAsBinop == nullptr && rightAsBinop != nullptr)
        {
            first = m_right;
            second = m_left;
        }
        else
        {
            first = m_left;
            second = m_right;
        }

        if (m_op->GetOperatorType() == Operators::OP_logical_and)
        {
            uint8 rp;

            LabelId falseLabel = contextGuard->NewLabel();
            chunk->TakeOwnershipOfLabel(falseLabel);

            LabelId trueLabel = contextGuard->NewLabel();
            chunk->TakeOwnershipOfLabel(trueLabel);

            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            int foldedValues[2] = { 0, 0 };

            // TODO: There is some duplicated code in here, needs to be cleaned up.

            { // do first part of expression
                bool folded = false;
                // attempt to constant fold the values
                RC<AstExpression> tmp(new AstFalse(SourceLocation::eof));

                if (RC<AstConstant> constantFolded = Optimizer::ConstantFold(first, tmp, Operators::OP_equals, visitor))
                {
                    foldedValues[0] = constantFolded->IsTrue();
                    folded = foldedValues[0] == 1 || foldedValues[1] == 0;

                    if (foldedValues[0] == 1)
                    {
                        // value is equal to 0, therefore it is false.
                        // load the label address from static memory into register 0
                        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, falseLabel));
                    }
                    else if (foldedValues[0] == 0)
                    {
                        // do not jump at all, only accept the code that it is true
                    }
                }

                if (!folded)
                {
                    // load left-hand side into register 0
                    chunk->Append(first->Build(visitor, mod));

                    // since this is an AND operation, jump as soon as the lhs is determined to be false
                    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

                    // compare lhs to 0 (false)
                    chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, rp));

                    // jump if they are equal: i.e the value is false
                    chunk->Append(BytecodeUtil::Make<Jump>(Jump::JE, falseLabel));
                }
            }

            // if we are at this point then lhs is true, so now test the rhs
            if (second != nullptr)
            {
                bool folded = false;
                // attempt to constant fold the values
                RC<AstExpression> tmp(new AstFalse(SourceLocation::eof));

                if (RC<AstConstant> constantFolded = Optimizer::ConstantFold(second, tmp, Operators::OP_equals, visitor))
                {
                    foldedValues[1] = constantFolded->IsTrue();
                    folded = foldedValues[1] == 1 || foldedValues[1] == 0;

                    if (foldedValues[1] == 1)
                    {
                        // value is equal to 0, therefore it is false.
                        // load the label address from static memory into register 0
                        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, falseLabel));
                    }
                }

                if (!folded)
                {
                    // load right-hand side into register 1
                    chunk->Append(second->Build(visitor, mod));

                    // get register position
                    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

                    // compare lhs to 0 (false)
                    chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, rp));

                    // jump if they are equal: i.e the value is false
                    chunk->Append(BytecodeUtil::Make<Jump>(Jump::JE, falseLabel));
                }
            }

            // both values were true at this point so load the value 'true'
            chunk->Append(BytecodeUtil::Make<ConstBool>(rp, true));

            if (foldedValues[0] != 1 || foldedValues[1] != 1)
            {
                // jump to the VERY end (so we don't load 'false' value)
                chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, trueLabel));

                chunk->Append(BytecodeUtil::Make<LabelMarker>(falseLabel));

                // here is where the value is false
                chunk->Append(BytecodeUtil::Make<ConstBool>(rp, false));

                chunk->Append(BytecodeUtil::Make<LabelMarker>(trueLabel));
            }
        }
        else if (m_op->GetOperatorType() == Operators::OP_logical_or)
        {
            uint8 rp;

            LabelId falseLabel = contextGuard->NewLabel();
            chunk->TakeOwnershipOfLabel(falseLabel);

            LabelId trueLabel = contextGuard->NewLabel();
            chunk->TakeOwnershipOfLabel(trueLabel);

            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            { // do first part of expression
                bool folded = false;
                // attempt to constant fold the values
                RC<AstExpression> tmp(new AstFalse(SourceLocation::eof));

                if (RC<AstConstant> constantFolded = Optimizer::ConstantFold(first, tmp, Operators::OP_equals, visitor))
                {
                    int foldedValue = constantFolded->IsTrue();
                    folded = foldedValue == 1 || foldedValue == 0;

                    if (foldedValue == 1)
                    {
                        // do not jump at all, we still have to test the second half of the expression
                    }
                    else if (foldedValue == 0)
                    {
                        // jump to end, the value is true and we don't have to check the second half
                        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, trueLabel));
                    }
                }

                if (!folded)
                {
                    // load left-hand side into register 0
                    chunk->Append(first->Build(visitor, mod));
                    // since this is an OR operation, jump as soon as the lhs is determined to be anything but 0
                    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

                    // compare lhs to 0 (false)
                    chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, rp));
                    chunk->Append(BytecodeUtil::Make<Jump>(Jump::JNE, trueLabel));
                }
            }

            // if we are at this point then lhs is true, so now test the rhs
            if (second != nullptr)
            {
                bool folded = false;
                { // attempt to constant fold the values
                    RC<AstExpression> tmp(new AstFalse(SourceLocation::eof));

                    if (RC<AstConstant> constantFolded = Optimizer::ConstantFold(second, tmp, Operators::OP_equals, visitor))
                    {
                        Tribool foldedValue = constantFolded->IsTrue();

                        if (foldedValue == Tribool::True())
                        {
                            // value is equal to 0
                            folded = true;
                        }
                        else if (foldedValue == Tribool::False())
                        {
                            folded = true;
                            // value is equal to 1 so jump to end
                            chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, trueLabel));
                        }
                    }
                }

                if (!folded)
                {
                    // load right-hand side into register 1
                    chunk->Append(second->Build(visitor, mod));
                    // get register position
                    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

                    // compare rhs to 0 (false)
                    chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, rp));

                    // jump if they are equal: i.e the value is true
                    chunk->Append(BytecodeUtil::Make<Jump>(Jump::JNE, trueLabel));
                }
            }

            // no values were true at this point so load the value 'false'
            chunk->Append(BytecodeUtil::Make<ConstBool>(rp, false));

            // jump to the VERY end (so we don't load 'true' value)
            chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, falseLabel));
            chunk->Append(BytecodeUtil::Make<LabelMarker>(trueLabel));

            // here is where the value is true
            chunk->Append(BytecodeUtil::Make<ConstBool>(rp, true));
            chunk->Append(BytecodeUtil::Make<LabelMarker>(falseLabel));
        }
    }
    else if (m_op->GetType() & COMPARISON)
    {
        uint8 rp;

        Jump::JumpClass jumpClass;

        bool swapped = false;

        switch (m_op->GetOperatorType())
        {
        case Operators::OP_equals:
            jumpClass = Jump::JNE;
            break;
        case Operators::OP_not_eql:
            jumpClass = Jump::JE;
            break;
        case Operators::OP_less:
            jumpClass = Jump::JGE;
            break;
        case Operators::OP_less_eql:
            jumpClass = Jump::JG;
            break;
        case Operators::OP_greater:
            jumpClass = Jump::JGE;
            swapped = true;
            break;
        case Operators::OP_greater_eql:
            jumpClass = Jump::JG;
            swapped = true;
            break;
        default:
            Assert(false, "Invalid operator for comparison");
        }

        AstBinaryExpression* leftAsBinop = dynamic_cast<AstBinaryExpression*>(m_left.Get());
        AstBinaryExpression* rightAsBinop = dynamic_cast<AstBinaryExpression*>(m_right.Get());

        if (m_right != nullptr)
        {
            uint8 r0, r1;

            LabelId trueLabel = contextGuard->NewLabel();
            chunk->TakeOwnershipOfLabel(trueLabel);

            LabelId falseLabel = contextGuard->NewLabel();
            chunk->TakeOwnershipOfLabel(falseLabel);

            if (leftAsBinop == nullptr && rightAsBinop != nullptr)
            {
                // if the right hand side is a binary operation,
                // we should build in the rhs first in order to
                // transverse the parse tree.
                chunk->Append(Compiler::LoadRightThenLeft(visitor, mod, info));
                rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

                r0 = rp;
                r1 = rp - 1;
            }
            else if (m_right != nullptr && m_right->MayHaveSideEffects())
            {
                // lhs must be temporary stored on the stack,
                // to avoid the rhs overwriting it.
                if (m_left->MayHaveSideEffects())
                {
                    chunk->Append(Compiler::LoadLeftAndStore(visitor, mod, info));
                    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

                    r0 = rp;
                    r1 = rp - 1;
                }
                else
                {
                    // left  doesn't have side effects,
                    // so just evaluate right without storing the lhs.
                    chunk->Append(Compiler::LoadRightThenLeft(visitor, mod, info));
                    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

                    r0 = rp;
                    r1 = rp - 1;
                }
            }
            else
            {
                // normal usage, load left into register 0,
                // then load right into register 1.
                // rinse and repeat.
                chunk->Append(Compiler::LoadLeftThenRight(visitor, mod, info));
                rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

                r0 = rp - 1;
                r1 = rp;
            }

            if (swapped)
            {
                std::swap(r0, r1);
            }

            // perform operation
            chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMP, r0, r1));

            visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
            rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

            // jump if they are equal
            chunk->Append(BytecodeUtil::Make<Jump>(jumpClass, trueLabel));

            // values are not equal at this point
            chunk->Append(BytecodeUtil::Make<ConstBool>(rp, true));

            // jump to the false label, the value is false at this point
            chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, falseLabel));

            chunk->Append(BytecodeUtil::Make<LabelMarker>(trueLabel));

            // values are equal
            chunk->Append(BytecodeUtil::Make<ConstBool>(rp, false));

            chunk->Append(BytecodeUtil::Make<LabelMarker>(falseLabel));
        }
        else
        {
            // load left-hand side into register
            // right-hand side has been optimized away
            chunk->Append(m_left->Build(visitor, mod));
        }
    }
    else if (m_op->GetType() & ASSIGNMENT)
    {
        uint8 rp;

        if (m_op->GetOperatorType() == Operators::OP_assign)
        {
            // load right-hand side into register 0
            chunk->Append(m_right->Build(visitor, mod));
            visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
        }
        else
        {
            // assignment/operation
            uint8 opcode = 0;

            switch (m_op->GetOperatorType())
            {
            case Operators::OP_add_assign:
                opcode = ADD;
                break;
            case Operators::OP_subtract_assign:
                opcode = SUB;
                break;
            case Operators::OP_multiply_assign:
                opcode = MUL;
                break;
            case Operators::OP_divide_assign:
                opcode = DIV;
                break;
            case Operators::OP_modulus_assign:
                opcode = MOD;
                break;
            default:
                Assert(false, "Invalid operator for assignment operation");
            }

            chunk->Append(Compiler::BuildBinOp(opcode, visitor, mod, info));
        }

        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        Assert(m_left->GetAccessOptions() & AccessMode::ACCESS_MODE_STORE);

        m_left->SetAccessMode(AccessMode::ACCESS_MODE_STORE);
        chunk->Append(m_left->Build(visitor, mod));

        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
    }

    return chunk;
}

void AstBinaryExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_overrideExpr != nullptr)
    {
        m_overrideExpr->Optimize(visitor, mod);

        return;
    }

#if HYP_SCRIPT_ENABLE_LAZY_DECLARATIONS
    if (m_variableDeclaration != nullptr)
    {
        m_variableDeclaration->Optimize(visitor, mod);
        return;
    }
#endif

    Assert(m_left != nullptr);

    m_left = Optimizer::OptimizeExpr(m_left, visitor, mod);

    if (!m_right)
    {
        return;
    }

    m_right = Optimizer::OptimizeExpr(m_right, visitor, mod);

    // check that we can further optimize the
    // binary expression by optimizing away the right
    // side, and combining the resulting value into
    // the left side of the operation.
    RC<AstConstant> constantValue = Optimizer::ConstantFold(
        m_left,
        m_right,
        m_op->GetOperatorType(),
        visitor);

    if (constantValue != nullptr)
    {
        // compile-time evaluation was successful
        m_left = std::move(constantValue);
        m_right.Reset();
    }
}

RC<AstStatement> AstBinaryExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstBinaryExpression::IsTrue() const
{
    if (m_right != nullptr)
    {
        // the right was not optimized away,
        // therefore we cannot determine whether or
        // not this expression would be true or false.
        return Tribool::Indeterminate();
    }

    return m_left->IsTrue();
}

bool AstBinaryExpression::MayHaveSideEffects() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->MayHaveSideEffects();
    }

    bool leftSideEffects = m_left->MayHaveSideEffects();
    bool rightSideEffects = false;

    if (m_right != nullptr)
    {
        rightSideEffects = m_right->MayHaveSideEffects();
    }

    if (m_op->ModifiesValue())
    {
        return true;
    }

    return leftSideEffects || rightSideEffects;
}

ConstantValue AstBinaryExpression::GetConstantValue() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->GetConstantValue();
    }

    if (m_right != nullptr)
    {
        return ConstantValue(INVALID_CONSTANT_NUMBER);
    }

    return m_left->GetConstantValue();
}

SymbolTypeRef AstBinaryExpression::GetExprType() const
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->GetExprType();
    }

    Assert(m_op != nullptr);

    if ((m_op->GetType() & LOGICAL) || (m_op->GetType() & COMPARISON))
    {
        return BuiltinTypes::s_boolType;
    }

    if (m_op->GetOperatorType() == OP_bitshift_right
        || m_op->GetOperatorType() == OP_bitshift_left)
    {
        // bitshift operators always return the LHS type
        Assert(m_left != nullptr);

        return m_left->GetExprType();
    }

    Assert(m_left != nullptr);

    SymbolTypeRef lTypePtr = m_left->GetExprType();
    Assert(lTypePtr != nullptr);

    if (m_right != nullptr)
    {
        // the right was not optimized away,
        // return type promotion
        SymbolTypeRef rTypePtr = m_right->GetExprType();

        Assert(rTypePtr != nullptr);

        return SymbolType::TypePromotion(lTypePtr, rTypePtr);
    }
    else
    {
        // right was optimized away, return only left type
        return lTypePtr;
    }
}

#if HYP_SCRIPT_ENABLE_LAZY_DECLARATIONS
RC<AstVariableDeclaration> AstBinaryExpression::CheckLazyDeclaration(AstVisitor* visitor, Module* mod)
{
    if (m_op->GetOperatorType() == Operators::OP_assign)
    {
        if (AstVariable* leftAsVar = dynamic_cast<AstVariable*>(m_left.Get()))
        {
            String varName = leftAsVar->GetName();
            // lookup variable name
            if (mod->LookUpIdentifier(varName, false))
            {
                return nullptr;
            }
            // not found as variable name
            // look up in the global module
            if (visitor->GetCompilationUnit()->GetGlobalModule()->LookUpIdentifier(varName, false))
            {
                return nullptr;
            }

            // check all modules for one with the same name
            if (visitor->GetCompilationUnit()->LookupModule(varName))
            {
                return nullptr;
            }

            return RC<AstVariableDeclaration>(new AstVariableDeclaration(
                varName,
                nullptr,
                m_right,
                false, // not const
                false, // not generic
                m_left->GetLocation()));
        }
    }

    return nullptr;
}

#endif

String AstBinaryExpression::ToString() const
{
    String leftStr = m_left ? m_left->ToString() : "<null>";
    String rightStr = m_right ? m_right->ToString() : "<null>";
    String opStr = m_op ? m_op->LookupStringValue() : "<null>";

    return "(" + leftStr + " " + opStr + " " + rightStr + ")";
}

} // namespace hyperion
