#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/AstVisitor.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeUtil.hpp>

#include <limits>
#include <cmath>

namespace hyperion::compiler {

AstFloat::AstFloat(hyperion::Float32 value, const SourceLocation &location)
    : AstConstant(location),
      m_value(value)
{
}

std::unique_ptr<Buildable> AstFloat::Build(AstVisitor *visitor, Module *mod)
{
    // get active register
    const UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
    return BytecodeUtil::Make<ConstF32>(rp, m_value);
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

hyperion::Int32 AstFloat::IntValue() const
{
    return (hyperion::Int32)m_value;
}

hyperion::UInt32 AstFloat::UnsignedValue() const
{
    return (hyperion::UInt32)m_value;
}

hyperion::Float32 AstFloat::FloatValue() const
{
    return m_value;
}

SymbolTypePtr_t AstFloat::GetExprType() const
{
    return BuiltinTypes::FLOAT;
}

RC<AstConstant> AstFloat::HandleOperator(Operators op_type, const AstConstant *right) const
{
    switch (op_type) {
        case OP_add:
            if (!right->IsNumber()) {
                return nullptr;
            }
            return RC<AstFloat>(
                new AstFloat(FloatValue() + right->FloatValue(), m_location));

        case OP_subtract:
            if (!right->IsNumber()) {
                return nullptr;
            }
            return RC<AstFloat>(
                new AstFloat(FloatValue() - right->FloatValue(), m_location));

        case OP_multiply:
            if (!right->IsNumber()) {
                return nullptr;
            }
            return RC<AstFloat>(
                new AstFloat(FloatValue() * right->FloatValue(), m_location));

        case OP_divide: {
            if (!right->IsNumber()) {
                return nullptr;
            }

            auto right_float = right->FloatValue();
            if (right_float == 0.0) {
                // division by zero
                return nullptr;
            }
            return RC<AstFloat>(new AstFloat(FloatValue() / right_float, m_location));
        }

        case OP_modulus: {
            if (!right->IsNumber()) {
                return nullptr;
            }

            auto right_float = right->FloatValue();
            if (right_float == 0.0) {
                // division by zero
                return nullptr;
            }

            return RC<AstFloat>(new AstFloat(std::fmod(FloatValue(), right_float), m_location));
        }

        case OP_logical_and: {
            int this_true = IsTrue();
            int right_true = right->IsTrue();

            if (!right->IsNumber()) {
                // this operator is valid to compare against null
                if (dynamic_cast<const AstNil*>(right)) {
                    // rhs is null, return false
                    return RC<AstFalse>(new AstFalse(m_location));
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
            if (FloatValue() < right->FloatValue()) {
                return RC<AstTrue>(new AstTrue(m_location));
            } else {
                return RC<AstFalse>(new AstFalse(m_location));
            }

        case OP_greater:
            if (!right->IsNumber()) {
                return nullptr;
            }
            if (FloatValue() > right->FloatValue()) {
                return RC<AstTrue>(new AstTrue(m_location));
            } else {
                return RC<AstFalse>(new AstFalse(m_location));
            }

        case OP_less_eql:
            if (!right->IsNumber()) {
                return nullptr;
            }
            if (FloatValue() <= right->FloatValue()) {
                return RC<AstTrue>(new AstTrue(m_location));
            } else {
                return RC<AstFalse>(new AstFalse(m_location));
            }

        case OP_greater_eql:
            if (!right->IsNumber()) {
                return nullptr;
            }
            if (FloatValue() >= right->FloatValue()) {
                return RC<AstTrue>(new AstTrue(m_location));
            } else {
                return RC<AstFalse>(new AstFalse(m_location));
            }

        case OP_equals:
            if (!right->IsNumber()) {
                return nullptr;
            }
            if (FloatValue() == right->FloatValue()) {
                return RC<AstTrue>(new AstTrue(m_location));
            } else {
                return RC<AstFalse>(new AstFalse(m_location));
            }

        case OP_negative:
            return RC<AstFloat>(new AstFloat(-FloatValue(), m_location));

        case OP_logical_not:
            if (FloatValue() == 0.0) {
                return RC<AstTrue>(new AstTrue(m_location));
            } else {
                return RC<AstFalse>(new AstFalse(m_location));
            }

        default:
            return nullptr;
    }
}

} // namespace hyperion::compiler
