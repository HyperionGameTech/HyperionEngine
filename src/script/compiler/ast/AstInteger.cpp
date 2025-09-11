#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstUnsignedInteger.hpp>
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

AstInteger::AstInteger(hyperion::int64 value, ConstantBitSize bitSize, const SourceLocation& location)
    : AstConstant(bitSize, location),
      m_value(value)
{
}

UniquePtr<Buildable> AstInteger::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    const uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    switch (m_bitSize)
    {
    case CBS_8:
        return BytecodeUtil::Make<ConstI32>(rp, int8(m_value)); // @TODO
    case CBS_16:
        return BytecodeUtil::Make<ConstI32>(rp, int16(m_value)); // @TODO
    case CBS_32:
        return BytecodeUtil::Make<ConstI32>(rp, int32(m_value));
    case CBS_64:
        return BytecodeUtil::Make<ConstI64>(rp, m_value);
    default:
        HYP_UNREACHABLE();
    }
}

RC<AstStatement> AstInteger::Clone() const
{
    return CloneImpl();
}

Tribool AstInteger::IsTrue() const
{
    // any non-zero value is considered true
    return Tribool(m_value != 0);
}

bool AstInteger::IsNumber() const
{
    return true;
}

hyperion::int64 AstInteger::IntValue() const
{
    return m_value;
}

hyperion::uint64 AstInteger::UnsignedValue() const
{
    return (hyperion::uint64)m_value;
}

double AstInteger::FloatValue() const
{
    return (double)m_value;
}

SymbolTypeRef AstInteger::GetExprType() const
{
    switch (m_bitSize)
    {
    case CBS_8:
        return BuiltinTypes::s_int8Type;
    case CBS_16:
        return BuiltinTypes::s_int16Type;
    case CBS_32:
        return BuiltinTypes::s_int32Type;
    case CBS_64:
        return BuiltinTypes::s_int64Type;
    default:
        HYP_UNREACHABLE();
    }
}

