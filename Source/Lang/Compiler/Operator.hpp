#pragma once

#include <Core/Types.hpp>
#include <Core/Containers/String.hpp>
#include <Core/Containers/FlatMap.hpp>
#include <Core/HashCode.hpp>

namespace Hyperion {

using OperatorTypeBits = uint32;

enum OperatorType : OperatorTypeBits
{
    ARITHMETIC = 0x1,
    BITWISE = 0x2,
    LOGICAL = 0x4,
    COMPARISON = 0x8,
    ASSIGNMENT = 0x10,
    PREFIX = 0x20,
    POSTFIX = 0x40
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

class Operator;

class Operator
{
public:
    static const FlatMap<String, Operator*> binaryOperators;
    static const FlatMap<String, Operator*> unaryOperators;

    static inline bool IsBinaryOperator(const String& str, OperatorTypeBits matchBits = 0)
    {
        const auto it = binaryOperators.Find(str);

        if (it == binaryOperators.End())
        {
            return false;
        }

        if (matchBits == 0)
        {
            return true;
        }

        return bool(it->second->GetType() & matchBits);
    }

    static inline bool IsBinaryOperator(const String& str, const Operator*& out)
    {
        const auto it = binaryOperators.Find(str);

        if (it == binaryOperators.End())
        {
            return false;
        }

        out = it->second;

        return true;
    }

    static inline bool IsBinaryOperator(const String& str, OperatorTypeBits matchBits, const Operator*& out)
    {
        const auto it = binaryOperators.Find(str);

        if (it == binaryOperators.End())
        {
            return false;
        }

        if (matchBits == 0)
        {
            out = it->second;

            return true;
        }

        if (it->second->GetType() & matchBits)
        {
            out = it->second;

            return true;
        }

        return false;
    }

    static inline bool IsUnaryOperator(const String& str, OperatorTypeBits matchBits = 0)
    {
        const auto it = unaryOperators.Find(str);

        if (it == unaryOperators.End())
        {
            return false;
        }

        if (matchBits == 0)
        {
            return true;
        }

        return bool(it->second->GetType() & matchBits);
    }

    static inline bool IsUnaryOperator(const String& str, const Operator*& out)
    {
        const auto it = unaryOperators.Find(str);

        if (it == unaryOperators.End())
        {
            return false;
        }

        out = it->second;

        return true;
    }

    static inline bool IsUnaryOperator(const String& str, OperatorTypeBits matchBits, const Operator*& out)
    {
        const auto it = unaryOperators.Find(str);

        if (it == unaryOperators.End())
        {
            return false;
        }

        if (matchBits == 0)
        {
            out = it->second;

            return true;
        }

        if (it->second->GetType() & matchBits)
        {
            out = it->second;

            return true;
        }

        return false;
    }

    static const Operator* FindBinaryOperator(Operators op);
    static const Operator* FindUnaryOperator(Operators op);

    static const Operator* GetNonAssignmentVariant(const Operator* op);

public:
    Operator(
        Operators opType,
        int precedence,
        int type,
        bool modifiesValue = false,
        bool supportsOverloading = false);
    Operator(const Operator& other);

    Operators GetOperatorType() const
    {
        return m_opType;
    }

    int GetType() const
    {
        return m_type;
    }

    int GetPrecedence() const
    {
        return m_precedence;
    }

    bool IsUnary() const
    {
        return m_precedence == 0;
    }

    bool ModifiesValue() const
    {
        return m_modifiesValue;
    }

    bool SupportsOverloading() const
    {
        return m_supportsOverloading;
    }

    String LookupStringValue() const;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_opType);
        hc.Add(m_precedence);
        hc.Add(m_type);
        hc.Add(m_modifiesValue);
        hc.Add(m_supportsOverloading);

        return hc;
    }

private:
    Operators m_opType;
    int m_precedence;
    int m_type;
    bool m_modifiesValue;
    bool m_supportsOverloading;
};

} // namespace Hyperion
