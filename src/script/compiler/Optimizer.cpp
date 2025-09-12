#include <script/compiler/Optimizer.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/ast/AstBinaryExpression.hpp>
#include <script/compiler/ast/AstUnaryExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstConstant.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstUnsignedInteger.hpp>
#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstFalse.hpp>

#include <core/debug/Debug.hpp>
#include <core/math/MathUtil.hpp>

#include <cmath>

namespace hyperion {

RC<AstConstant> Optimizer::ConstantFold(
    AstExpression* left,
    AstExpression* right,
    Operators opType,
    AstVisitor* visitor)
{
    Assert(left != nullptr);

    const bool isBinOp = Operator::FindBinaryOperator(opType) != nullptr;

    if (isBinOp)
    {
        Assert(right != nullptr);
    }

    ConstantValue leftValue = left->GetValueOf()->GetConstantValue();
    ConstantValue rightValue = right != nullptr
        ? right->GetValueOf()->GetConstantValue()
        : ConstantValue(INVALID_CONSTANT_NUMBER);

    RC<AstConstant> result;

    if (leftValue.IsValid())
    {
        if (isBinOp && !rightValue.IsValid())
        {
            return nullptr;
        }

        // Perform constant folding based on the operator type
        switch (opType)
        {
        case OP_add:
            if (leftValue.IsFloat() || rightValue.IsFloat())
            {
                const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize, CBS_32);
                result = RC<AstFloat>(new AstFloat(leftValue.AsFloat() + rightValue.AsFloat(), bitSize, left->GetLocation()));
            }
            else if (leftValue.IsUInt() || rightValue.IsUInt())
            {
                const ConstantBitSize bitSize = MathUtil::Min(leftValue.bitSize > rightValue.bitSize ? leftValue.bitSize : ConstantBitSize(rightValue.bitSize << 1), CBS_64);
                result = RC<AstUnsignedInteger>(new AstUnsignedInteger(leftValue.AsUInt() + rightValue.AsUInt(), bitSize, left->GetLocation()));
            }
            else if (leftValue.IsNumber() && rightValue.IsNumber())
            {
                const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize);
                result = RC<AstInteger>(new AstInteger(leftValue.AsInt() + rightValue.AsInt(), bitSize, left->GetLocation()));
            }
            break;

        case OP_subtract:
            if (leftValue.IsFloat() || rightValue.IsFloat())
            {
                const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize, CBS_32);
                result = RC<AstFloat>(new AstFloat(leftValue.AsFloat() - rightValue.AsFloat(), bitSize, left->GetLocation()));
            }
            else if (leftValue.IsUInt() || rightValue.IsUInt())
            {
                const ConstantBitSize bitSize = MathUtil::Min(leftValue.bitSize > rightValue.bitSize ? leftValue.bitSize : ConstantBitSize(rightValue.bitSize << 1), CBS_64);
                result = RC<AstUnsignedInteger>(new AstUnsignedInteger(leftValue.AsUInt() - rightValue.AsUInt(), bitSize, left->GetLocation()));
            }
            else if (leftValue.IsNumber() && rightValue.IsNumber())
            {
                const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize);
                result = RC<AstInteger>(new AstInteger(leftValue.AsInt() - rightValue.AsInt(), bitSize, left->GetLocation()));
            }

            break;

        case OP_multiply:
            if (leftValue.IsFloat() || rightValue.IsFloat())
            {
                const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize, CBS_32);
                result = RC<AstFloat>(new AstFloat(leftValue.AsFloat() * rightValue.AsFloat(), bitSize, left->GetLocation()));
            }
            else if (leftValue.IsUInt() || rightValue.IsUInt())
            {
                const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize);
                result = RC<AstUnsignedInteger>(new AstUnsignedInteger(leftValue.AsUInt() * rightValue.AsUInt(), bitSize, left->GetLocation()));
            }
            else if (leftValue.IsNumber() && rightValue.IsNumber())
            {
                const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize);
                result = RC<AstInteger>(new AstInteger(leftValue.AsInt() * rightValue.AsInt(), bitSize, left->GetLocation()));
            }
            break;

        case OP_divide:
            if (rightValue.AsFloat() == 0.0 || (rightValue.IsInt() && rightValue.AsInt() == 0) || (rightValue.IsUInt() && rightValue.AsUInt() == 0))
            {
                // Division by zero - don't fold
                break;
            }
            if (leftValue.IsFloat() || rightValue.IsFloat())
            {
                const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize, CBS_32);
                result = RC<AstFloat>(new AstFloat(leftValue.AsFloat() / rightValue.AsFloat(), bitSize, left->GetLocation()));
            }
            else if (leftValue.IsUInt() || rightValue.IsUInt())
            {
                const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize);
                result = RC<AstUnsignedInteger>(new AstUnsignedInteger(leftValue.AsUInt() / rightValue.AsUInt(), bitSize, left->GetLocation()));
            }
            else if (leftValue.IsNumber() && rightValue.IsNumber())
            {
                const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize);
                result = RC<AstInteger>(new AstInteger(leftValue.AsInt() / rightValue.AsInt(), bitSize, left->GetLocation()));
            }
            break;

        case OP_modulus:
            if (rightValue.AsFloat() == 0.0 || (rightValue.IsInt() && rightValue.AsInt() == 0) || (rightValue.IsUInt() && rightValue.AsUInt() == 0))
            {
                // Modulus by zero - don't fold
                break;
            }
            if (leftValue.IsFloat() || rightValue.IsFloat())
            {
                const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize, CBS_32);
                result = RC<AstFloat>(new AstFloat(std::fmod(leftValue.AsFloat(), rightValue.AsFloat()), bitSize, left->GetLocation()));
            }
            else if (leftValue.IsUInt() || rightValue.IsUInt())
            {
                const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize);
                result = RC<AstUnsignedInteger>(new AstUnsignedInteger(leftValue.AsUInt() % rightValue.AsUInt(), bitSize, left->GetLocation()));
            }
            else if (leftValue.IsNumber() && rightValue.IsNumber())
            {
                const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize);
                result = RC<AstInteger>(new AstInteger(leftValue.AsInt() % rightValue.AsInt(), bitSize, left->GetLocation()));
            }
            break;

        case OP_equals:
            if (leftValue.IsFloat() || rightValue.IsFloat())
            {
                if (leftValue.AsFloat() == rightValue.AsFloat())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsBool() && rightValue.IsBool())
            {
                if (leftValue.AsBool() == rightValue.AsBool())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsUInt() || rightValue.IsUInt())
            {
                if (leftValue.AsUInt() == rightValue.AsUInt())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsNumber() && rightValue.IsNumber())
            {
                if (leftValue.AsInt() == rightValue.AsInt())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            break;

        case OP_not_eql:
            if (leftValue.IsFloat() || rightValue.IsFloat())
            {
                if (leftValue.AsFloat() != rightValue.AsFloat())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsBool() && rightValue.IsBool())
            {
                if (leftValue.AsBool() != rightValue.AsBool())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsUInt() || rightValue.IsUInt())
            {
                if (leftValue.AsUInt() != rightValue.AsUInt())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsNumber() && rightValue.IsNumber())
            {
                if (leftValue.AsInt() != rightValue.AsInt())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            break;

        case OP_less:
            if (leftValue.IsFloat() || rightValue.IsFloat())
            {
                if (leftValue.AsFloat() < rightValue.AsFloat())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsUInt() || rightValue.IsUInt())
            {
                if (leftValue.AsUInt() < rightValue.AsUInt())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsNumber() && rightValue.IsNumber())
            {
                if (leftValue.AsInt() < rightValue.AsInt())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsBool() && rightValue.IsBool())
            {
                // false < true
                if (!leftValue.AsBool() && rightValue.AsBool())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            break;

        case OP_greater:
            if (leftValue.IsFloat() || rightValue.IsFloat())
            {
                if (leftValue.AsFloat() > rightValue.AsFloat())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsUInt() || rightValue.IsUInt())
            {
                if (leftValue.AsUInt() > rightValue.AsUInt())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsNumber() && rightValue.IsNumber())
            {
                if (leftValue.AsInt() > rightValue.AsInt())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsBool() && rightValue.IsBool())
            {
                // true > false
                if (leftValue.AsBool() && !rightValue.AsBool())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            break;

        case OP_less_eql:
            if (leftValue.IsFloat() || rightValue.IsFloat())
            {
                if (leftValue.AsFloat() <= rightValue.AsFloat())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsUInt() || rightValue.IsUInt())
            {
                if (leftValue.AsUInt() <= rightValue.AsUInt())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsNumber() && rightValue.IsNumber())
            {
                if (leftValue.AsInt() <= rightValue.AsInt())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsBool() && rightValue.IsBool())
            {
                // false <= true and false <= false and true <= true
                if ((!leftValue.AsBool() && rightValue.AsBool()) || (leftValue.AsBool() == rightValue.AsBool()))
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            break;

        case OP_greater_eql:
            if (leftValue.IsFloat() || rightValue.IsFloat())
            {
                if (leftValue.AsFloat() >= rightValue.AsFloat())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsUInt() || rightValue.IsUInt())
            {
                if (leftValue.AsUInt() >= rightValue.AsUInt())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsNumber() && rightValue.IsNumber())
            {
                if (leftValue.AsInt() >= rightValue.AsInt())
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            else if (leftValue.IsBool() && rightValue.IsBool())
            {
                // true >= false and false >= false and true >= true
                if ((leftValue.AsBool() && !rightValue.AsBool()) || (leftValue.AsBool() == rightValue.AsBool()))
                {
                    result = RC<AstTrue>(new AstTrue(left->GetLocation()));
                }
                else
                {
                    result = RC<AstFalse>(new AstFalse(left->GetLocation()));
                }
            }
            break;

        // Add bitwise operations for integer types only
        case OP_bitwise_and:
            if (!leftValue.IsFloat() && !rightValue.IsFloat())
            {
                if (leftValue.IsUInt() || rightValue.IsUInt())
                {
                    const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize);
                    result = RC<AstUnsignedInteger>(new AstUnsignedInteger(leftValue.AsUInt() & rightValue.AsUInt(), bitSize, left->GetLocation()));
                }
                else if (leftValue.IsInt() || rightValue.IsInt())
                {
                    const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize);
                    result = RC<AstInteger>(new AstInteger(leftValue.AsInt() & rightValue.AsInt(), bitSize, left->GetLocation()));
                }
            }
            break;

        case OP_bitwise_or:
            if (!leftValue.IsFloat() && !rightValue.IsFloat())
            {
                if (leftValue.IsUInt() || rightValue.IsUInt())
                {
                    const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize);
                    result = RC<AstUnsignedInteger>(new AstUnsignedInteger(leftValue.AsUInt() | rightValue.AsUInt(), bitSize, left->GetLocation()));
                }
                else if (leftValue.IsInt() || rightValue.IsInt())
                {
                    const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize);
                    result = RC<AstInteger>(new AstInteger(leftValue.AsInt() | rightValue.AsInt(), bitSize, left->GetLocation()));
                }
            }
            break;

        case OP_bitwise_xor:
            if (!leftValue.IsFloat() && !rightValue.IsFloat())
            {
                if (leftValue.IsUInt() || rightValue.IsUInt())
                {
                    const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize);
                    result = RC<AstUnsignedInteger>(new AstUnsignedInteger(leftValue.AsUInt() ^ rightValue.AsUInt(), bitSize, left->GetLocation()));
                }
                else if (leftValue.IsInt() || rightValue.IsInt())
                {
                    const ConstantBitSize bitSize = MathUtil::Max(leftValue.bitSize, rightValue.bitSize);
                    result = RC<AstInteger>(new AstInteger(leftValue.AsInt() ^ rightValue.AsInt(), bitSize, left->GetLocation()));
                }
            }
            break;

        case OP_bitshift_left:
            if (!leftValue.IsFloat() && !rightValue.IsFloat())
            {
                const ConstantBitSize bitSize = leftValue.bitSize;

                if (leftValue.IsUInt())
                {
                    result = RC<AstUnsignedInteger>(new AstUnsignedInteger(leftValue.AsUInt() << rightValue.AsUInt(), bitSize, left->GetLocation()));
                }
                else if (leftValue.IsInt())
                {
                    result = RC<AstInteger>(new AstInteger(leftValue.AsInt() << rightValue.AsInt(), bitSize, left->GetLocation()));
                }
            }
            break;

        case OP_bitshift_right:
            if (!leftValue.IsFloat() && !rightValue.IsFloat())
            {
                const ConstantBitSize bitSize = leftValue.bitSize;

                if (leftValue.IsUInt())
                {
                    result = RC<AstUnsignedInteger>(new AstUnsignedInteger(leftValue.AsUInt() >> rightValue.AsUInt(), bitSize, left->GetLocation()));
                }
                else if (leftValue.IsInt())
                {
                    result = RC<AstInteger>(new AstInteger(leftValue.AsInt() >> rightValue.AsInt(), bitSize, left->GetLocation()));
                }
            }
            break;

        // Add logical operations for boolean values
        case OP_logical_and:
        {
            const bool leftBool = leftValue.AsBool();
            const bool rightBool = rightValue.AsBool();
            if (leftBool && rightBool)
            {
                result = RC<AstTrue>(new AstTrue(left->GetLocation()));
            }
            else
            {
                result = RC<AstFalse>(new AstFalse(left->GetLocation()));
            }
        }
        break;

        case OP_logical_or:
        {
            const bool leftBool = leftValue.AsBool();
            const bool rightBool = rightValue.AsBool();
            if (leftBool || rightBool)
            {
                result = RC<AstTrue>(new AstTrue(left->GetLocation()));
            }
            else
            {
                result = RC<AstFalse>(new AstFalse(left->GetLocation()));
            }
        }
        break;
        case OP_logical_not:
        {
            const bool leftBool = leftValue.AsBool();
            if (!leftBool)
            {
                result = RC<AstTrue>(new AstTrue(left->GetLocation()));
            }
            else
            {
                result = RC<AstFalse>(new AstFalse(left->GetLocation()));
            }
        }
        case OP_negative:
            if (leftValue.IsFloat())
            {
                const ConstantBitSize bitSize = leftValue.bitSize;
                result = RC<AstFloat>(new AstFloat(-leftValue.AsFloat(), bitSize, left->GetLocation()));
            }
            else if (leftValue.IsUInt())
            {
                const ConstantBitSize bitSize = leftValue.bitSize;
                switch (bitSize)
                {
                case CBS_8:
                    result = RC<AstUnsignedInteger>(new AstUnsignedInteger(-uint8(leftValue.AsUInt()), bitSize, left->GetLocation()));
                    break;
                case CBS_16:
                    result = RC<AstUnsignedInteger>(new AstUnsignedInteger(-uint16(leftValue.AsUInt()), bitSize, left->GetLocation()));
                    break;
                case CBS_32:
                    result = RC<AstUnsignedInteger>(new AstUnsignedInteger(-uint32(leftValue.AsUInt()), bitSize, left->GetLocation()));
                    break;
                case CBS_64:
                    result = RC<AstUnsignedInteger>(new AstUnsignedInteger(-uint64(leftValue.AsUInt()), bitSize, left->GetLocation()));
                    break;
                default:
                    HYP_UNREACHABLE();
                    break;
                }
            }
            else if (leftValue.IsInt())
            {
                const ConstantBitSize bitSize = leftValue.bitSize;
                result = RC<AstInteger>(new AstInteger(-leftValue.AsInt(), bitSize, left->GetLocation()));
            }
            break;
        case OP_positive:
            if (leftValue.IsFloat())
            {
                const ConstantBitSize bitSize = leftValue.bitSize;
                result = RC<AstFloat>(new AstFloat(+leftValue.AsFloat(), bitSize, left->GetLocation()));
            }
            else if (leftValue.IsUInt())
            {
                const ConstantBitSize bitSize = leftValue.bitSize;
                switch (bitSize)
                {
                case CBS_8:
                    result = RC<AstUnsignedInteger>(new AstUnsignedInteger(+uint8(leftValue.AsUInt()), bitSize, left->GetLocation()));
                    break;
                case CBS_16:
                    result = RC<AstUnsignedInteger>(new AstUnsignedInteger(+uint16(leftValue.AsUInt()), bitSize, left->GetLocation()));
                    break;
                case CBS_32:
                    result = RC<AstUnsignedInteger>(new AstUnsignedInteger(+uint32(leftValue.AsUInt()), bitSize, left->GetLocation()));
                    break;
                case CBS_64:
                    result = RC<AstUnsignedInteger>(new AstUnsignedInteger(+uint64(leftValue.AsUInt()), bitSize, left->GetLocation()));
                    break;
                default:
                    HYP_UNREACHABLE();
                    break;
                }
            }
            else if (leftValue.IsInt())
            {
                const ConstantBitSize bitSize = leftValue.bitSize;
                result = RC<AstInteger>(new AstInteger(+leftValue.AsInt(), bitSize, left->GetLocation()));
            }
            break;
        case OP_bitwise_complement:
            if (leftValue.IsUInt())
            {
                const ConstantBitSize bitSize = leftValue.bitSize;
                result = RC<AstUnsignedInteger>(new AstUnsignedInteger(~leftValue.AsUInt(), bitSize, left->GetLocation()));
            }
            else if (leftValue.IsInt())
            {
                const ConstantBitSize bitSize = leftValue.bitSize;
                result = RC<AstInteger>(new AstInteger(~leftValue.AsInt(), bitSize, left->GetLocation()));
            }
            break;
        default:
            // Unsupported operator for constant folding
            break;
        }
    }

    return result;
}

