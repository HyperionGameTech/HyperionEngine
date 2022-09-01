#ifndef OPERATOR_HPP
#define OPERATOR_HPP

#include <map>
#include <string>

namespace hyperion::compiler {

using OperatorTypeBits = uint32_t;

enum OperatorType : OperatorTypeBits {
    ARITHMETIC = 1,
    BITWISE    = 2,
    LOGICAL    = 4,
    COMPARISON = 8,
    ASSIGNMENT = 16,

    PREFIX     = 32,
    POSTFIX    = 64
};

enum Operators {
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

class Operator {
public:
    static const std::map<std::string, Operator> binary_operators;
    static const std::map<std::string, Operator> unary_operators;

    static inline bool IsBinaryOperator(const std::string &str, OperatorTypeBits match_bits = 0)
    {
        const auto it = binary_operators.find(str);

        if (it == binary_operators.end()) {
            return false;
        }

        if (match_bits == 0) {
            return true;
        }

        return bool(it->second.GetType() & match_bits);
    }

    static inline bool IsBinaryOperator(const std::string &str, const Operator *&out)
    {
        const auto it = binary_operators.find(str);

        if (it == binary_operators.end()) {
            return false;
        }

        out = &it->second;

        return true;
    }

    static inline bool IsBinaryOperator(const std::string &str, OperatorTypeBits match_bits, const Operator *&out)
    {
        const auto it = binary_operators.find(str);

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

    static inline bool IsUnaryOperator(const std::string &str, OperatorTypeBits match_bits = 0)
    {
        const auto it = unary_operators.find(str);

        if (it == unary_operators.end()) {
            return false;
        }

        if (match_bits == 0) {
            return true;
        }

        return bool(it->second.GetType() & match_bits);
    }

    static inline bool IsUnaryOperator(const std::string &str, const Operator *&out)
    {
        const auto it = unary_operators.find(str);

        if (it == unary_operators.end()) {
            return false;
        }

        out = &it->second;

        return true;
    }

    static inline bool IsUnaryOperator(const std::string &str, OperatorTypeBits match_bits, const Operator *&out)
    {
        const auto it = unary_operators.find(str);

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
        int precedence,
        int type,
        bool modifies_value = false,
        bool supports_overloading = false
    );
    Operator(const Operator &other);

    Operators GetOperatorType() const
        { return m_op_type; }
    int GetType() const
        { return m_type; }
    int GetPrecedence() const
        { return m_precedence; }
    bool IsUnary() const
        { return m_precedence == 0; }
    bool ModifiesValue() const
        { return m_modifies_value; }
    bool SupportsOverloading() const
        { return m_supports_overloading; }

    std::string LookupStringValue() const;

private:
    Operators m_op_type;
    int m_precedence;
    int m_type;
    bool m_modifies_value;
    bool m_supports_overloading;
};

} // namespace hyperion::compiler

#endif
