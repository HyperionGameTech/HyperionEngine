/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/json/parser/CompilerError.hpp>

namespace Hyperion::JSON {

const HashMap<ErrorMessage, String> CompilerError::errorMessageStrings {
    /* Fatal errors */
    { MSG_INTERNAL_ERROR, "Internal error" },
    { MSG_CUSTOM_ERROR, "%" },
    { MSG_NOT_IMPLEMENTED, "Feature '%' not implemented." },
    { MSG_ILLEGAL_SYNTAX, "Illegal syntax" },
    { MSG_ILLEGAL_EXPRESSION, "Illegal expression" },
    { MSG_ILLEGAL_OPERATOR, "Illegal usage of operator '%'" },
    { MSG_INVALID_OPERATOR_FOR_TYPE, "Operator '%' is not valid for type '%'" },
    { MSG_CANNOT_OVERLOAD_OPERATOR, "Operator '%' does not support overloading" },
    { MSG_CONST_MISSING_ASSIGNMENT, "'%': const value missing assignment" },
    { MSG_REF_MISSING_ASSIGNMENT, "'%': ref value missing assignment" },
    { MSG_CANNOT_CREATE_REFERENCE, "Cannot create a reference to this value" },
    { MSG_CANNOT_MODIFY_RVALUE, "The left hand side is not suitable for assignment" },
    { MSG_CONST_ASSIGNED_TO_NON_CONST_REF, "'%': const value assigned to a non-const ref." },
    { MSG_PROHIBITED_ACTION_ATTRIBUTE, "Attribute '%' prohibits this action" },
    { MSG_UNBALANCED_EXPRESSION, "Unbalanced expression" },
    { MSG_UNMATCHED_PARENTHESES, "Unmatched parentheses: Expected '}'" },
    { MSG_UNEXPECTED_CHARACTER, "Unexpected character '%'" },
    { MSG_UNEXPECTED_IDENTIFIER, "Unexpected identifier '%'" },
    { MSG_UNEXPECTED_TOKEN, "Unexpected token '%'" },
    { MSG_UNEXPECTED_EOF, "Unexpected end of file" },
    { MSG_UNEXPECTED_EOL, "Unexpected end of line" },
    { MSG_UNRECOGNIZED_ESCAPE_SEQUENCE, "Unrecognized escape sequence '%'" },
    { MSG_UNTERMINATED_STRING_LITERAL, "Unterminated string quotes" },
    { MSG_ARGUMENT_AFTER_VARARGS, "Argument not allowed after '...'" },
    { MSG_INCORRECT_NUMBER_OF_ARGUMENTS, "Incorrect number of arguments provided: % required, % given" },
    { MSG_MAXIMUM_NUMBER_OF_ARGUMENTS, "Maximum number of arguments exceeded" },
    { MSG_ARG_TYPE_INCOMPATIBLE, "% cannot be passed as %" },
    { MSG_NAMED_ARG_NOT_FOUND, "Could not find a parameter named '%'" },
    { MSG_REDECLARED_IDENTIFIER, "Identifier '%' has already been declared in this scope" },
    { MSG_REDECLARED_IDENTIFIER_TYPE, "'%' is the name of a type and cannot be used as an identifier" },
    { MSG_UNDECLARED_IDENTIFIER, "'%' is not declared in module %" },
    { MSG_EXPECTED_IDENTIFIER, "Expected an identifier" },
    { MSG_EXPECTED_TOKEN, "Expected '%'" }
};

CompilerError::CompilerError(const CompilerError& other)
    : m_level(other.m_level),
      m_msg(other.m_msg),
      m_location(other.m_location),
      m_text(other.m_text)
{
}

bool CompilerError::operator<(const CompilerError& other) const
{
    if (m_level != other.m_level)
    {
        return m_level < other.m_level;
    }

    if (m_location.GetFileName() != other.m_location.GetFileName())
    {
        return m_location.GetFileName() < other.m_location.GetFileName();
    }

    if (m_location.GetLine() != other.m_location.GetLine())
    {
        return m_location.GetLine() < other.m_location.GetLine();
    }

    if (m_location.GetColumn() != other.m_location.GetColumn())
    {
        return m_location.GetColumn() < other.m_location.GetColumn();
    }

    return m_text < other.m_text;
}

} // namespace Hyperion::JSON
