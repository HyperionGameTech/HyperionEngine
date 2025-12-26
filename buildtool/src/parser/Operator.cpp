#include <parser/Operator.hpp>

namespace Hyperion::CodeGen {

const Operator::OperatorMap Operator::s_binaryOperators = {
    { "+", Operator(OP_add, 13, ARITHMETIC, false, true) },
    { "-", Operator(OP_subtract, 13, ARITHMETIC, false, true) },
    { "*", Operator(OP_multiply, 14, ARITHMETIC, false, true) },
    { "/", Operator(OP_divide, 14, ARITHMETIC, false, true) },
    { "%", Operator(OP_modulus, 14, ARITHMETIC, false, true) },

    // Bitwise operators
    { "&", Operator(OP_bitwise_and, 9, BITWISE, false, true) },
    { "^", Operator(OP_bitwise_xor, 8, BITWISE, false, true) },
    { "|", Operator(OP_bitwise_or, 7, BITWISE, false, true) },
    { "<<", Operator(OP_bitshift_left, 12, BITWISE, false, true) },
    { ">>", Operator(OP_bitshift_right, 12, BITWISE, false, true) },

    // Logical operators
    { "&&", Operator(OP_logical_and, 6, LOGICAL, false, true) },
    { "||", Operator(OP_logical_or, 5, LOGICAL, false, true) },

    // Comparison operators
    { "==", Operator(OP_equals, 10, COMPARISON, false, true) },
    { "!=", Operator(OP_not_eql, 10, COMPARISON, false, true) },
    { "<", Operator(OP_less, 11, COMPARISON, false, true) },
    { ">", Operator(OP_greater, 11, COMPARISON, false, true) },
    { "<=", Operator(OP_less_eql, 11, COMPARISON, false, true) },
    { ">=", Operator(OP_greater_eql, 11, COMPARISON, false, true) },

    // Assignment operators
    { "=", Operator(OP_assign, 3, ASSIGNMENT, true, false) },
    { "+=", Operator(OP_add_assign, 3, ASSIGNMENT | ARITHMETIC, true, true) },
    { "-=", Operator(OP_subtract_assign, 3, ASSIGNMENT | ARITHMETIC, true, true) },
    { "*=", Operator(OP_multiply_assign, 3, ASSIGNMENT | ARITHMETIC, true, true) },
    { "/=", Operator(OP_divide_assign, 3, ASSIGNMENT | ARITHMETIC, true, true) },
    { "%=", Operator(OP_modulus_assign, 3, ASSIGNMENT | ARITHMETIC, true, true) },
    { "^=", Operator(OP_bitwise_xor_assign, 3, ASSIGNMENT | BITWISE, true, true) },
    { "&=", Operator(OP_bitwise_and_assign, 3, ASSIGNMENT | BITWISE, true, true) },
    { "|=", Operator(OP_bitwise_or_assign, 3, ASSIGNMENT | BITWISE, true, true) },
};

const Operator::OperatorMap Operator::s_unaryOperators = {
    // Unary operators
    { "!", Operator(OP_logical_not, 0, LOGICAL | PREFIX, false, true) },
    { "-", Operator(OP_negative, 0, ARITHMETIC | PREFIX, false, true) },
    { "+", Operator(OP_positive, 0, ARITHMETIC | PREFIX, false, true) },
    { "~", Operator(OP_bitwise_complement, 0, BITWISE | PREFIX, false, true) },
    { "++", Operator(OP_increment, 0, ASSIGNMENT | ARITHMETIC | POSTFIX | PREFIX, true, true) },
    { "--", Operator(OP_decrement, 0, ASSIGNMENT | ARITHMETIC | POSTFIX | PREFIX, true, true) }
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
    const OperatorMap* map = &Operator::s_binaryOperators;

    if (IsUnary())
    {
        map = &Operator::s_unaryOperators;
    }

    for (const auto& it : *map)
    {
        if (it.second.GetOperatorType() == m_opType)
        {
            return it.first;
        }
    }

    return "??";
}

bool Operator::IsBinaryOperator(UTF8StringView str, OperatorTypeBits matchBits)
{
    const auto it = s_binaryOperators.FindAs(str);

    if (it == s_binaryOperators.end())
    {
        return false;
    }

    if (matchBits == 0)
    {
        return true;
    }

    return bool(it->second.GetType() & matchBits);
}

bool Operator::IsBinaryOperator(UTF8StringView str, const Operator*& out)
{
    const auto it = s_binaryOperators.FindAs(str);

    if (it == s_binaryOperators.end())
    {
        return false;
    }

    out = &it->second;

    return true;
}

bool Operator::IsBinaryOperator(UTF8StringView str, OperatorTypeBits matchBits, const Operator*& out)
{
    const auto it = s_binaryOperators.FindAs(str);

    if (it == s_binaryOperators.end())
    {
        return false;
    }

    if (matchBits == 0)
    {
        out = &it->second;

        return true;
    }

    if (it->second.GetType() & matchBits)
    {
        out = &it->second;

        return true;
    }

    return false;
}

bool Operator::IsUnaryOperator(UTF8StringView str, OperatorTypeBits matchBits)
{
    const auto it = s_unaryOperators.FindAs(str);

    if (it == s_unaryOperators.end())
    {
        return false;
    }

    if (matchBits == 0)
    {
        return true;
    }

    return bool(it->second.GetType() & matchBits);
}

bool Operator::IsUnaryOperator(UTF8StringView str, const Operator*& out)
{
    const auto it = s_unaryOperators.FindAs(str);

    if (it == s_unaryOperators.end())
    {
        return false;
    }

    out = &it->second;

    return true;
}

bool Operator::IsUnaryOperator(UTF8StringView str, OperatorTypeBits matchBits, const Operator*& out)
{
    const auto it = s_unaryOperators.FindAs(str);

    if (it == s_unaryOperators.end())
    {
        return false;
    }

    if (matchBits == 0)
    {
        out = &it->second;

        return true;
    }

    if (it->second.GetType() & matchBits)
    {
        out = &it->second;

        return true;
    }

    return false;
}

const Operator* Operator::FindBinaryOperator(Operators op)
{
    for (auto& it : s_binaryOperators)
    {
        if (it.second.GetOperatorType() == op)
        {
            return &it.second;
        }
    }

    return nullptr;
}

const Operator* Operator::FindUnaryOperator(Operators op)
{
    for (auto& it : s_unaryOperators)
    {
        if (it.second.GetOperatorType() == op)
        {
            return &it.second;
        }
    }

    return nullptr;
}

} // namespace Hyperion::CodeGen