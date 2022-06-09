#include <script/compiler/Operator.hpp>

#include <array>

namespace hyperion::compiler {

const std::map<std::string, Operator> Operator::binary_operators = {
    { "+", Operator(OP_add, 13, ARITHMETIC) },
    { "-", Operator(OP_subtract, 13, ARITHMETIC) },
    { "*", Operator(OP_multiply, 14, ARITHMETIC) },
    { "/", Operator(OP_divide, 14, ARITHMETIC) },
    { "%", Operator(OP_modulus, 14, ARITHMETIC) },

    // Bitwise operators
    { "&", Operator(OP_bitwise_and, 9, BITWISE) },
    { "^", Operator(OP_bitwise_xor, 8, BITWISE) },
    { "|", Operator(OP_bitwise_or, 7, BITWISE) },
    { "<<", Operator(OP_bitshift_left, 12, BITWISE) },
    { ">>", Operator(OP_bitshift_right, 12, BITWISE) },

    // Logical operators
    { "&&", Operator(OP_logical_and, 6, LOGICAL) },
    { "||", Operator(OP_logical_or, 5, LOGICAL) },

    // Comparison operators
    { "==", Operator(OP_equals, 10, COMPARISON) },
    { "!=", Operator(OP_not_eql, 10, COMPARISON) },
    { "<", Operator(OP_less, 11, COMPARISON) },
    { ">", Operator(OP_greater, 11, COMPARISON) },
    { "<=", Operator(OP_less_eql, 11, COMPARISON) },
    { ">=", Operator(OP_greater_eql, 11, COMPARISON) },

    // Assignment operators
    { "=", Operator(OP_assign, 3, ASSIGNMENT, true) },
    { "+=", Operator(OP_add_assign, 3, ASSIGNMENT | ARITHMETIC, true) },
    { "-=", Operator(OP_subtract_assign, 3, ASSIGNMENT | ARITHMETIC, true) },
    { "*=", Operator(OP_multiply_assign, 3, ASSIGNMENT | ARITHMETIC, true) },
    { "/=", Operator(OP_divide_assign, 3, ASSIGNMENT | ARITHMETIC, true) },
    { "%=", Operator(OP_modulus_assign, 3, ASSIGNMENT | ARITHMETIC, true) },
    { "^=", Operator(OP_bitwise_xor_assign, 3, ASSIGNMENT | BITWISE, true) },
    { "&=", Operator(OP_bitwise_and_assign, 3, ASSIGNMENT | BITWISE, true) },
    { "|=", Operator(OP_bitwise_or_assign, 3, ASSIGNMENT | BITWISE, true) },
};

const std::map<std::string, Operator> Operator::unary_operators = {
    // Unary operators
    { "!", Operator(OP_logical_not, 0, LOGICAL) },
    { "-", Operator(OP_negative, 0, ARITHMETIC) },
    { "+", Operator(OP_positive, 0, ARITHMETIC) },
    { "~", Operator(OP_bitwise_complement, 0, BITWISE) },
    { "++", Operator(OP_increment, 0, ASSIGNMENT | ARITHMETIC, true) },
    { "--", Operator(OP_decrement, 0, ASSIGNMENT | ARITHMETIC, true) }
};

Operator::Operator(Operators op_type,
    int precedence,
    int type,
    bool modifies_value)
    : m_op_type(op_type),
      m_precedence(precedence),
      m_type(type),
      m_modifies_value(modifies_value)
{
}

Operator::Operator(const Operator &other)
    : m_op_type(other.m_op_type),
      m_precedence(other.m_precedence),
      m_type(other.m_type),
      m_modifies_value(other.m_modifies_value)
{
}

std::string Operator::LookupStringValue() const
{
    const std::map<std::string, Operator> *map = &Operator::binary_operators;

    if (IsUnary()) {
        map = &Operator::unary_operators;
    }

    for (auto it = map->begin(); it != map->end(); ++it) {
        if (it->second.GetOperatorType() == m_op_type) {
            return it->first;
        }
    }

    return "??";
}

} // namespace hyperion::compiler
