#include <script/compiler/ast/AstUnsignedInteger.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstUndefined.hpp>
#include <script/compiler/AstVisitor.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeUtil.hpp>

#include <iostream>
#include <limits>
#include <cmath>

namespace hyperion {

AstUnsignedInteger::AstUnsignedInteger(hyperion::uint64 value, ConstantBitSize bitSize, const SourceLocation& location)
    : AstConstant(bitSize, location),
      m_value(value)
{
}

UniquePtr<Buildable> AstUnsignedInteger::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    switch (m_bitSize)
    {
    case CBS_8:
        return BytecodeUtil::Make<ConstU32>(rp, uint8(m_value)); // @TODO
    case CBS_16:
        return BytecodeUtil::Make<ConstU32>(rp, uint16(m_value)); // @TODO
    case CBS_32:
        return BytecodeUtil::Make<ConstU32>(rp, uint32(m_value));
    case CBS_64:
        return BytecodeUtil::Make<ConstU64>(rp, m_value);
    default:
        HYP_UNREACHABLE();
    }
}

RC<AstStatement> AstUnsignedInteger::Clone() const
{
    return CloneImpl();
}

Tribool AstUnsignedInteger::IsTrue() const
{
    // any non-zero value is considered true
    return Tribool(m_value != 0);
}

bool AstUnsignedInteger::IsNumber() const
{
    return true;
}

ConstantValue AstUnsignedInteger::GetConstantValue() const
{
    return ConstantValue(m_value, m_bitSize);
}

SymbolTypeRef AstUnsignedInteger::GetExprType() const
{
    switch (m_bitSize)
    {
    case CBS_8:
        return BuiltinTypes::s_uint8Type;
    case CBS_16:
        return BuiltinTypes::s_uint16Type;
    case CBS_32:
        return BuiltinTypes::s_uint32Type;
    case CBS_64:
        return BuiltinTypes::s_uint64Type;
    default:
        HYP_UNREACHABLE();
    }
}

