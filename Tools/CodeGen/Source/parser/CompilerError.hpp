/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifndef HYPERION_CODEGEN_COMPILER_ERROR_HPP
#define HYPERION_CODEGEN_COMPILER_ERROR_HPP

#include <parser/SourceLocation.hpp>

#include <Core/containers/String.hpp>
#include <Core/containers/HashMap.hpp>

#include <Core/utilities/Format.hpp>

namespace Hyperion::CodeGen {

enum ErrorLevel
{
    LEVEL_INFO,
    LEVEL_WARN,
    LEVEL_ERROR
};

enum ErrorMessage
{
    Msg_unexpected_eof,
    Msg_unexpected_token,
    Msg_unexpected_character,
    Msg_unterminated_string_literal,
    Msg_unrecognized_escape_sequence,
    Msg_cannot_overload_operator,
    Msg_invalid_numeric_literal,
    Msg_expected_token,
    Msg_expected_identifier,
    Msg_expected_end_of_statement,
    Msg_illegal_operator,
    Msg_illegal_expression,
    Msg_internal_error
};

class CompilerError
{
    static const HashMap<ErrorMessage, String> errorMessageStrings;

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
        String msgStr = errorMessageStrings.At(m_msg);
        MakeMessage(msgStr.Data(), args...);
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
                m_text += HYP_FORMAT("{}", value);
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

} // namespace Hyperion::CodeGen

#endif
