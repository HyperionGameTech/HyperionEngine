#include <Lang/Compiler/Operator.hpp>

#include <array>

namespace Hyperion {

const TFlatMap<String, Operator*> Operator::binaryOperators = {
    { "+", new Operator(OP_add, 13, ARITHMETIC, false, true) },
    { "-", new Operator(OP_subtract, 13, ARITHMETIC, false, true) },
    { "*", new Operator(OP_multiply, 14, ARITHMETIC, false, true) },
    { "/", new Operator(OP_divide, 14, ARITHMETIC, false, true) },
    { "%", new Operator(OP_modulus, 14, ARITHMETIC, false, true) },

    // Bitwise operators
    { "&", new Operator(OP_bitwise_and, 9, BITWISE, false, true) },
    { "^", new Operator(OP_bitwise_xor, 8, BITWISE, false, true) },
    { "|", new Operator(OP_bitwise_or, 7, BITWISE, false, true) },
    { "<<", new Operator(OP_bitshift_left, 12, BITWISE, false, true) },
    { ">>", new Operator(OP_bitshift_right, 12, BITWISE, false, true) },

    // Logical operators
    { "&&", new Operator(OP_logical_and, 6, LOGICAL, false, true) },
    { "||", new Operator(OP_logical_or, 5, LOGICAL, false, true) },

    // Comparison operators
    { "==", new Operator(OP_equals, 10, COMPARISON, false, true) },
    { "!=", new Operator(OP_not_eql, 10, COMPARISON, false, true) },
    { "<", new Operator(OP_less, 11, COMPARISON, false, true) },
    { ">", new Operator(OP_greater, 11, COMPARISON, false, true) },
    { "<=", new Operator(OP_less_eql, 11, COMPARISON, false, true) },
    { ">=", new Operator(OP_greater_eql, 11, COMPARISON, false, true) },

    // Assignment operators
    { "=", new Operator(OP_assign, 3, ASSIGNMENT, true) },
    { "+=", new Operator(OP_add_assign, 3, ASSIGNMENT | ARITHMETIC, true) },
    { "-=", new Operator(OP_subtract_assign, 3, ASSIGNMENT | ARITHMETIC, true) },
    { "*=", new Operator(OP_multiply_assign, 3, ASSIGNMENT | ARITHMETIC, true) },
    { "/=", new Operator(OP_divide_assign, 3, ASSIGNMENT | ARITHMETIC, true) },
    { "%=", new Operator(OP_modulus_assign, 3, ASSIGNMENT | ARITHMETIC, true) },
    { "^=", new Operator(OP_bitwise_xor_assign, 3, ASSIGNMENT | BITWISE, true) },
    { "&=", new Operator(OP_bitwise_and_assign, 3, ASSIGNMENT | BITWISE, true) },
    { "|=", new Operator(OP_bitwise_or_assign, 3, ASSIGNMENT | BITWISE, true) },
};

const TFlatMap<String, Operator*> Operator::unaryOperators = {
    // Unary operators
    { "!", new Operator(OP_logical_not, 0, LOGICAL | PREFIX, false, true) },
    { "-", new Operator(OP_negative, 0, ARITHMETIC | PREFIX, false, true) },
    { "+", new Operator(OP_positive, 0, ARITHMETIC | PREFIX, false, true) },
    { "~", new Operator(OP_bitwise_complement, 0, BITWISE | PREFIX, false, true) },
    { "++", new Operator(OP_increment, 0, ASSIGNMENT | ARITHMETIC | POSTFIX | PREFIX, true, true) },
    { "--", new Operator(OP_decrement, 0, ASSIGNMENT | ARITHMETIC | POSTFIX | PREFIX, true, true) }
};

Operator::Operator(
    Operators opType,
    int precedence,
    int type,
    bool modifiesValue,
    bool supportsOverloading)
    : m_opType(opType),
      m_precedence(precedence),
      m_type(type),
      m_modifiesValue(modifiesValue),
      m_supportsOverloading(supportsOverloading)
{
}

Operator::Operator(const Operator& other)
    : m_opType(other.m_opType),
      m_precedence(other.m_precedence),
      m_type(other.m_type),
      m_modifiesValue(other.m_modifiesValue),
      m_supportsOverloading(other.m_supportsOverloading)
{
}

String Operator::LookupStringValue() const
{
    const auto* map = &Operator::binaryOperators;

    if (IsUnary())
    {
        map = &Operator::unaryOperators;
    }

    for (const auto& it : *map)
    {
        if (it.second->GetOperatorType() == m_opType)
        {
            return it.first;
        }
    }

    return "??";
}

const Operator* Operator::FindBinaryOperator(Operators op)
{
    for (auto& it : binaryOperators)
    {
        if (it.second->GetOperatorType() == op)
        {
            return it.second;
        }
    }

    return nullptr;
}

const Operator* Operator::FindUnaryOperator(Operators op)
{
    for (auto& it : unaryOperators)
    {
        if (it.second->GetOperatorType() == op)
        {
            return it.second;
        }
    }

    return nullptr;
}

const Operator* Operator::GetNonAssignmentVariant(const Operator* op)
{
    if (!op)
    {
        return nullptr;
    }

    if (!op->ModifiesValue())
    {
        return op;
    }

    switch (op->GetOperatorType())
    {
    case Operators::OP_add_assign:
        return FindBinaryOperator(Operators::OP_add);
    case Operators::OP_subtract_assign:
        return FindBinaryOperator(Operators::OP_subtract);
    case Operators::OP_multiply_assign:
        return FindBinaryOperator(Operators::OP_multiply);
    case Operators::OP_divide_assign:
        return FindBinaryOperator(Operators::OP_divide);
    case Operators::OP_modulus_assign:
        return FindBinaryOperator(Operators::OP_modulus);
    case Operators::OP_bitwise_and_assign:
        return FindBinaryOperator(Operators::OP_bitwise_and);
    case Operators::OP_bitwise_or_assign:
        return FindBinaryOperator(Operators::OP_bitwise_or);
    case Operators::OP_bitwise_xor_assign:
        return FindBinaryOperator(Operators::OP_bitwise_xor);
    default:
        return op;
    }
}

} // namespace Hyperion
