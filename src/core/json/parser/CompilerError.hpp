/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYP_JSON_COMPILER_ERROR_HPP
#define HYP_JSON_COMPILER_ERROR_HPP

#include <core/json/parser/SourceLocation.hpp>
#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <string>
#include <sstream>
#include <map>

namespace hyperion::json {

enum ErrorLevel
{
    LEVEL_INFO,
    LEVEL_WARN,
    LEVEL_ERROR
};

enum ErrorMessage
{
    /* Fatal errors */
    MSG_INTERNAL_ERROR,
    MSG_CUSTOM_ERROR,
    MSG_NOT_IMPLEMENTED,
    MSG_ILLEGAL_SYNTAX,
    MSG_ILLEGAL_EXPRESSION,
    MSG_ILLEGAL_OPERATOR,
    MSG_INVALID_OPERATOR_FOR_TYPE,
    MSG_CANNOT_OVERLOAD_OPERATOR,
    MSG_INVALID_SYMBOL_QUERY,
    MSG_EXPRESSION_CANNOT_BE_MODIFIED,
    MSG_CONST_MISSING_ASSIGNMENT,
    MSG_REF_MISSING_ASSIGNMENT,
    MSG_CANNOT_CREATE_REFERENCE,
    MSG_CONST_ASSIGNED_TO_NON_CONST_REF,
    MSG_CANNOT_MODIFY_RVALUE,
    MSG_PROHIBITED_ACTION_ATTRIBUTE,
    MSG_UNBALANCED_EXPRESSION,
    MSG_UNMATCHED_PARENTHESES,
    MSG_UNEXPECTED_CHARACTER,
    MSG_UNEXPECTED_IDENTIFIER,
    MSG_UNEXPECTED_TOKEN,
    MSG_UNEXPECTED_EOF,
    MSG_UNEXPECTED_EOL,
    MSG_UNRECOGNIZED_ESCAPE_SEQUENCE,
    MSG_UNTERMINATED_STRING_LITERAL,
    MSG_ARGUMENT_AFTER_VARARGS,
    MSG_INCORRECT_NUMBER_OF_ARGUMENTS,
    MSG_MAXIMUM_NUMBER_OF_ARGUMENTS,
    MSG_ARG_TYPE_INCOMPATIBLE,
    MSG_INCOMPATIBLE_CAST,
    MSG_NAMED_ARG_NOT_FOUND,
    MSG_REDECLARED_IDENTIFIER,
    MSG_REDECLARED_IDENTIFIER_TYPE,
    MSG_UNDECLARED_IDENTIFIER,
    MSG_EXPECTED_IDENTIFIER,
    MSG_KEYWORD_CANNOT_BE_USED_AS_IDENTIFIER,
    MSG_AMBIGUOUS_IDENTIFIER,
    MSG_INVALID_CONSTRUCTOR,
    MSG_RETURN_INVALID_IN_CONSTRUCTOR,
    MSG_RETURN_TYPE_SPECIFICATION_INVALID_ON_CONSTRUCTOR,
    MSG_EXPECTED_TYPE_GOT_IDENTIFIER,
    MSG_MISSING_TYPE_AND_ASSIGNMENT,
    MSG_TYPE_NO_DEFAULT_ASSIGNMENT,
    MSG_COULD_NOT_DEDUCE_TYPE_FOR_EXPRESSION,
    MSG_EXPRESSION_NOT_GENERIC,
    MSG_TOO_MANY_GENERIC_ARGS,
    MSG_TOO_FEW_GENERIC_ARGS,
    MSG_NO_SUBSTITUTION_FOR_GENERIC_ARG,
    MSG_ENUM_ASSIGNMENT_NOT_CONSTANT,
    MSG_GENERIC_ARG_MAY_NOT_HAVE_SIDE_EFFECTS,

    /* LOOPS */
    MSG_BREAK_OUTSIDE_LOOP,
    MSG_CONTINUE_OUTSIDE_LOOP,

    /* FUNCTIONS */
    MSG_MULTIPLE_RETURN_TYPES,
    MSG_MISMATCHED_RETURN_TYPE,
    MSG_MUST_BE_EXPLICITLY_MARKED_ANY,
    MSG_ANY_RESERVED_FOR_PARAMETERS,
    MSG_RETURN_OUTSIDE_FUNCTION,
    MSG_YIELD_OUTSIDE_FUNCTION,
    MSG_YIELD_OUTSIDE_GENERATOR_FUNCTION,
    MSG_NOT_A_FUNCTION,
    MSG_MEMBER_NOT_A_METHOD,
    MSG_CLOSURE_CAPTURE_MUST_BE_PARAMETER,
    MSG_PURE_FUNCTION_SCOPE,

    /* ARRAYS */
    MSG_INVALID_SUBSCRIPT,

