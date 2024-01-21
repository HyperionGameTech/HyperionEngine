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

namespace hyperion::compiler {

AstInteger::AstInteger(hyperion::Int32 value, const SourceLocation &location)
    : AstConstant(location),
      m_value(value)
{
}

std::unique_ptr<Buildable> AstInteger::Build(AstVisitor *visitor, Module *mod)
{
    // get active register
    const UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
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

hyperion::Int32 AstInteger::IntValue() const
{
    return m_value;
}

hyperion::UInt32 AstInteger::UnsignedValue() const
{
    return (hyperion::UInt32)m_value;
}

hyperion::Float32 AstInteger::FloatValue() const
{
    return (hyperion::Float32)m_value;
}

SymbolTypePtr_t AstInteger::GetExprType() const
{
    return BuiltinTypes::INT;
}

RC<AstConstant> AstInteger::HandleOperator(Operators op_type, const AstConstant *right) const
{
    switch (op_type) {
    case OP_add:
        if (!right->IsNumber()) {
            return nullptr;
        }

        // we have to determine weather or not to promote this to a float
        if (dynamic_cast<const AstFloat *>(right)) {
            return RC<AstFloat>(
                new AstFloat(FloatValue() + right->FloatValue(), m_location));
        } else if (dynamic_cast<const AstUnsignedInteger *>(right)) {
            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() + right->UnsignedValue(), m_location));
        } else {
            return RC<AstInteger>(
                new AstInteger(IntValue() + right->IntValue(), m_location));
        }

    case OP_subtract:
        if (!right->IsNumber()) {
            return nullptr;
        }

        // we have to determine weather or not to promote this to a float
        if (dynamic_cast<const AstFloat*>(right)) {
            return RC<AstFloat>(
                new AstFloat(FloatValue() - right->FloatValue(), m_location));
        } else if (dynamic_cast<const AstUnsignedInteger *>(right)) {
            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() - right->UnsignedValue(), m_location));
        } else {
            return RC<AstInteger>(
                new AstInteger(IntValue() - right->IntValue(), m_location));
        }

    case OP_multiply:
        if (!right->IsNumber()) {
            return nullptr;
        }

        // we have to determine weather or not to promote this to a float
        if (dynamic_cast<const AstFloat*>(right)) {
            return RC<AstFloat>(
                new AstFloat(FloatValue() * right->FloatValue(), m_location));
        } else if (dynamic_cast<const AstUnsignedInteger *>(right)) {
            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() * right->UnsignedValue(), m_location));
        } else {
            return RC<AstInteger>(
                new AstInteger(IntValue() * right->IntValue(), m_location));
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
        } else if (dynamic_cast<const AstUnsignedInteger *>(right)) {
            auto right_uint = right->UnsignedValue();
            if (right_uint == 0) {
                // division by zero, return Undefined
                return nullptr;
            } else {
                return RC<AstUnsignedInteger>(
                    new AstUnsignedInteger(UnsignedValue() / right_uint, m_location));
            }
        } else {
            auto right_int = right->IntValue();
            if (right_int == 0) {
                // division by zero, return Undefined
                return nullptr;
            } else {
                return RC<AstInteger>(
                    new AstInteger(IntValue() / right_int, m_location));
            }
        }

    case OP_modulus:
        if (!right->IsNumber()) {
            return nullptr;
        }

        if (dynamic_cast<const AstFloat*>(right)) {
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
        } else if (dynamic_cast<const AstUnsignedInteger *>(right)) {
            auto right_uint = right->UnsignedValue();
            if (right_uint == 0) {
                // division by zero, return Undefined
                return nullptr;
            } else {
                return RC<AstUnsignedInteger>(
                    new AstUnsignedInteger(UnsignedValue() % right_uint, m_location));
            }
        } else {
            auto right_int = right->IntValue();
            if (right_int == 0) {
                // division by zero, return Undefined
                return nullptr;
            } else {
                return RC<AstInteger>(
                    new AstInteger(IntValue() % right_int, m_location));
            }
        }

    case OP_bitwise_xor:
        // right must be integer
        if (!right->IsNumber()
            || (right->GetExprType() != BuiltinTypes::INT
                && right->GetExprType() != BuiltinTypes::UNSIGNED_INT)) {
            return nullptr;
        }

        if (dynamic_cast<const AstUnsignedInteger *>(right)) {
            auto right_uint = right->UnsignedValue();

            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() ^ right_uint, m_location));
        }

        return RC<AstInteger>(
            new AstInteger(IntValue() ^ right->IntValue(), m_location));

    case OP_bitwise_and:
        // right must be integer
        if (!right->IsNumber()
            || (right->GetExprType() != BuiltinTypes::INT
                && right->GetExprType() != BuiltinTypes::UNSIGNED_INT)) {
            return nullptr;
        }

        if (dynamic_cast<const AstUnsignedInteger *>(right)) {
            auto right_uint = right->UnsignedValue();

            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() & right_uint, m_location));
        }

        return RC<AstInteger>(
            new AstInteger(IntValue() & right->IntValue(), m_location));

    case OP_bitwise_or:
        if (!right->IsNumber()
            || (right->GetExprType() != BuiltinTypes::INT
                && right->GetExprType() != BuiltinTypes::UNSIGNED_INT)) {
            return nullptr;
        }

        if (dynamic_cast<const AstUnsignedInteger *>(right)) {
            auto right_uint = right->UnsignedValue();

            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() | right_uint, m_location));
        }

        return RC<AstInteger>(
            new AstInteger(IntValue() | right->IntValue(), m_location));

    case OP_bitshift_left:
        // right must be integer
        if (!right->IsNumber()
            || (right->GetExprType() != BuiltinTypes::INT
                && right->GetExprType() != BuiltinTypes::UNSIGNED_INT)) {
            return nullptr;
        }

        if (dynamic_cast<const AstUnsignedInteger *>(right)) {
            auto right_uint = right->UnsignedValue();

            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() << right_uint, m_location));
        }

        return RC<AstInteger>(
            new AstInteger(IntValue() << right->IntValue(), m_location));

    case OP_bitshift_right:
        // right must be integer
        if (!right->IsNumber()
            || (right->GetExprType() != BuiltinTypes::INT
                && right->GetExprType() != BuiltinTypes::UNSIGNED_INT)) {
            return nullptr;
        }

        if (dynamic_cast<const AstUnsignedInteger *>(right)) {
            auto right_uint = right->UnsignedValue();

            return RC<AstUnsignedInteger>(
                new AstUnsignedInteger(UnsignedValue() >> right_uint, m_location));
        }

        return RC<AstInteger>(
            new AstInteger(IntValue() >> right->IntValue(), m_location));

    case OP_logical_and: {
        int this_true = IsTrue();
        int right_true = right->IsTrue();

        if (!right->IsNumber()) {
            // this operator is valid to compare against null
            if (dynamic_cast<const AstNil *>(right)) {
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

        if (IntValue() < right->IntValue()) {
            return RC<AstTrue>(new AstTrue(m_location));
        } else {
            return RC<AstFalse>(new AstFalse(m_location));
        }

    case OP_greater:
        if (!right->IsNumber()) {
            return nullptr;
        }

        if (IntValue() > right->IntValue()) {
            return RC<AstTrue>(new AstTrue(m_location));
        } else {
            return RC<AstFalse>(new AstFalse(m_location));
        }

    case OP_less_eql:
        if (!right->IsNumber()) {
            return nullptr;
        }

        if (IntValue() <= right->IntValue()) {
            return RC<AstTrue>(new AstTrue(m_location));
        } else {
            return RC<AstFalse>(new AstFalse(m_location));
        }

    case OP_greater_eql:
        if (!right->IsNumber()) {
            return nullptr;
        }

        if (IntValue() >= right->IntValue()) {
            return RC<AstTrue>(new AstTrue(m_location));
        } else {
            return RC<AstFalse>(new AstFalse(m_location));
        }

    case OP_equals:
        if (!right->IsNumber()) {
            return nullptr;
        }

        if (IntValue() == right->IntValue()) {
            return RC<AstTrue>(new AstTrue(m_location));
        } else {
            return RC<AstFalse>(new AstFalse(m_location));
        }

    case OP_negative:
        return RC<AstInteger>(new AstInteger(-IntValue(), m_location));

    case OP_bitwise_complement:
        return RC<AstInteger>(new AstInteger(~IntValue(), m_location));

    case OP_logical_not:
        if (IntValue() == 0) {
            return RC<AstTrue>(new AstTrue(m_location));
        } else {
            return RC<AstFalse>(new AstFalse(m_location));
        }

    default:
        return nullptr;
    }
}

} // namespace hyperion::compiler
