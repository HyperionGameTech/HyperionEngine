#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/AstVisitor.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeUtil.hpp>

#include <core/utilities/Format.hpp>

#include <limits>
#include <cmath>

namespace hyperion {

AstFloat::AstFloat(double value, ConstantBitSize bitSize, const SourceLocation& location)
    : AstConstant(bitSize, location),
      m_value(value)
{
}

UniquePtr<Buildable> AstFloat::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    const uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    switch (m_bitSize)
    {
    case CBS_32:
        return BytecodeUtil::Make<ConstF32>(rp, float32(m_value));
    case CBS_64:
        return BytecodeUtil::Make<ConstF64>(rp, m_value);
    default:
        HYP_UNREACHABLE();
    }
}

RC<AstStatement> AstFloat::Clone() const
{
    return CloneImpl();
}

Tribool AstFloat::IsTrue() const
{
    // any non-zero value is considered true
    return Tribool(m_value != 0.0f);
}

bool AstFloat::IsNumber() const
{
    return true;
}

hyperion::int64 AstFloat::IntValue() const
{
    return (hyperion::int64)m_value;
}

hyperion::uint64 AstFloat::UnsignedValue() const
{
    return (hyperion::uint64)m_value;
}

double AstFloat::FloatValue() const
{
    return m_value;
}

SymbolTypeRef AstFloat::GetExprType() const
{
    switch (m_bitSize)
    {
    case CBS_32:
        return BuiltinTypes::s_floatType;
    case CBS_64:
        return BuiltinTypes::s_doubleType;
    default:
        HYP_UNREACHABLE();
    }
}

RC<AstConstant> AstFloat::HandleOperator(Operators opType, const AstConstant* right) const
{
    switch (opType)
    {
    case OP_add:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        return RC<AstFloat>(new AstFloat(
            FloatValue() + right->FloatValue(),
            MathUtil::Max(m_bitSize, right->GetBitSize(), CBS_32),
            m_location));

    case OP_subtract:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        return RC<AstFloat>(new AstFloat(
            FloatValue() - right->FloatValue(),
            MathUtil::Max(m_bitSize, right->GetBitSize(), CBS_32),
            m_location));

    case OP_multiply:
        if (!right->IsNumber())
        {
            return nullptr;
        }

        return RC<AstFloat>(new AstFloat(
            FloatValue() * right->FloatValue(),
            MathUtil::Max(m_bitSize, right->GetBitSize(), CBS_32),
            m_location));

    case OP_divide:
    {
        if (!right->IsNumber())
        {
            return nullptr;
        }

        auto rightFloat = right->FloatValue();
        if (rightFloat == 0.0)
        {
            // division by zero
            return nullptr;
        }

        return RC<AstFloat>(new AstFloat(
            FloatValue() / rightFloat,
            MathUtil::Max(m_bitSize, right->GetBitSize(), CBS_32),
            m_location));
    }

    case OP_modulus:
    {
        if (!right->IsNumber())
        {
            return nullptr;
        }

        auto rightFloat = right->FloatValue();
        if (rightFloat == 0.0)
        {
            // division by zero
            return nullptr;
        }

        return RC<AstFloat>(new AstFloat(
            std::fmod(FloatValue(), rightFloat),
            MathUtil::Max(m_bitSize, right->GetBitSize(), CBS_32),
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
        if (FloatValue() < right->FloatValue())
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
        if (FloatValue() > right->FloatValue())
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
        if (FloatValue() <= right->FloatValue())
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
        if (FloatValue() >= right->FloatValue())
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
        if (FloatValue() == right->FloatValue())
        {
            return RC<AstTrue>(new AstTrue(m_location));
        }
        else
        {
            return RC<AstFalse>(new AstFalse(m_location));
        }

    case OP_negative:
        return RC<AstFloat>(new AstFloat(-FloatValue(), m_bitSize, m_location));

    case OP_logical_not:
        if (FloatValue() == 0.0)
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

String AstFloat::ToString() const
{
    return HYP_FORMAT("{}", m_value);
}

} // namespace hyperion