    /* TYPES */
    MSG_NOT_A_TYPE,
    MSG_UNDEFINED_TYPE,
    MSG_REDEFINED_TYPE,
    MSG_REDEFINED_BUILTIN_TYPE,
    MSG_TYPE_NOT_DEFINED_GLOBALLY,
    MSG_IDENTIFIER_IS_TYPE,
    MSG_CANNOT_DETERMINE_IMPLICIT_TYPE,
    MSG_MISMATCHED_TYPES,
    MSG_MISMATCHED_TYPES_ASSIGNMENT,
    MSG_IMPLICIT_ANY_MISMATCH,
    MSG_TYPE_NOT_GENERIC,
    MSG_GENERIC_PARAMETERS_MISSING,
    MSG_GENERIC_PARAMETER_REDECLARED,
    MSG_GENERIC_EXPRESSION_NO_ARGUMENTS_PROVIDED,
    MSG_GENERIC_EXPRESSION_MUST_BE_CONST,
    MSG_GENERIC_EXPRESSION_INVALID_ARGUMENTS,
    MSG_GENERIC_EXPRESSION_REQUIRES_ASSIGNMENT,
    MSG_GENERIC_ARGUMENT_MUST_BE_LITERAL,
    MSG_NOT_A_DATA_MEMBER,
    MSG_NOT_A_CONSTANT_TYPE,
    MSG_TYPE_MISSING_PROTOTYPE,
    MSG_CANNOT_INLINE_VARIABLE,

    MSG_BITWISE_OPERANDS_MUST_BE_INT,
    MSG_BITWISE_OPERAND_MUST_BE_INT,
    MSG_ARITHMETIC_OPERANDS_MUST_BE_NUMBERS,
    MSG_ARITHMETIC_OPERAND_MUST_BE_NUMBERS,
    MSG_EXPECTED_TOKEN,
    MSG_UNKNOWN_DIRECTIVE,
    MSG_UNKNOWN_MODULE,
    MSG_EXPECTED_MODULE,
    MSG_EMPTY_MODULE,
    MSG_MODULE_ALREADY_DEFINED,
    MSG_MODULE_NOT_IMPORTED,
    MSG_INVALID_MODULE_ACCESS,
    MSG_STATEMENT_OUTSIDE_MODULE,
    MSG_MODULE_DECLARED_IN_BLOCK,
    MSG_COULD_NOT_OPEN_FILE,
    MSG_COULD_NOT_FIND_MODULE,
    MSG_COULD_NOT_FIND_NESTED_MODULE,
    MSG_IDENTIFIER_IS_MODULE,
    MSG_IMPORT_OUTSIDE_GLOBAL,
    MSG_IMPORT_CURRENT_FILE,
    MSG_EXPORT_OUTSIDE_GLOBAL,
    MSG_EXPORT_INVALID_NAME,
    MSG_EXPORT_DUPLICATE,
    MSG_SELF_OUTSIDE_CLASS,
    MSG_ELSE_OUTSIDE_IF,
    MSG_PROXY_CLASS_CANNOT_BE_CONSTRUCTED,
    MSG_PROXY_CLASS_MAY_ONLY_CONTAIN_METHODS,
    MSG_ALIAS_MISSING_ASSIGNMENT,
    MSG_ALIAS_MUST_BE_IDENTIFIER,
    MSG_UNRECOGNIZED_ALIAS_TYPE,
    MSG_TYPE_CONTRACT_OUTSIDE_DEFINITION,
    MSG_UNKNOWN_TYPE_CONTRACT_REQUIREMENT,
    MSG_INVALID_TYPE_CONTRACT_OPERATOR,
    MSG_UNSATISFIED_TYPE_CONTRACT,
    MSG_UNSUPPORTED_FEATURE,

    MSG_UNREACHABLE_CODE,
    MSG_EXPECTED_END_OF_STATEMENT,

    /* Info */
    MSG_UNUSED_IDENTIFIER,
    MSG_EMPTY_FUNCTION_BODY,
    MSG_EMPTY_STATEMENT_BODY,
    MSG_MODULE_NAME_BEGINS_LOWERCASE,
};

class CompilerError
{
    static const HashMap<ErrorMessage, String> error_message_strings;

public:
    template <typename... Args>
    CompilerError(
        ErrorLevel level, ErrorMessage msg,
        const SourceLocation& location,
        const Args&... args)
        : m_level(level),
          m_msg(msg),
          m_location(location)
    {
        String msg_str = error_message_strings.At(m_msg);
        MakeMessage(msg_str.Data(), args...);
    }

    CompilerError(const CompilerError& other);
    ~CompilerError() = default;

    HYP_NODISCARD HYP_FORCE_INLINE ErrorLevel GetLevel() const
    {
        return m_level;
    }

    HYP_NODISCARD HYP_FORCE_INLINE ErrorMessage GetMessage() const
    {
        return m_msg;
    }

    HYP_NODISCARD HYP_FORCE_INLINE const SourceLocation& GetLocation() const
    {
        return m_location;
    }

    HYP_NODISCARD HYP_FORCE_INLINE const String& GetText() const
    {
        return m_text;
    }

    HYP_NODISCARD HYP_FORCE_INLINE bool operator==(const CompilerError& other) const
    {
        return m_level == other.m_level
            && m_msg == other.m_msg
            && m_location == other.m_location
            && m_text == other.m_text;
    }

    HYP_NODISCARD HYP_FORCE_INLINE bool operator!=(const CompilerError& other) const
    {
        return !(*this == other);
    }

    bool operator<(const CompilerError& other) const;

private:
    void MakeMessage(const char* format)
    {
        m_text += format;
    }

    template <typename T, typename... Args>
    void MakeMessage(const char* format, const T& value, Args&&... args)
    {
        for (; *format; format++)
        {
            if (*format == '%')
            {
                std::stringstream sstream;
                sstream << value;
                m_text += sstream.str().c_str();
                MakeMessage(format + 1, args...);
                return;
            }

            m_text += *format;
        }
    }

    ErrorLevel m_level;
    ErrorMessage m_msg;
    SourceLocation m_location;
    String m_text;
};

} // namespace hyperion::json

#endif
