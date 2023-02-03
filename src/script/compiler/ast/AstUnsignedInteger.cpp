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

namespace hyperion::compiler {

AstUnsignedInteger::AstUnsignedInteger(hyperion::UInt32 value, const SourceLocation &location)
    : AstConstant(location),
      m_value(value)
{
}

std::unique_ptr<Buildable> AstUnsignedInteger::Build(AstVisitor *visitor, Module *mod)
{
    // get active register
    UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
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

hyperion::Int32 AstUnsignedInteger::IntValue() const
{
    return (hyperion::Int32)m_value;
}

hyperion::UInt32 AstUnsignedInteger::UnsignedValue() const
{
    return m_value;
}

hyperion::Float32 AstUnsignedInteger::FloatValue() const
{
    return (hyperion::Float32)m_value;
}

SymbolTypePtr_t AstUnsignedInteger::GetExprType() const
{
    return BuiltinTypes::UNSIGNED_INT;
}

RC<AstConstant> AstUnsignedInteger::HandleOperator(Operators op_type, const AstConstant *right) const
{
    switch (op_type) {
        case OP_add:
            if (!right->IsNumber()) {
                return nullptr;
            }

            // we have to determine weather or not to promote this to a float
            if (dynamic_cast<const AstFloat*>(right)) {
                return RC<AstFloat>(
                    new AstFloat(FloatValue() + right->FloatValue(), m_location));
            } else {
                return RC<AstUnsignedInteger>(
                    new AstUnsignedInteger(UnsignedValue() + right->UnsignedValue(), m_location));
            }

        case OP_subtract:
            if (!right->IsNumber()) {
                return nullptr;
            }

            // we have to determine weather or not to promote this to a float
            if (dynamic_cast<const AstFloat*>(right)) {
                return RC<AstFloat>(
                    new AstFloat(FloatValue() - right->FloatValue(), m_location));
            } else {
                return RC<AstUnsignedInteger>(
                    new AstUnsignedInteger(UnsignedValue() - right->UnsignedValue(), m_location));
            }

        case OP_multiply:
            if (!right->IsNumber()) {
                return nullptr;
            }

            // we have to determine weather or not to promote this to a float
            if (dynamic_cast<const AstFloat*>(right)) {
                return RC<AstFloat>(
                    new AstFloat(FloatValue() * right->FloatValue(), m_location));
            } else {
                return RC<AstUnsignedInteger>(
                    new AstUnsignedInteger(UnsignedValue() * right->UnsignedValue(), m_location));
            }

        case OP_divide:
            if (!right->IsNumber()) {
                return nullptr;
            }

            // we have to determine weather or not to promote this to a float
            if (dynamic_cast<const AstFloat*>(right)) {
                hyperion::Float32 result;
                auto right_float = right->FloatValue();
                if (right_float == 0.0) {
                    // division by zero, return Undefined
                    return nullptr;
                } else {
                    result = FloatValue() / right_float;
                }
                return RC<AstFloat>(
                    new AstFloat(result, m_location));
            } else {
                auto right_int = right->UnsignedValue();
                if (right_int == 0) {
                    // division by zero, return Undefined
                    return nullptr;
                } else {
                    return RC<AstUnsignedInteger>(
                        new AstUnsignedInteger(UnsignedValue() / right_int, m_location));
                }
            }

        case OP_modulus:
            if (!right->IsNumber()) {
                return nullptr;
            }

            // we have to determine weather or not to promote this to a float
            if (dynamic_cast<const AstFloat *>(right)) {
                hyperion::Float32 result;
                auto right_float = right->FloatValue();
                if (right_float == 0.0) {
                    // division by zero, return Undefined
                    return nullptr;
                } else {
                    result = std::fmod(FloatValue(), right_float);
                }
                return RC<AstFloat>(
                    new AstFloat(result, m_location));
            } else {
                auto right_int = right->UnsignedValue();
                if (right_int == 0) {
                    // division by zero, return Undefined
                    return nullptr;
                } else {
                    return RC<AstUnsignedInteger>(
                        new AstUnsignedInteger(UnsignedValue() % right_int, m_location));
                }
            }

        case OP_bitwise_xor:
            if (!right->IsNumber()
                || (right->GetExprType() != BuiltinTypes::INT
                    && right->GetExprType() != BuiltinTypes::UNSIGNED_INT)) {
                return nullptr;
            }

            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() ^ right->UnsignedValue(), m_location));

        case OP_bitwise_and:
            if (!right->IsNumber()
                || (right->GetExprType() != BuiltinTypes::INT
                    && right->GetExprType() != BuiltinTypes::UNSIGNED_INT)) {
                return nullptr;
            }

            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() & right->UnsignedValue(), m_location));

        case OP_bitwise_or:
            if (!right->IsNumber()
                || (right->GetExprType() != BuiltinTypes::INT
                    && right->GetExprType() != BuiltinTypes::UNSIGNED_INT)) {
                return nullptr;
            }

            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() | right->UnsignedValue(), m_location));

        case OP_bitshift_left:
            if (!right->IsNumber()
                || (right->GetExprType() != BuiltinTypes::INT
                    && right->GetExprType() != BuiltinTypes::UNSIGNED_INT)) {
                return nullptr;
            }

            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() << right->UnsignedValue(), m_location));

        case OP_bitshift_right:
            if (!right->IsNumber()
                || (right->GetExprType() != BuiltinTypes::INT
                    && right->GetExprType() != BuiltinTypes::UNSIGNED_INT)) {
                return nullptr;
            }

            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() >> right->UnsignedValue(), m_location));

        case OP_logical_and: {
            int this_true = IsTrue();
            int right_true = right->IsTrue();

            if (!right->IsNumber()) {
                // this operator is valid to compare against null
                if (dynamic_cast<const AstNil*>(right)) {
                    // rhs is null, return false
                    return RC<AstFalse>(
                        new AstFalse(m_location));
                }
                return nullptr;
            }

            if (this_true == 1 && right_true == 1) {
                return RC<AstTrue>(new AstTrue(m_location));
            } else if (this_true == 0 && right_true == 0) {
                return RC<AstFalse>(new AstFalse(m_location));
            } else {
                // indeterminate
                return nullptr;
            }
        }

        case OP_logical_or: {
            int this_true = IsTrue();
            int right_true = right->IsTrue();

            if (!right->IsNumber()) {
                // this operator is valid to compare against null
                if (dynamic_cast<const AstNil*>(right)) {
                    if (this_true == 1) {
                        return RC<AstTrue>(new AstTrue(m_location));
                    } else if (this_true == 0) {
                        return RC<AstFalse>(new AstFalse(m_location));
                    }
                }
                return nullptr;
            }

            if (this_true == 1 || right_true == 1) {
                return RC<AstTrue>(new AstTrue(m_location));
            } else if (this_true == 0 || right_true == 0) {
                return RC<AstFalse>(new AstFalse(m_location));
            } else {
                // indeterminate
                return nullptr;
            }
        }

        case OP_less:
            if (!right->IsNumber()) {
                return nullptr;
            }

            if (UnsignedValue() < right->UnsignedValue()) {
                return RC<AstTrue>(new AstTrue(m_location));
            } else {
                return RC<AstFalse>(new AstFalse(m_location));
            }

        case OP_greater:
            if (!right->IsNumber()) {
                return nullptr;
            }

            if (UnsignedValue() > right->UnsignedValue()) {
                return RC<AstTrue>(new AstTrue(m_location));
            } else {
                return RC<AstFalse>(new AstFalse(m_location));
            }

        case OP_less_eql:
            if (!right->IsNumber()) {
                return nullptr;
            }

            if (UnsignedValue() <= right->UnsignedValue()) {
                return RC<AstTrue>(new AstTrue(m_location));
            } else {
                return RC<AstFalse>(new AstFalse(m_location));
            }

        case OP_greater_eql:
            if (!right->IsNumber()) {
                return nullptr;
            }

            if (UnsignedValue() >= right->UnsignedValue()) {
                return RC<AstTrue>(new AstTrue(m_location));
            } else {
                return RC<AstFalse>(new AstFalse(m_location));
            }

        case OP_equals:
            if (!right->IsNumber()) {
                return nullptr;
            }

            if (UnsignedValue() == right->UnsignedValue()) {
                return RC<AstTrue>(new AstTrue(m_location));
            } else {
                return RC<AstFalse>(new AstFalse(m_location));
            }

        case OP_negative:
            return RC<AstUnsignedInteger>(new AstUnsignedInteger(-UnsignedValue(), m_location));

        case OP_bitwise_complement:
            return RC<AstUnsignedInteger>(new AstUnsignedInteger(~UnsignedValue(), m_location));

        case OP_logical_not:
            if (UnsignedValue() == 0) {
                return RC<AstTrue>(new AstTrue(m_location));
            } else {
                return RC<AstFalse>(new AstFalse(m_location));
            }

        default:
            return nullptr;
    }
}

} // namespace hyperion::compiler