RC<AstConstant> AstInteger::HandleOperator(Operators opType, const AstConstant* right) const
{
    switch (opType)
    {
    case OP_add:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        // we have to determine weather or not to promote this to a float
        if (dynamic_cast<const AstFloat*>(right))
        {
            return RC<AstFloat>(new AstFloat(
                FloatValue() + right->FloatValue(),
                MathUtil::Max(m_bitSize, right->GetBitSize(), CBS_32),
                m_location));
        }
        else if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() + int64(right->UnsignedValue()),
                MathUtil::Min(m_bitSize > right->GetBitSize() ? m_bitSize : ConstantBitSize(right->GetBitSize() << 1), CBS_64),
                m_location));
        }
        else
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() + right->IntValue(),
                MathUtil::Max(m_bitSize, right->GetBitSize()),
                m_location));
        }

    case OP_subtract:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        // we have to determine weather or not to promote this to a float
        if (dynamic_cast<const AstFloat*>(right))
        {
            return RC<AstFloat>(new AstFloat(
                FloatValue() - right->FloatValue(),
                MathUtil::Max(m_bitSize, right->GetBitSize(), CBS_32),
                m_location));
        }
        else if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() - int64(right->UnsignedValue()),
                MathUtil::Min(m_bitSize > right->GetBitSize() ? m_bitSize : ConstantBitSize(right->GetBitSize() << 1), CBS_64),
                m_location));
        }
        else
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() - right->IntValue(),
                MathUtil::Max(m_bitSize, right->GetBitSize()),
                m_location));
        }

    case OP_multiply:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        // we have to determine weather or not to promote this to a float
        if (dynamic_cast<const AstFloat*>(right))
        {
            return RC<AstFloat>(new AstFloat(
                FloatValue() * right->FloatValue(),
                MathUtil::Max(m_bitSize, right->GetBitSize(), CBS_32),
                m_location));
        }
        else if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() * int64(right->UnsignedValue()),
                MathUtil::Min(m_bitSize > right->GetBitSize() ? m_bitSize : ConstantBitSize(right->GetBitSize() << 1), CBS_64),
                m_location));
        }
        else
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() * right->IntValue(),
                MathUtil::Max(m_bitSize, right->GetBitSize()),
                m_location));
        }

    case OP_divide:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        // we have to determine weather or not to promote this to a float
        if (dynamic_cast<const AstFloat*>(right))
        {
            double result;
            auto rightFloat = right->FloatValue();
            if (rightFloat == 0.0)
            {
                result = NAN;
            }
            else
            {
                result = FloatValue() / rightFloat;
            }

            return RC<AstFloat>(new AstFloat(
                result,
                MathUtil::Max(m_bitSize, right->GetBitSize(), CBS_32),
                m_location));
        }
        else if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            auto rightInt = right->UnsignedValue();
            if (rightInt == 0)
            {
                return RC<AstUndefined>(new AstUndefined(m_location));
            }
            else
            {
                return RC<AstInteger>(new AstInteger(
                    IntValue() / int64(rightInt),
                    MathUtil::Min(m_bitSize > right->GetBitSize() ? m_bitSize : ConstantBitSize(right->GetBitSize() << 1), CBS_64),
                    m_location));
            }
        }
        else
        {
            auto rightInt = right->IntValue();
            if (rightInt == 0)
            {
                return RC<AstUndefined>(new AstUndefined(m_location));
            }
            else
            {
                return RC<AstInteger>(new AstInteger(
                    IntValue() / rightInt,
                    MathUtil::Max(m_bitSize, right->GetBitSize()),
                    m_location));
            }
        }

    case OP_modulus:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        if (dynamic_cast<const AstFloat*>(right))
        {
            return RC<AstFloat>(new AstFloat(
                std::fmod(FloatValue(), right->FloatValue()),
                MathUtil::Max(m_bitSize, right->GetBitSize(), CBS_32),
                m_location));
        }
        else if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() % int64(right->UnsignedValue()),
                MathUtil::Min(m_bitSize > right->GetBitSize() ? m_bitSize : ConstantBitSize(right->GetBitSize() << 1), CBS_64),
                m_location));
        }
        else
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() % right->IntValue(),
                MathUtil::Max(m_bitSize, right->GetBitSize()),
                m_location));
        }

    case OP_bitwise_xor:
        if (!right->GetExprType()->IsIntegral())
        {
            return nullptr;
        }

        if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() ^ int64(right->UnsignedValue()),
                MathUtil::Min(m_bitSize > right->GetBitSize() ? m_bitSize : ConstantBitSize(right->GetBitSize() << 1), CBS_64),
                m_location));
        }
        else
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() ^ right->IntValue(),
                MathUtil::Max(m_bitSize, right->GetBitSize()),
                m_location));
        }

    case OP_bitwise_and:
        if (!right->GetExprType()->IsIntegral())
        {
            return nullptr;
        }

        if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() & int64(right->UnsignedValue()),
                MathUtil::Min(m_bitSize > right->GetBitSize() ? m_bitSize : ConstantBitSize(right->GetBitSize() << 1), CBS_64),
                m_location));
        }
        else
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() & right->IntValue(),
                MathUtil::Max(m_bitSize, right->GetBitSize()),
                m_location));
        }

    case OP_bitwise_or:
        if (!right->GetExprType()->IsIntegral())
        {
            return nullptr;
        }

        if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() | int64(right->UnsignedValue()),
                MathUtil::Min(m_bitSize > right->GetBitSize() ? m_bitSize : ConstantBitSize(right->GetBitSize() << 1), CBS_64),
                m_location));
        }
        else
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() | right->IntValue(),
                MathUtil::Max(m_bitSize, right->GetBitSize()),
                m_location));
        }

    case OP_bitshift_left:
        if (!right->GetExprType()->IsIntegral())
        {
            return nullptr;
        }

        if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() << int64(right->UnsignedValue()),
                MathUtil::Min(m_bitSize > right->GetBitSize() ? m_bitSize : ConstantBitSize(right->GetBitSize() << 1), CBS_64),
                m_location));
        }
        else
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() << right->IntValue(),
                MathUtil::Max(m_bitSize, right->GetBitSize()),
                m_location));
        }

    case OP_bitshift_right:
        if (!right->GetExprType()->IsIntegral())
        {
            return nullptr;
        }

        if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() >> int64(right->UnsignedValue()),
                MathUtil::Min(m_bitSize > right->GetBitSize() ? m_bitSize : ConstantBitSize(right->GetBitSize() << 1), CBS_64),
                m_location));
        }
        else
        {
            return RC<AstInteger>(new AstInteger(
                IntValue() >> right->IntValue(),
                MathUtil::Max(m_bitSize, right->GetBitSize()),
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

        if (IntValue() < right->IntValue())
        {
            return RC<AstTrue>(new AstTrue(m_location));
        }
        else
        {
            return RC<AstFalse>(new AstFalse(m_location));
        }

    case OP_greater:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        if (IntValue() > right->IntValue())
        {
            return RC<AstTrue>(new AstTrue(m_location));
        }
        else
        {
            return RC<AstFalse>(new AstFalse(m_location));
        }

    case OP_less_eql:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        if (IntValue() <= right->IntValue())
        {
            return RC<AstTrue>(new AstTrue(m_location));
        }
        else
        {
            return RC<AstFalse>(new AstFalse(m_location));
        }

    case OP_greater_eql:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        if (IntValue() >= right->IntValue())
        {
            return RC<AstTrue>(new AstTrue(m_location));
        }
        else
        {
            return RC<AstFalse>(new AstFalse(m_location));
        }

    case OP_equals:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        if (IntValue() == right->IntValue())
        {
            return RC<AstTrue>(new AstTrue(m_location));
        }
        else
        {
            return RC<AstFalse>(new AstFalse(m_location));
        }

    case OP_negative:
        return RC<AstInteger>(new AstInteger(-IntValue(), m_bitSize, m_location));

    case OP_bitwise_complement:
        return RC<AstInteger>(new AstInteger(~IntValue(), m_bitSize, m_location));

    case OP_logical_not:
        if (IntValue() == 0)
        {
            return RC<AstTrue>(new AstTrue(m_location));
        }
        else
        {
            return RC<AstFalse>(new AstFalse(m_location));
        }

    default:
        return nullptr;
    }
}

} // namespace hyperion
