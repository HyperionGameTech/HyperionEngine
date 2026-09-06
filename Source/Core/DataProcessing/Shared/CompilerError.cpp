#include <Core/DataProcessing/Shared/CompilerError.hpp>

namespace Hyperion::DataProcessing {

const Map<ErrorMessage, String> CompilerError::s_errorMessageStrings {
    /* Generic / lexical errors */
    { MSG_INTERNAL_ERROR, "Internal error" },
    { MSG_CUSTOM_ERROR, "%" },
    { MSG_NOT_IMPLEMENTED, "Feature '%' not implemented." },
    { MSG_ILLEGAL_SYNTAX, "Illegal syntax" },
    { MSG_ILLEGAL_EXPRESSION, "Illegal expression" },
    { MSG_ILLEGAL_OPERATOR, "Illegal usage of operator '%'" },
    { MSG_UNEXPECTED_CHARACTER, "Unexpected character '%'" },
    { MSG_UNEXPECTED_IDENTIFIER, "Unexpected identifier '%'" },
    { MSG_UNEXPECTED_TOKEN, "Unexpected token '%'" },
    { MSG_UNEXPECTED_EOF, "Unexpected end of file" },
    { MSG_UNEXPECTED_EOL, "Unexpected end of line" },
    { MSG_UNRECOGNIZED_ESCAPE_SEQUENCE, "Unrecognized escape sequence '%'" },
    { MSG_UNTERMINATED_STRING_LITERAL, "Unterminated string quotes" },
    { MSG_EXPECTED_IDENTIFIER, "Expected an identifier" },
    { MSG_EXPECTED_TOKEN, "Expected '%'" },

    /* Expression / type errors */
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
    { MSG_ARGUMENT_AFTER_VARARGS, "Argument not allowed after '...'" },
    { MSG_INCORRECT_NUMBER_OF_ARGUMENTS, "Incorrect number of arguments provided: % required, % given" },
    { MSG_MAXIMUM_NUMBER_OF_ARGUMENTS, "Maximum number of arguments exceeded" },
    { MSG_ARG_TYPE_INCOMPATIBLE, "% cannot be passed as %" },
    { MSG_NAMED_ARG_NOT_FOUND, "Could not find a parameter named '%'" },
    { MSG_REDECLARED_IDENTIFIER, "Identifier '%' has already been declared in this scope" },
    { MSG_REDECLARED_IDENTIFIER_TYPE, "'%' is the name of a type and cannot be used as an identifier" },
    { MSG_UNDECLARED_IDENTIFIER, "'%' is not declared in module %" },

    /* HMF-specific semantic errors */
    { MSG_UNKNOWN_FIELD, "Class '%' has no field '%'" },
    { MSG_CANNOT_ASSIGN_PROPERTY, "Property '%' of type '%' is not assignable" },
    { MSG_UNRESOLVED_ENUM_NAME, "Enum '%' has no value named '%'" },
    { MSG_TYPE_MISMATCH, "Value of type '%' cannot be assigned to '%'" },
    { MSG_CLASS_NOT_FOUND, "Class '%' has not been registered" },
    { MSG_CLASS_NOT_DERIVED, "Class '%' is not derived from '%'" },
    { MSG_NOT_AN_ENUM_FLAGS_TYPE, "Flag-list syntax used on a type '%' that is not an EnumFlags type" },
    { MSG_NOT_AN_ENUM_TYPE, "Bare enum name '%' used on a type that is not an enum" },
    { MSG_UNKNOWN_VARIANT_TAG, "Variant cannot be matched to any alternative type for value: %" },
    { MSG_INVALID_LITERAL_FOR_TYPE, "Literal '%' is not valid for type '%'" },
    { MSG_UNBALANCED_BRACES, "Unbalanced braces: Expected '}'" },
    { MSG_UNBALANCED_BRACKETS, "Unbalanced brackets: Expected ']'" },
    { MSG_UNKNOWN_ASSET_PATH, "Asset path '%' could not be resolved" },
    { MSG_UNKNOWN_OVERRIDE_SECTION, "Unknown override section: '%'" },
    { MSG_OVERRIDE_SECTION_PARSE_FAILED, "Failed to materialize override section '%' for type '%'" }
};

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

} // namespace Hyperion::DataProcessing