RC<AstConstant> AstUnsignedInteger::HandleOperator(Operators opType, const AstConstant* right) const
{
    switch (opType)
    {
    case OP_add:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        {
            const ConstantValue leftValue = GetConstantValue();
            const ConstantValue rightValue = right->GetConstantValue();

            if (rightValue.IsFloat())
            {
                return RC<AstFloat>(new AstFloat(
                    leftValue.AsFloat() + rightValue.AsFloat(),
                    MathUtil::Max(m_bitSize, rightValue.bitSize, CBS_32),
                    m_location));
            }
            else if (rightValue.IsInt())
            {
                return RC<AstInteger>(new AstInteger(
                    int64(leftValue.AsUInt()) + rightValue.AsInt(),
                    MathUtil::Min(m_bitSize > rightValue.bitSize ? m_bitSize : ConstantBitSize(m_bitSize << 1), CBS_64),
                    m_location));
            }
            else
            {
                return RC<AstUnsignedInteger>(new AstUnsignedInteger(
                    leftValue.AsUInt() + rightValue.AsUInt(),
                    MathUtil::Max(m_bitSize, rightValue.bitSize),
                    m_location));
            }
        }

    case OP_subtract:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        {
            const ConstantValue leftValue = GetConstantValue();
            const ConstantValue rightValue = right->GetConstantValue();

            if (rightValue.IsFloat())
            {
                return RC<AstFloat>(new AstFloat(
                    leftValue.AsFloat() - rightValue.AsFloat(),
                    MathUtil::Max(m_bitSize, rightValue.bitSize, CBS_32),
                    m_location));
            }
            else if (rightValue.IsInt())
            {
                return RC<AstInteger>(new AstInteger(
                    int64(leftValue.AsUInt()) - rightValue.AsInt(),
                    MathUtil::Min(m_bitSize > rightValue.bitSize ? m_bitSize : ConstantBitSize(m_bitSize << 1), CBS_64),
                    m_location));
            }
            else
            {
                return RC<AstUnsignedInteger>(new AstUnsignedInteger(
                    leftValue.AsUInt() - rightValue.AsUInt(),
                    MathUtil::Max(m_bitSize, rightValue.bitSize),
                    m_location));
            }
        }

    case OP_multiply:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        {
            const ConstantValue leftValue = GetConstantValue();
            const ConstantValue rightValue = right->GetConstantValue();

            if (rightValue.IsFloat())
            {
                return RC<AstFloat>(new AstFloat(
                    leftValue.AsFloat() * rightValue.AsFloat(),
                    MathUtil::Max(m_bitSize, rightValue.bitSize, CBS_32),
                    m_location));
            }
            else if (rightValue.IsInt())
            {
                return RC<AstInteger>(new AstInteger(
                    int64(leftValue.AsUInt()) * rightValue.AsInt(),
                    MathUtil::Min(m_bitSize > rightValue.bitSize ? m_bitSize : ConstantBitSize(m_bitSize << 1), CBS_64),
                    m_location));
            }
            else
            {
                return RC<AstUnsignedInteger>(new AstUnsignedInteger(
                    leftValue.AsUInt() * rightValue.AsUInt(),
                    MathUtil::Max(m_bitSize, rightValue.bitSize),
                    m_location));
            }
        }

    case OP_divide:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        {
            const ConstantValue leftValue = GetConstantValue();
            const ConstantValue rightValue = right->GetConstantValue();

            if (rightValue.IsFloat())
            {
                double result;
                double rightFloat = rightValue.AsFloat();
                if (rightFloat == 0.0)
                {
                    result = NAN;
                }
                else
                {
                    result = leftValue.AsFloat() / rightFloat;
                }

                return RC<AstFloat>(new AstFloat(
                    result,
                    MathUtil::Max(m_bitSize, rightValue.bitSize, CBS_32),
                    m_location));
            }
            else if (rightValue.IsInt())
            {
                int64 rightInt = rightValue.AsInt();
                if (rightInt == 0)
                {
                    return RC<AstUndefined>(new AstUndefined(m_location));
                }
                else
                {
                    return RC<AstInteger>(new AstInteger(
                        int64(leftValue.AsUInt()) / rightInt,
                        MathUtil::Min(m_bitSize > rightValue.bitSize ? m_bitSize : ConstantBitSize(m_bitSize << 1), CBS_64),
                        m_location));
                }
            }
            else
            {
                uint64 rightUInt = rightValue.AsUInt();
                if (rightUInt == 0)
                {
                    return RC<AstUndefined>(new AstUndefined(m_location));
                }
                else
                {
                    return RC<AstUnsignedInteger>(new AstUnsignedInteger(
                        leftValue.AsUInt() / rightUInt,
                        MathUtil::Max(m_bitSize, rightValue.bitSize),
                        m_location));
                }
            }
        }

    case OP_modulus:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        {
            const ConstantValue leftValue = GetConstantValue();
            const ConstantValue rightValue = right->GetConstantValue();

            if (rightValue.IsFloat())
            {
                return RC<AstFloat>(new AstFloat(
                    std::fmod(leftValue.AsFloat(), rightValue.AsFloat()),
                    MathUtil::Max(m_bitSize, rightValue.bitSize, CBS_32),
                    m_location));
            }
            else if (rightValue.IsInt())
            {
                return RC<AstInteger>(new AstInteger(
                    int64(leftValue.AsUInt()) % rightValue.AsInt(),
                    MathUtil::Min(m_bitSize > rightValue.bitSize ? m_bitSize : ConstantBitSize(m_bitSize << 1), CBS_64),
                    m_location));
            }
            else
            {
                return RC<AstUnsignedInteger>(new AstUnsignedInteger(
                    leftValue.AsUInt() % rightValue.AsUInt(),
                    MathUtil::Max(m_bitSize, rightValue.bitSize),
                    m_location));
            }
        }

    case OP_bitwise_xor:
        if (!right->GetExprType()->IsIntegral())
        {
            return nullptr;
        }

        {
            const ConstantValue leftValue = GetConstantValue();
            const ConstantValue rightValue = right->GetConstantValue();

            if (rightValue.IsInt())
            {
                return RC<AstInteger>(new AstInteger(
                    int64(leftValue.AsUInt()) ^ rightValue.AsInt(),
                    MathUtil::Min(m_bitSize > rightValue.bitSize ? m_bitSize : ConstantBitSize(m_bitSize << 1), CBS_64),
                    m_location));
            }
            else
            {
                return RC<AstUnsignedInteger>(new AstUnsignedInteger(
                    leftValue.AsUInt() ^ rightValue.AsUInt(),
                    MathUtil::Max(m_bitSize, rightValue.bitSize),
                    m_location));
            }
        }

    case OP_bitwise_and:
        if (!right->GetExprType()->IsIntegral())
        {
            return nullptr;
        }

        {
            const ConstantValue leftValue = GetConstantValue();
            const ConstantValue rightValue = right->GetConstantValue();

            if (rightValue.IsInt())
            {
                return RC<AstInteger>(new AstInteger(
                    int64(leftValue.AsUInt()) & rightValue.AsInt(),
                    MathUtil::Min(m_bitSize > rightValue.bitSize ? m_bitSize : ConstantBitSize(m_bitSize << 1), CBS_64),
                    m_location));
            }
            else
            {
                return RC<AstUnsignedInteger>(new AstUnsignedInteger(
                    leftValue.AsUInt() & rightValue.AsUInt(),
                    MathUtil::Max(m_bitSize, rightValue.bitSize),
                    m_location));
            }
        }

    case OP_bitwise_or:
        if (!right->GetExprType()->IsIntegral())
        {
            return nullptr;
        }

        {
            const ConstantValue leftValue = GetConstantValue();
            const ConstantValue rightValue = right->GetConstantValue();

            if (rightValue.IsInt())
            {
                return RC<AstInteger>(new AstInteger(
                    int64(leftValue.AsUInt()) | rightValue.AsInt(),
                    MathUtil::Min(m_bitSize > rightValue.bitSize ? m_bitSize : ConstantBitSize(m_bitSize << 1), CBS_64),
                    m_location));
            }
            else
            {
                return RC<AstUnsignedInteger>(new AstUnsignedInteger(
                    leftValue.AsUInt() | rightValue.AsUInt(),
                    MathUtil::Max(m_bitSize, rightValue.bitSize),
                    m_location));
            }
        }

    case OP_bitshift_left:
        if (!right->GetExprType()->IsIntegral())
        {
            return nullptr;
        }

        {
            const ConstantValue leftValue = GetConstantValue();
            const ConstantValue rightValue = right->GetConstantValue();

            return RC<AstUnsignedInteger>(new AstUnsignedInteger(
                leftValue.AsUInt() << rightValue.AsUInt(),
                m_bitSize,
                m_location));
        }

    case OP_bitshift_right:
        if (!right->GetExprType()->IsIntegral())
        {
            return nullptr;
        }

        {
            const ConstantValue leftValue = GetConstantValue();
            const ConstantValue rightValue = right->GetConstantValue();

            return RC<AstUnsignedInteger>(new AstUnsignedInteger(
                leftValue.AsUInt() >> rightValue.AsUInt(),
                m_bitSize,
                m_location));
        }

    case OP_logical_and:
    {
        int thisTrue = IsTrue();
        int rightTrue = right->IsTrue();

        if (!right->IsNumber())
        {
            // this operator is valid to compare against null
            if (dynamic_cast<const AstNil*>(right))
            {
                // rhs is null, return false
                return RC<AstFalse>(new AstFalse(m_location));
            }

            return nullptr;
        }

        if (thisTrue == 1 && rightTrue == 1)
        {
            return RC<AstTrue>(new AstTrue(m_location));
        }
        else if (thisTrue == 0 && rightTrue == 0)
        {
            return RC<AstFalse>(new AstFalse(m_location));
        }
        else
        {
            // indeterminate
            return nullptr;
        }
    }

    case OP_logical_or:
    {
        int thisTrue = IsTrue();
        int rightTrue = right->IsTrue();

        if (!right->IsNumber())
        {
            // this operator is valid to compare against null
            if (dynamic_cast<const AstNil*>(right))
            {
                if (thisTrue == 1)
                {
                    return RC<AstTrue>(new AstTrue(m_location));
                }
                else if (thisTrue == 0)
                {
                    return RC<AstFalse>(new AstFalse(m_location));
                }
            }

            return nullptr;
        }

        if (thisTrue == 1 || rightTrue == 1)
        {
            return RC<AstTrue>(new AstTrue(m_location));
        }
        else if (thisTrue == 0 || rightTrue == 0)
        {
            return RC<AstFalse>(new AstFalse(m_location));
        }
        else
        {
            // indeterminate
            return nullptr;
        }
    }

    case OP_less:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        {
            const ConstantValue leftValue = GetConstantValue();
            const ConstantValue rightValue = right->GetConstantValue();

            if (leftValue.AsUInt() < rightValue.AsUInt())
            {
                return RC<AstTrue>(new AstTrue(m_location));
            }
            else
            {
                return RC<AstFalse>(new AstFalse(m_location));
            }
        }

    case OP_greater:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        {
            const ConstantValue leftValue = GetConstantValue();
            const ConstantValue rightValue = right->GetConstantValue();

            if (leftValue.AsUInt() > rightValue.AsUInt())
            {
                return RC<AstTrue>(new AstTrue(m_location));
            }
            else
            {
                return RC<AstFalse>(new AstFalse(m_location));
            }
        }

    case OP_less_eql:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        {
            const ConstantValue leftValue = GetConstantValue();
            const ConstantValue rightValue = right->GetConstantValue();

            if (leftValue.AsUInt() <= rightValue.AsUInt())
            {
                return RC<AstTrue>(new AstTrue(m_location));
            }
            else
            {
                return RC<AstFalse>(new AstFalse(m_location));
            }
        }

    case OP_greater_eql:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        {
            const ConstantValue leftValue = GetConstantValue();
            const ConstantValue rightValue = right->GetConstantValue();

            if (leftValue.AsUInt() >= rightValue.AsUInt())
            {
                return RC<AstTrue>(new AstTrue(m_location));
            }
            else
            {
                return RC<AstFalse>(new AstFalse(m_location));
            }
        }

    case OP_equals:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        {
            const ConstantValue leftValue = GetConstantValue();
            const ConstantValue rightValue = right->GetConstantValue();

            if (leftValue.AsUInt() == rightValue.AsUInt())
            {
                return RC<AstTrue>(new AstTrue(m_location));
            }
            else
            {
                return RC<AstFalse>(new AstFalse(m_location));
            }
        }

    case OP_negative:
    {
        const ConstantValue leftValue = GetConstantValue();
        return RC<AstUnsignedInteger>(new AstUnsignedInteger(-leftValue.AsUInt(), m_bitSize, m_location));
    }

    case OP_bitwise_complement:
    {
        const ConstantValue leftValue = GetConstantValue();
        return RC<AstUnsignedInteger>(new AstUnsignedInteger(~leftValue.AsUInt(), m_bitSize, m_location));
    }

    case OP_logical_not:
    {
        const ConstantValue leftValue = GetConstantValue();
        if (leftValue.AsUInt() == 0)
        {
            return RC<AstTrue>(new AstTrue(m_location));
        }
        else
        {
            return RC<AstFalse>(new AstFalse(m_location));
        }
    }

    default:
        return nullptr;
    }
}

} // namespace hyperion
