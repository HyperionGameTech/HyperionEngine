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

AstInteger::AstInteger(hyperion::int32 value, const SourceLocation& location)
    : AstConstant(location),
      m_value(value)
{
}

UniquePtr<Buildable> AstInteger::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    const uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    return BytecodeUtil::Make<ConstI32>(rp, m_value);
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

hyperion::int32 AstInteger::IntValue() const
{
    return m_value;
}

hyperion::uint32 AstInteger::UnsignedValue() const
{
    return (hyperion::uint32)m_value;
}

float AstInteger::FloatValue() const
{
    return (float)m_value;
}

SymbolTypeRef AstInteger::GetExprType() const
{
    return BuiltinTypes::g_intType;
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
            return RC<AstFloat>(
                new AstFloat(FloatValue() + right->FloatValue(), m_location));
        }
        else if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() + right->UnsignedValue(), m_location));
        }
        else
        {
            return RC<AstInteger>(
                new AstInteger(IntValue() + right->IntValue(), m_location));
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
        else if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() - right->UnsignedValue(), m_location));
        }
        else
        {
            return RC<AstInteger>(
                new AstInteger(IntValue() - right->IntValue(), m_location));
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
        else if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() * right->UnsignedValue(), m_location));
        }
        else
        {
            return RC<AstInteger>(
                new AstInteger(IntValue() * right->IntValue(), m_location));
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
        else if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            auto rightUint = right->UnsignedValue();
            if (rightUint == 0)
            {
                // division by zero, return Undefined
                return nullptr;
            }
            else
            {
                return RC<AstUnsignedInteger>(
                    new AstUnsignedInteger(UnsignedValue() / rightUint, m_location));
            }
        }
        else
        {
            auto rightInt = right->IntValue();
            if (rightInt == 0)
            {
                // division by zero, return Undefined
                return nullptr;
            }
            else
            {
                return RC<AstInteger>(
                    new AstInteger(IntValue() / rightInt, m_location));
            }
        }

    case OP_modulus:
        if (!right->IsNumber())
        {
            return nullptr;
        }

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
        else if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            auto rightUint = right->UnsignedValue();
            if (rightUint == 0)
            {
                // division by zero, return Undefined
                return nullptr;
            }
            else
            {
                return RC<AstUnsignedInteger>(
                    new AstUnsignedInteger(UnsignedValue() % rightUint, m_location));
            }
        }
        else
        {
            auto rightInt = right->IntValue();
            if (rightInt == 0)
            {
                // division by zero, return Undefined
                return nullptr;
            }
            else
            {
                return RC<AstInteger>(
                    new AstInteger(IntValue() % rightInt, m_location));
            }
        }

    case OP_bitwise_xor:
        // right must be integer
        if (!right->IsNumber()
            || (right->GetExprType() != BuiltinTypes::g_intType
                && right->GetExprType() != BuiltinTypes::g_unsignedIntType))
        {
            return nullptr;
        }

        if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            auto rightUint = right->UnsignedValue();

            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() ^ rightUint, m_location));
        }

        return RC<AstInteger>(
            new AstInteger(IntValue() ^ right->IntValue(), m_location));

    case OP_bitwise_and:
        // right must be integer
        if (!right->IsNumber()
            || (right->GetExprType() != BuiltinTypes::g_intType
                && right->GetExprType() != BuiltinTypes::g_unsignedIntType))
        {
            return nullptr;
        }

        if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            auto rightUint = right->UnsignedValue();

            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() & rightUint, m_location));
        }

        return RC<AstInteger>(
            new AstInteger(IntValue() & right->IntValue(), m_location));

    case OP_bitwise_or:
        if (!right->IsNumber()
            || (right->GetExprType() != BuiltinTypes::g_intType
                && right->GetExprType() != BuiltinTypes::g_unsignedIntType))
        {
            return nullptr;
        }

        if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            auto rightUint = right->UnsignedValue();

            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() | rightUint, m_location));
        }

        return RC<AstInteger>(
            new AstInteger(IntValue() | right->IntValue(), m_location));

    case OP_bitshift_left:
        // right must be integer
        if (!right->IsNumber()
            || (right->GetExprType() != BuiltinTypes::g_intType
                && right->GetExprType() != BuiltinTypes::g_unsignedIntType))
        {
            return nullptr;
        }

        if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            auto rightUint = right->UnsignedValue();

            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() << rightUint, m_location));
        }

        return RC<AstInteger>(
            new AstInteger(IntValue() << right->IntValue(), m_location));

    case OP_bitshift_right:
        // right must be integer
        if (!right->IsNumber()
            || (right->GetExprType() != BuiltinTypes::g_intType
                && right->GetExprType() != BuiltinTypes::g_unsignedIntType))
        {
            return nullptr;
        }

        if (dynamic_cast<const AstUnsignedInteger*>(right))
        {
            auto rightUint = right->UnsignedValue();

            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() >> rightUint, m_location));
        }

        return RC<AstInteger>(
            new AstInteger(IntValue() >> right->IntValue(), m_location));

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
        return RC<AstInteger>(new AstInteger(-IntValue(), m_location));

    case OP_bitwise_complement:
        return RC<AstInteger>(new AstInteger(~IntValue(), m_location));

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
