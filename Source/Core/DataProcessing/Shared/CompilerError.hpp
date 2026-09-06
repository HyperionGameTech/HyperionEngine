#pragma once

#include <Core/DataProcessing/Shared/SourceLocation.hpp>

#include <Core/Containers/String.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Utilities/Format.hpp>

namespace Hyperion::DataProcessing {

enum class ErrorLevel : uint8
{
    Error = 0,
    Warning,
    Diagnostic
};

enum ErrorMessage : uint8
{
    /* Generic / lexical errors (shared between all parsers) */
    MSG_INTERNAL_ERROR,
    MSG_CUSTOM_ERROR,
    MSG_NOT_IMPLEMENTED,
    MSG_ILLEGAL_SYNTAX,
    MSG_ILLEGAL_EXPRESSION,
    MSG_ILLEGAL_OPERATOR,
    MSG_UNEXPECTED_CHARACTER,
    MSG_UNEXPECTED_IDENTIFIER,
    MSG_UNEXPECTED_TOKEN,
    MSG_UNEXPECTED_EOF,
    MSG_UNEXPECTED_EOL,
    MSG_UNRECOGNIZED_ESCAPE_SEQUENCE,
    MSG_UNTERMINATED_STRING_LITERAL,
    MSG_EXPECTED_IDENTIFIER,
    MSG_EXPECTED_TOKEN,

    /* Expression / type errors (from JSON/lang parser) */
    MSG_INVALID_OPERATOR_FOR_TYPE,
    MSG_CANNOT_OVERLOAD_OPERATOR,
    MSG_CONST_MISSING_ASSIGNMENT,
    MSG_REF_MISSING_ASSIGNMENT,
    MSG_CANNOT_CREATE_REFERENCE,
    MSG_CONST_ASSIGNED_TO_NON_CONST_REF,
    MSG_CANNOT_MODIFY_RVALUE,
    MSG_PROHIBITED_ACTION_ATTRIBUTE,
    MSG_UNBALANCED_EXPRESSION,
    MSG_UNMATCHED_PARENTHESES,
    MSG_ARGUMENT_AFTER_VARARGS,
    MSG_INCORRECT_NUMBER_OF_ARGUMENTS,
    MSG_MAXIMUM_NUMBER_OF_ARGUMENTS,
    MSG_ARG_TYPE_INCOMPATIBLE,
    MSG_NAMED_ARG_NOT_FOUND,
    MSG_REDECLARED_IDENTIFIER,
    MSG_REDECLARED_IDENTIFIER_TYPE,
    MSG_UNDECLARED_IDENTIFIER,

    /* HMF-specific semantic errors */
    MSG_UNKNOWN_FIELD,
    MSG_CANNOT_ASSIGN_PROPERTY,
    MSG_UNRESOLVED_ENUM_NAME,
    MSG_TYPE_MISMATCH,
    MSG_CLASS_NOT_FOUND,
    MSG_CLASS_NOT_DERIVED,
    MSG_NOT_AN_ENUM_FLAGS_TYPE,
    MSG_NOT_AN_ENUM_TYPE,
    MSG_UNKNOWN_VARIANT_TAG,
    MSG_INVALID_LITERAL_FOR_TYPE,
    MSG_UNBALANCED_BRACES,
    MSG_UNBALANCED_BRACKETS,
    MSG_UNKNOWN_ASSET_PATH,
    MSG_UNKNOWN_OVERRIDE_SECTION,
    MSG_OVERRIDE_SECTION_PARSE_FAILED
};

class CORE_API CompilerError
{
    static const Map<ErrorMessage, String> s_errorMessageStrings;

public:
    using Level = ErrorLevel;

    template <typename... Args>
    CompilerError(ErrorLevel level, ErrorMessage msg, const SourceLocation& location, const Args&... args)
        : m_level(level),
          m_msg(msg),
          m_location(location)
    {
        const String& msgStr = s_errorMessageStrings.At(m_msg);
        MakeMessage(msgStr.Data(), args...);
    }

    CompilerError(const CompilerError& other) = default;
    CompilerError& operator=(const CompilerError& other) = default;

    ~CompilerError() = default;

    HYP_NODISCARD HYP_FORCE_INLINE ErrorLevel GetLevel() const { return m_level; }
    HYP_NODISCARD HYP_FORCE_INLINE ErrorMessage GetMessage() const { return m_msg; }
    HYP_NODISCARD HYP_FORCE_INLINE const SourceLocation& GetLocation() const { return m_location; }
    HYP_NODISCARD HYP_FORCE_INLINE const String& GetText() const { return m_text; }

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
                m_text += HYP_FORMAT("{}", value);
                MakeMessage(format + 1, std::forward<Args>(args)...);
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

} // namespace Hyperion::DataProcessing