RC<AstExpression> Optimizer::OptimizeExpr(
    const RC<AstExpression>& expr,
    AstVisitor* visitor,
    Module* mod)
{
    Assert(expr != nullptr);
    expr->Optimize(visitor, mod);

    if (const AstIdentifier* exprAsIdentifier = dynamic_cast<AstIdentifier*>(expr.Get()))
    {
        // the side is a variable, so we can further optimize by inlining,
        // only if it is literal.
        if (exprAsIdentifier->IsLiteral())
        {
            if (const RC<Identifier>& ident = exprAsIdentifier->GetProperties().GetIdentifier())
            {
                if (const RC<AstExpression>& currentValue = ident->GetCurrentValue())
                {
                    // decrement use count because it would have been incremented by Visit()
                    ident->DecUseCount();

                    return Optimizer::OptimizeExpr(currentValue, visitor, mod);
                }
            }
        }
    }
    else if (const AstBinaryExpression* exprAsBinop = dynamic_cast<AstBinaryExpression*>(expr.Get()))
    {
        if (!exprAsBinop->GetRight())
        {
            // right side has been optimized away
            return Optimizer::OptimizeExpr(exprAsBinop->GetLeft(), visitor, mod);
        }
    }

    return expr;
}

Optimizer::Optimizer(AstIterator* astIterator, CompilationUnit* compilationUnit)
    : AstVisitor(astIterator, compilationUnit)
{
}

Optimizer::Optimizer(const Optimizer& other)
    : AstVisitor(other.m_astIterator, other.m_compilationUnit)
{
}

void Optimizer::Optimize(bool expectModuleDecl)
{
    /*if (expectModuleDecl) {
        if (m_astIterator->HasNext()) {
            RC<AstStatement> firstStmt = m_astIterator->Next();

            if (AstModuleDeclaration *modDecl = dynamic_cast<AstModuleDeclaration*>(firstStmt.Get())) {
                // all files must begin with a module declaration
                modDecl->Optimize(this, nullptr);
                OptimizeInner();
            }
        }
    } else {
        OptimizeInner();
    }*/

    OptimizeInner();
}

void Optimizer::OptimizeInner()
{
    Module* mod = m_compilationUnit->GetCurrentModule();
    Assert(mod != nullptr);

    while (m_astIterator->HasNext())
    {
        m_astIterator->Next()->Optimize(this, mod);
    }
}

} // namespace hyperion
