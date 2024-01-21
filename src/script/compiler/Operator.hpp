#ifndef OPERATOR_HPP
#define OPERATOR_HPP

#include <Types.hpp>
#include <core/lib/String.hpp>
#include <core/lib/HashMap.hpp>
#include <HashCode.hpp>

namespace hyperion::compiler {

using OperatorTypeBits = UInt32;

enum OperatorType : OperatorTypeBits
{
    ARITHMETIC = 0x1,
    BITWISE    = 0x2,
    LOGICAL    = 0x4,
    COMPARISON = 0x8,
    ASSIGNMENT = 0x10,
    PREFIX     = 0x20,
    POSTFIX    = 0x40
};

enum Operators
{
    OP_add,
    OP_subtract,
    OP_multiply,
    OP_divide,
    OP_modulus,

    OP_bitwise_xor,
    OP_bitwise_and,
    OP_bitwise_or,
    OP_bitshift_left,
    OP_bitshift_right,

    OP_logical_and,
    OP_logical_or,

    OP_equals,
    OP_not_eql,
    OP_less,
    OP_greater,
    OP_less_eql,
    OP_greater_eql,

    OP_assign,
    OP_add_assign,
    OP_subtract_assign,
    OP_multiply_assign,
    OP_divide_assign,
    OP_modulus_assign,
    OP_bitwise_xor_assign,
    OP_bitwise_and_assign,
    OP_bitwise_or_assign,

    OP_logical_not,
    OP_negative,
    OP_positive,
    OP_bitwise_complement,
    OP_increment,
    OP_decrement
};

class Operator
{
public:
    static const HashMap<String, Operator> binary_operators;
    static const HashMap<String, Operator> unary_operators;

    static inline Bool IsBinaryOperator(const String &str, OperatorTypeBits match_bits = 0)
    {
        const auto it = binary_operators.Find(str);

        if (it == binary_operators.end()) {
            return false;
        }

        if (match_bits == 0) {
            return true;
        }

        return Bool(it->second.GetType() & match_bits);
    }

    static inline Bool IsBinaryOperator(const String &str, const Operator *&out)
    {
        const auto it = binary_operators.Find(str);

        if (it == binary_operators.end()) {
            return false;
        }

        out = &it->second;

        return true;
    }

    static inline Bool IsBinaryOperator(const String &str, OperatorTypeBits match_bits, const Operator *&out)
    {
        const auto it = binary_operators.Find(str);

        if (it == binary_operators.end()) {
            return false;
        }

        if (match_bits == 0) {
            out = &it->second;

            return true;
        }

        if (it->second.GetType() & match_bits) {
            out = &it->second;

            return true;
        }

        return false;
    }

    static inline Bool IsUnaryOperator(const String &str, OperatorTypeBits match_bits = 0)
    {
        const auto it = unary_operators.Find(str);

        if (it == unary_operators.end()) {
            return false;
        }

        if (match_bits == 0) {
            return true;
        }

        return Bool(it->second.GetType() & match_bits);
    }

    static inline Bool IsUnaryOperator(const String &str, const Operator *&out)
    {
        const auto it = unary_operators.Find(str);

        if (it == unary_operators.end()) {
            return false;
        }

        out = &it->second;

        return true;
    }

    static inline Bool IsUnaryOperator(const String &str, OperatorTypeBits match_bits, const Operator *&out)
    {
        const auto it = unary_operators.Find(str);

        if (it == unary_operators.end()) {
            return false;
        }

        if (match_bits == 0) {
            out = &it->second;

            return true;
        }

        if (it->second.GetType() & match_bits) {
            out = &it->second;

            return true;
        }

        return false;
    }

    static const Operator *FindBinaryOperator(Operators op);
    static const Operator *FindUnaryOperator(Operators op);

public:
    Operator(
        Operators op_type,
        Int precedence,
        Int type,
        Bool modifies_value = false,
        Bool supports_overloading = false
    );
    Operator(const Operator &other);

    Operators GetOperatorType() const
        { return m_op_type; }

    Int GetType() const
        { return m_type; }

    Int GetPrecedence() const
        { return m_precedence; }

    Bool IsUnary() const
        { return m_precedence == 0; }

    Bool ModifiesValue() const
        { return m_modifies_value; }

    Bool SupportsOverloading() const
        { return m_supports_overloading; }

    String LookupStringValue() const;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_op_type);
        hc.Add(m_precedence);
        hc.Add(m_type);
        hc.Add(m_modifies_value);
        hc.Add(m_supports_overloading);

        return hc;
    }

private:
    Operators   m_op_type;
    Int         m_precedence;
    Int         m_type;
    Bool        m_modifies_value;
    Bool        m_supports_overloading;
};

} // namespace hyperion::compiler

#endif
