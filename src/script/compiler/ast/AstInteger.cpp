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
namespace compiler {

AstInteger::AstInteger(hyperion::aint32 value, const SourceLocation &location)
    : AstConstant(location),
      m_value(value)
{
}

std::unique_ptr<Buildable> AstInteger::Build(AstVisitor *visitor, Module *mod)
{
    // get active register
    uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    return BytecodeUtil::Make<ConstI32>(rp, m_value);
}

Pointer<AstStatement> AstInteger::Clone() const
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

hyperion::aint32 AstInteger::IntValue() const
{
    return m_value;
}

hyperion::afloat32 AstInteger::FloatValue() const
{
    return (hyperion::afloat32)m_value;
}

SymbolTypePtr_t AstInteger::GetSymbolType() const
{
    return BuiltinTypes::INT;
}

std::shared_ptr<AstConstant> AstInteger::HandleOperator(Operators op_type, AstConstant *right) const
{
    switch (op_type) {
        case OP_add:
            if (!right->IsNumber()) {
                return nullptr;
            }

            // we have to determine weather or not to promote this to a float
            if (dynamic_cast<const AstFloat*>(right)) {
                return std::shared_ptr<AstFloat>(
                    new AstFloat(FloatValue() + right->FloatValue(), m_location));
            } else {
                return std::shared_ptr<AstInteger>(
                    new AstInteger(IntValue() + right->IntValue(), m_location));
            }

        case OP_subtract:
            if (!right->IsNumber()) {
                return nullptr;
            }

            // we have to determine weather or not to promote this to a float
            if (dynamic_cast<const AstFloat*>(right)) {
                return std::shared_ptr<AstFloat>(
                    new AstFloat(FloatValue() - right->FloatValue(), m_location));
            } else {
                return std::shared_ptr<AstInteger>(
                    new AstInteger(IntValue() - right->IntValue(), m_location));
            }

        case OP_multiply:
            if (!right->IsNumber()) {
                return nullptr;
            }

            // we have to determine weather or not to promote this to a float
            if (dynamic_cast<const AstFloat*>(right)) {
                return std::shared_ptr<AstFloat>(
                    new AstFloat(FloatValue() * right->FloatValue(), m_location));
            } else {
                return std::shared_ptr<AstInteger>(
                    new AstInteger(IntValue() * right->IntValue(), m_location));
            }

        case OP_divide:
            if (!right->IsNumber()) {
                return nullptr;
            }

            // we have to determine weather or not to promote this to a float
            if (dynamic_cast<const AstFloat*>(right)) {
                hyperion::afloat32 result;
                auto right_float = right->FloatValue();
                if (right_float == 0.0) {
                    // division by zero, return Undefined
                    return nullptr;
                } else {
                    result = FloatValue() / right_float;
                }
                return std::shared_ptr<AstFloat>(
                    new AstFloat(result, m_location));
            } else {
                auto right_int = right->IntValue();
                if (right_int == 0) {
                    // division by zero, return Undefined
                    return nullptr;
                } else {
                    return std::shared_ptr<AstInteger>(
                        new AstInteger(IntValue() / right_int, m_location));
                }
            }

        case OP_modulus:
            if (!right->IsNumber()) {
                return nullptr;
            }

            // we have to determine weather or not to promote this to a float
            if (dynamic_cast<const AstFloat*>(right)) {
                hyperion::afloat32 result;
                auto right_float = right->FloatValue();
                if (right_float == 0.0) {
                    // division by zero, return Undefined
                    return nullptr;
                } else {
                    result = std::fmod(FloatValue(), right_float);
                }
                return std::shared_ptr<AstFloat>(
                    new AstFloat(result, m_location));
            } else {
                auto right_int = right->IntValue();
                if (right_int == 0) {
                    // division by zero, return Undefined
                    return nullptr;
                } else {
                    return std::shared_ptr<AstInteger>(
                        new AstInteger(IntValue() % right_int, m_location));
                }
            }

        case OP_bitwise_xor:
            // right must be integer
            if (!right->IsNumber() || right->GetSymbolType() != BuiltinTypes::INT) {
                return nullptr;
            }

            return std::shared_ptr<AstInteger>(
                new AstInteger(IntValue() ^ right->IntValue(), m_location));

        case OP_bitwise_and:
            // right must be integer
            if (!right->IsNumber() || right->GetSymbolType() != BuiltinTypes::INT) {
                return nullptr;
            }

            return std::shared_ptr<AstInteger>(
                new AstInteger(IntValue() & right->IntValue(), m_location));

        case OP_bitwise_or:
            // right must be integer
            if (!right->IsNumber() || right->GetSymbolType() != BuiltinTypes::INT) {
                return nullptr;
            }

            return std::shared_ptr<AstInteger>(
                new AstInteger(IntValue() | right->IntValue(), m_location));

        case OP_bitshift_left:
            // right must be integer
            if (!right->IsNumber() || right->GetSymbolType() != BuiltinTypes::INT) {
                return nullptr;
            }

            return std::shared_ptr<AstInteger>(
                new AstInteger(IntValue() << right->IntValue(), m_location));

        case OP_bitshift_right:
            // right must be integer
            if (!right->IsNumber() || right->GetSymbolType() != BuiltinTypes::INT) {
                return nullptr;
            }

            return std::shared_ptr<AstInteger>(
                new AstInteger(IntValue() >> right->IntValue(), m_location));

        case OP_logical_and: {
            int this_true = IsTrue();
            int right_true = right->IsTrue();

            if (!right->IsNumber()) {
                // this operator is valid to compare against null
                if (dynamic_cast<AstNil*>(right)) {
                    // rhs is null, return false
                    return std::shared_ptr<AstFalse>(
                        new AstFalse(m_location));
                }
                return nullptr;
            }

            if (this_true == 1 && right_true == 1) {
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else if (this_true == 0 && right_true == 0) {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
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
                if (dynamic_cast<AstNil*>(right)) {
                    if (this_true == 1) {
                        return std::shared_ptr<AstTrue>(new AstTrue(m_location));
                    } else if (this_true == 0) {
                        return std::shared_ptr<AstFalse>(new AstFalse(m_location));
                    }
                }
                return nullptr;
            }

            if (this_true == 1 || right_true == 1) {
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else if (this_true == 0 || right_true == 0) {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
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
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
            }

        case OP_greater:
            if (!right->IsNumber()) {
                return nullptr;
            }
            if (IntValue() > right->IntValue()) {
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
            }

        case OP_less_eql:
            if (!right->IsNumber()) {
                return nullptr;
            }
            if (IntValue() <= right->IntValue()) {
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
            }

        case OP_greater_eql:
            if (!right->IsNumber()) {
                return nullptr;
            }
            if (IntValue() >= right->IntValue()) {
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
            }

        case OP_equals:
            if (!right->IsNumber()) {
                return nullptr;
            }
            if (IntValue() == right->IntValue()) {
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
            }

        case OP_negative:
            return std::shared_ptr<AstInteger>(new AstInteger(-IntValue(), m_location));

        case OP_bitwise_complement:
            return std::shared_ptr<AstInteger>(new AstInteger(~IntValue(), m_location));

        case OP_logical_not:
            if (IntValue() == 0) {
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            } else {
                return std::shared_ptr<AstFalse>(new AstFalse(m_location));
            }

        default:
            return nullptr;
    }
}

} // namespace compiler
} // namespace hyperion
