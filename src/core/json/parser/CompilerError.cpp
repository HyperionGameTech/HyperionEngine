/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/json/parser/CompilerError.hpp>

namespace hyperion::json {

const HashMap<ErrorMessage, String> CompilerError::error_message_strings {
    /* Fatal errors */
    { MSG_INTERNAL_ERROR, "Internal error" },
    { MSG_CUSTOM_ERROR, "%" },
    { MSG_NOT_IMPLEMENTED, "Feature '%' not implemented." },
    { MSG_ILLEGAL_SYNTAX, "Illegal syntax" },
    { MSG_ILLEGAL_EXPRESSION, "Illegal expression" },
    { MSG_ILLEGAL_OPERATOR, "Illegal usage of operator '%'" },
    { MSG_INVALID_OPERATOR_FOR_TYPE, "Operator '%' is not valid for type '%'" },
    { MSG_CANNOT_OVERLOAD_OPERATOR, "Operator '%' does not support overloading" },
    { MSG_INVALID_SYMBOL_QUERY, "Unknown symbol query '%'" },
    { MSG_EXPRESSION_CANNOT_BE_MODIFIED, "Expression cannot be modified" },
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
    { MSG_INCOMPATIBLE_CAST, "% cannot be converted to %" },
    { MSG_NAMED_ARG_NOT_FOUND, "Could not find a parameter named '%'" },
    { MSG_REDECLARED_IDENTIFIER, "Identifier '%' has already been declared in this scope" },
    { MSG_REDECLARED_IDENTIFIER_TYPE, "'%' is the name of a type and cannot be used as an identifier" },
    { MSG_UNDECLARED_IDENTIFIER, "'%' is not declared in module %" },
    { MSG_EXPECTED_IDENTIFIER, "Expected an identifier" },
    { MSG_KEYWORD_CANNOT_BE_USED_AS_IDENTIFIER, "Keyword '%' cannot be used as a name in this case" },
    { MSG_AMBIGUOUS_IDENTIFIER, "Identifier '%' is ambiguous" },
    { MSG_INVALID_CONSTRUCTOR, "Invalid constructor" },
    { MSG_RETURN_INVALID_IN_CONSTRUCTOR, "'return' statement invalid in constructor definition" },
    { MSG_RETURN_TYPE_SPECIFICATION_INVALID_ON_CONSTRUCTOR, "Return type specification invalid on constructor definition" },
    { MSG_EXPECTED_TYPE_GOT_IDENTIFIER, "'%' is an identifier, expected a type" },
    { MSG_MISSING_TYPE_AND_ASSIGNMENT, "No type or assignment has been provided for '%'" },
    { MSG_TYPE_NO_DEFAULT_ASSIGNMENT, "Type '%' has no default assignment" },
    { MSG_COULD_NOT_DEDUCE_TYPE_FOR_EXPRESSION, "%: could not deduce type from expression" },
    { MSG_EXPRESSION_NOT_GENERIC, "Generic arguments provided to non-generic type, '%'" },
    { MSG_TOO_MANY_GENERIC_ARGS, "Too many generic arguments provided: % required, found %" },
    { MSG_TOO_FEW_GENERIC_ARGS, "Too few generic arguments provided: % required, found %" },
    { MSG_NO_SUBSTITUTION_FOR_GENERIC_ARG, "No substitution found for generic parameter %" },
    { MSG_ENUM_ASSIGNMENT_NOT_CONSTANT, "Assignment for enum member '%' is not a constant." },
    { MSG_GENERIC_ARG_MAY_NOT_HAVE_SIDE_EFFECTS, "Generic argument may not have side effects" },
    { MSG_BREAK_OUTSIDE_LOOP, "'break' cannot be used outside of a loop or switch statement" },
    { MSG_CONTINUE_OUTSIDE_LOOP, "'continue' cannot be used outside of a loop" },
    { MSG_MULTIPLE_RETURN_TYPES, "Function has more than one possible return type" },
    { MSG_MISMATCHED_RETURN_TYPE, "Function is marked to return '%', but attempting to return '%'" },
    { MSG_MUST_BE_EXPLICITLY_MARKED_ANY, "Function must be explicitly marked to return 'any'" },
    { MSG_ANY_RESERVED_FOR_PARAMETERS, "'any' type is reserved for function parameters" },
    { MSG_RETURN_OUTSIDE_FUNCTION, "'return' not allowed outside of function body" },
    { MSG_YIELD_OUTSIDE_FUNCTION, "'yield' not allowed outside of function body" },
    { MSG_YIELD_OUTSIDE_GENERATOR_FUNCTION, "'yield' only allowed within generator functions" },
    { MSG_NOT_A_FUNCTION, "An object of type '%' is not callable as a function" },
    { MSG_MEMBER_NOT_A_METHOD, "Data member '%' is not a method" },
    { MSG_CLOSURE_CAPTURE_MUST_BE_PARAMETER, "'%' was declared in a function above this one, and must be passed as a parameter to be captured" },
    { MSG_PURE_FUNCTION_SCOPE, "variables declared from an outside scope may not be used in a pure function" },
    { MSG_INVALID_SUBSCRIPT, "Subscript operator invalid on type '%'" },
    { MSG_NOT_A_TYPE, "'%' is not a type" },
    { MSG_UNDEFINED_TYPE, "'%' is not a built-in or user-defined type" },
    { MSG_REDEFINED_TYPE, "Type '%' has already been defined in this module" },
    { MSG_REDEFINED_BUILTIN_TYPE, "Cannot create type '%', it is a built-in type" },
    { MSG_TYPE_NOT_DEFINED_GLOBALLY, "Type definitions are not allowed in local scopes" },
    { MSG_IDENTIFIER_IS_TYPE, "'%' is the name of a type, expected an identifier" },
    { MSG_CANNOT_DETERMINE_IMPLICIT_TYPE, "Cannot determine implicit type; no common type given" },
    { MSG_MISMATCHED_TYPES, "Mismatched types '%' and '%'" },
    { MSG_MISMATCHED_TYPES_ASSIGNMENT, "Cannot assign % to %" },
    { MSG_IMPLICIT_ANY_MISMATCH, "An explicit cast to '%' is required" },
    { MSG_TYPE_NOT_GENERIC, "Type '%' is not generic" },
    { MSG_GENERIC_PARAMETERS_MISSING, "Generic type '%' requires % parameter(s)" },
    { MSG_GENERIC_PARAMETER_REDECLARED, "Generic parameter '%' already declared" },
    { MSG_GENERIC_EXPRESSION_MUST_BE_CONST, "Generic '%' must be const" },
    { MSG_GENERIC_EXPRESSION_NO_ARGUMENTS_PROVIDED, "'%' is generic, which requires argument(s) provided within <> (may also be empty)" },
    { MSG_GENERIC_EXPRESSION_INVALID_ARGUMENTS, "Generic expression requires arguments: '%'" },
    { MSG_GENERIC_EXPRESSION_REQUIRES_ASSIGNMENT, "'%' is missing assignment (all generics must have a value)" },
    { MSG_GENERIC_ARGUMENT_MUST_BE_LITERAL, "Generic argument is not resolvable at compile-time" },
    { MSG_NOT_A_DATA_MEMBER, "'%' not found in %" },
    { MSG_NOT_A_CONSTANT_TYPE, "% is not a constant. An exception will be thrown at runtime if this object is not a class." },
    { MSG_TYPE_MISSING_PROTOTYPE, "Type % is missing '$proto' member." },
    { MSG_CANNOT_INLINE_VARIABLE, "Unable to inline variable which is marked as force inline" },
    { MSG_BITWISE_OPERANDS_MUST_BE_INT, "Bitwise operands must both be 'int', got '%' and '%'" },
    { MSG_BITWISE_OPERAND_MUST_BE_INT, "Bitwise operand must be 'int', got '%'" },
    { MSG_ARITHMETIC_OPERANDS_MUST_BE_NUMBERS, "Operands of arithmetic operator '%', % and %, are not numeric and no overload was found" },
    { MSG_ARITHMETIC_OPERAND_MUST_BE_NUMBERS, "Operand of arithmetic operator '%', % is not numeric and no overload was found" },
    { MSG_EXPECTED_TOKEN, "Expected '%'" },
    { MSG_UNKNOWN_DIRECTIVE, "Unknown directive '%'" },
    { MSG_UNKNOWN_MODULE, "'%' is not an imported module" },
    { MSG_EXPECTED_MODULE, "Statement found outside of module" },
    { MSG_EMPTY_MODULE, "The module is empty" },
    { MSG_MODULE_ALREADY_DEFINED, "Module '%' was already defined or imported" },
    { MSG_MODULE_NOT_IMPORTED, "Module '%' was not imported" },
    { MSG_IDENTIFIER_IS_MODULE, "'%' is the name of a module, expected an identifier" },
    { MSG_INVALID_MODULE_ACCESS, "'%' is a module, expected an identifier or function call" },
    { MSG_STATEMENT_OUTSIDE_MODULE, "Statement outside of module" },
    { MSG_MODULE_DECLARED_IN_BLOCK, "A module may not be declared within a conditional, loop or function" },
    { MSG_COULD_NOT_OPEN_FILE, "Could not open file '%'" },
    { MSG_COULD_NOT_FIND_MODULE, "Could not find module '%' in paths %" },
    { MSG_COULD_NOT_FIND_NESTED_MODULE, "Could not find nested module or identifier '%' in module '%'" },
    { MSG_IMPORT_OUTSIDE_GLOBAL, "Import statement must be in module or global scope" },
    { MSG_IMPORT_CURRENT_FILE, "Attempt to import current file" },
    { MSG_EXPORT_OUTSIDE_GLOBAL, "Export statement must be in module or global scope" },
    { MSG_EXPORT_INVALID_NAME, "Export is not valid, statement does not have a name" },
    { MSG_EXPORT_DUPLICATE, "Export is not valid, identifier '%' has already been exported" },
    { MSG_SELF_OUTSIDE_CLASS, "'self' not allowed outside of a class" },
    { MSG_ELSE_OUTSIDE_IF, "'else' not connected to an if statement" },
    { MSG_PROXY_CLASS_CANNOT_BE_CONSTRUCTED, "A proxy class may not be constructed" },
    { MSG_PROXY_CLASS_MAY_ONLY_CONTAIN_METHODS, "A proxy class may only contain methods" },
    { MSG_ALIAS_MISSING_ASSIGNMENT, "Alias '%' must have an assignment" },
    { MSG_ALIAS_MUST_BE_IDENTIFIER, "Alias '%' must reference an identifier" },
    { MSG_UNRECOGNIZED_ALIAS_TYPE, "Only identifiers, types and module names may be aliased" },
    { MSG_TYPE_CONTRACT_OUTSIDE_DEFINITION, "Type contracts not allowed outside of function definitions" },
    { MSG_UNKNOWN_TYPE_CONTRACT_REQUIREMENT, "Unknown type contract requirement: '%'" },
    { MSG_INVALID_TYPE_CONTRACT_OPERATOR, "Invalid type contract operator '%'. Supported operators are '|' and '&'" },
    { MSG_UNSATISFIED_TYPE_CONTRACT, "Type '%' does not satisfy type contract" },
    { MSG_UNSUPPORTED_FEATURE, "Unsupported feature" },

    /* Warnings */
    { MSG_UNREACHABLE_CODE, "Unreachable code detected" },
    { MSG_EXPECTED_END_OF_STATEMENT, "End of statement expected (use a newline or semicolon to end a statement)" },

    /* Info */
    { MSG_UNUSED_IDENTIFIER, "'%' is not used" },
    { MSG_EMPTY_FUNCTION_BODY, "The function body of '%' is empty" },
    { MSG_EMPTY_STATEMENT_BODY, "Loop or statement body is empty" },
    { MSG_MODULE_NAME_BEGINS_LOWERCASE, "Module name '%' should begin with an uppercase character" },
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

} // namespace hyperion::json
