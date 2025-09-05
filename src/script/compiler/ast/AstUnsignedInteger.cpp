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

AstUnsignedInteger::AstUnsignedInteger(hyperion::uint32 value, const SourceLocation& location)
    : AstConstant(location),
      m_value(value)
{
}

UniquePtr<Buildable> AstUnsignedInteger::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    return BytecodeUtil::Make<ConstU32>(rp, m_value);
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

hyperion::int32 AstUnsignedInteger::IntValue() const
{
    return (hyperion::int32)m_value;
}

hyperion::uint32 AstUnsignedInteger::UnsignedValue() const
{
    return m_value;
}

float AstUnsignedInteger::FloatValue() const
{
    return (float)m_value;
}

SymbolTypeRef AstUnsignedInteger::GetExprType() const
{
    return BuiltinTypes::UNSIGNED_INT;
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

        // we have to determine weather or not to promote this to a float
        if (dynamic_cast<const AstFloat*>(right))
        {
            return RC<AstFloat>(
                new AstFloat(FloatValue() + right->FloatValue(), m_location));
        }
        else
        {
            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() + right->UnsignedValue(), m_location));
        }

    case OP_subtract:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        // we have to determine weather or not to promote this to a float
        if (dynamic_cast<const AstFloat*>(right))
        {
            return RC<AstFloat>(
                new AstFloat(FloatValue() - right->FloatValue(), m_location));
        }
        else
        {
            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() - right->UnsignedValue(), m_location));
        }

    case OP_multiply:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        // we have to determine weather or not to promote this to a float
        if (dynamic_cast<const AstFloat*>(right))
        {
            return RC<AstFloat>(
                new AstFloat(FloatValue() * right->FloatValue(), m_location));
        }
        else
        {
            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() * right->UnsignedValue(), m_location));
        }

    case OP_divide:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        // we have to determine weather or not to promote this to a float
        if (dynamic_cast<const AstFloat*>(right))
        {
            float result;
            auto rightFloat = right->FloatValue();
            if (rightFloat == 0.0)
            {
                // division by zero, return Undefined
                return nullptr;
            }
            else
            {
                result = FloatValue() / rightFloat;
            }
            return RC<AstFloat>(
                new AstFloat(result, m_location));
        }
        else
        {
            auto rightInt = right->UnsignedValue();
            if (rightInt == 0)
            {
                // division by zero, return Undefined
                return nullptr;
            }
            else
            {
                return RC<AstUnsignedInteger>(
                    new AstUnsignedInteger(UnsignedValue() / rightInt, m_location));
            }
        }

    case OP_modulus:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        // we have to determine weather or not to promote this to a float
        if (dynamic_cast<const AstFloat*>(right))
        {
            float result;
            auto rightFloat = right->FloatValue();
            if (rightFloat == 0.0)
            {
                // division by zero, return Undefined
                return nullptr;
            }
            else
            {
                result = std::fmod(FloatValue(), rightFloat);
            }
            return RC<AstFloat>(
                new AstFloat(result, m_location));
        }
        else
        {
            auto rightInt = right->UnsignedValue();
            if (rightInt == 0)
            {
                // division by zero, return Undefined
                return nullptr;
            }
            else
            {
                return RC<AstUnsignedInteger>(
                    new AstUnsignedInteger(UnsignedValue() % rightInt, m_location));
            }
        }

    case OP_bitwise_xor:
        if (!right->IsNumber()
            || (right->GetExprType() != BuiltinTypes::INT
                && right->GetExprType() != BuiltinTypes::UNSIGNED_INT))
        {
            return nullptr;
        }

        return RC<AstUnsignedInteger>(
            new AstUnsignedInteger(UnsignedValue() ^ right->UnsignedValue(), m_location));

    case OP_bitwise_and:
        if (!right->IsNumber()
            || (right->GetExprType() != BuiltinTypes::INT
                && right->GetExprType() != BuiltinTypes::UNSIGNED_INT))
        {
            return nullptr;
        }

        return RC<AstUnsignedInteger>(
            new AstUnsignedInteger(UnsignedValue() & right->UnsignedValue(), m_location));

    case OP_bitwise_or:
        if (!right->IsNumber()
            || (right->GetExprType() != BuiltinTypes::INT
                && right->GetExprType() != BuiltinTypes::UNSIGNED_INT))
        {
            return nullptr;
        }

        return RC<AstUnsignedInteger>(
            new AstUnsignedInteger(UnsignedValue() | right->UnsignedValue(), m_location));

    case OP_bitshift_left:
        if (!right->IsNumber()
            || (right->GetExprType() != BuiltinTypes::INT
                && right->GetExprType() != BuiltinTypes::UNSIGNED_INT))
        {
            return nullptr;
        }

        return RC<AstUnsignedInteger>(
            new AstUnsignedInteger(UnsignedValue() << right->UnsignedValue(), m_location));

    case OP_bitshift_right:
        if (!right->IsNumber()
            || (right->GetExprType() != BuiltinTypes::INT
                && right->GetExprType() != BuiltinTypes::UNSIGNED_INT))
        {
            return nullptr;
        }

        return RC<AstUnsignedInteger>(
            new AstUnsignedInteger(UnsignedValue() >> right->UnsignedValue(), m_location));

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
                return RC<AstFalse>(
                    new AstFalse(m_location));
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

        if (UnsignedValue() < right->UnsignedValue())
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

        if (UnsignedValue() > right->UnsignedValue())
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

        if (UnsignedValue() <= right->UnsignedValue())
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

        if (UnsignedValue() >= right->UnsignedValue())
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

        if (UnsignedValue() == right->UnsignedValue())
        {
            return RC<AstTrue>(new AstTrue(m_location));
        }
        else
        {
            return RC<AstFalse>(new AstFalse(m_location));
        }

    case OP_negative:
        return RC<AstUnsignedInteger>(new AstUnsignedInteger(-UnsignedValue(), m_location));

    case OP_bitwise_complement:
        return RC<AstUnsignedInteger>(new AstUnsignedInteger(~UnsignedValue(), m_location));

    case OP_logical_not:
        if (UnsignedValue() == 0)
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
