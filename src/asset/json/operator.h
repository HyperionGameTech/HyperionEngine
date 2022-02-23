#ifndef HYPERION_JSON_OPERATOR_H
#define HYPERION_JSON_OPERATOR_H

#include <map>
#include <string>

namespace hyperion {
namespace json {

enum OperatorType {
    ARITHMETIC = 1,
    BITWISE = 2,
    LOGICAL = 4,
    COMPARISON = 8,
    ASSIGNMENT = 16,
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

    static inline bool IsBinaryOperator(const std::string &str)
    {
        return binary_operators.find(str) != binary_operators.end();
    }
    static inline bool IsBinaryOperator(const std::string &str, const Operator *&out)
    {
        auto it = binary_operators.find(str);
        if (it != binary_operators.end()) {
            out = &it->second;
            return true;
        }
        return false;
    }
    static inline bool IsUnaryOperator(const std::string &str)
    {
        return unary_operators.find(str) != unary_operators.end();
    }
    static inline bool IsUnaryOperator(const std::string &str, const Operator *&out)
    {
        auto it = unary_operators.find(str);
        if (it != unary_operators.end()) {
            out = &it->second;
            return true;
        }
        return false;
    }

public:
    Operator(Operators op_type,
        int precedence,
        int type,
        bool modifies_value = false);
    Operator(const Operator &other);

    inline Operators GetOperatorType() const
    {
        return m_op_type;
    }
    inline int GetType() const
    {
        return m_type;
    }
    inline int GetPrecedence() const
    {
        return m_precedence;
    }
    inline bool IsUnary() const
    {
        return m_precedence == 0;
    }
    inline bool ModifiesValue() const
    {
        return m_modifies_value;
    }

    std::string LookupStringValue() const;

private:
    Operators m_op_type;
    int m_precedence;
    int m_type;
    bool m_modifies_value;
};

} // namespace json
} // namespace hyperion

#endif