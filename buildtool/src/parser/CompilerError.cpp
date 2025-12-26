/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <parser/CompilerError.hpp>

namespace Hyperion::CodeGen {

const HashMap<ErrorMessage, String> CompilerError::errorMessageStrings {
    { Msg_unexpected_eof, "Unexpected end of file." },
    { Msg_unexpected_token, "Unexpected token." },
    { Msg_unexpected_character, "Unexpected character." },
    { Msg_unterminated_string_literal, "Unterminated string literal." },
    { Msg_unrecognized_escape_sequence, "Unrecognized escape sequence: %." },
    { Msg_cannot_overload_operator, "Cannot overload operator: %." },
    { Msg_invalid_numeric_literal, "Invalid numeric literal: %." },
    { Msg_expected_token, "Expected token: %." },
    { Msg_expected_identifier, "Expected identifier." },
    { Msg_expected_end_of_statement, "Expected end of statement." },
    { Msg_illegal_operator, "Illegal operator used: %." },
    { Msg_illegal_expression, "Illegal expression." },
    { Msg_internal_error, "Internal compiler error." }
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

} // namespace Hyperion::CodeGen
