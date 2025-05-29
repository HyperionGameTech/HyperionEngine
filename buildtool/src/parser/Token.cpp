/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <parser/Token.hpp>

#include <cstring>

namespace hyperion::buildtool {

const Token Token::EMPTY = Token(TK_EMPTY, "", SourceLocation::eof);

String Token::TokenTypeToString(TokenClass token_class)
{
    switch (token_class)
    {
    case TK_INTEGER:
        return "integer";
    case TK_FLOAT:
        return "float";
    case TK_STRING:
        return "string";
    case TK_IDENT:
        return "identifier";
    case TK_LABEL:
        return "label";
    case TK_OPERATOR:
        return "operator";
    case TK_COMMA:
        return ",";
    case TK_SEMICOLON:
        return ";";
    case TK_COLON:
        return ":";
    case TK_DOUBLE_COLON:
        return "::";
    case TK_QUESTION_MARK:
        return "?";
    case TK_DOT:
        return ".";
    case TK_ELLIPSIS:
        return "...";
    case TK_RIGHT_ARROW:
        return "->";
    case TK_OPEN_PARENTH:
        return "(";
    case TK_CLOSE_PARENTH:
        return ")";
    case TK_OPEN_BRACKET:
        return "[";
    case TK_CLOSE_BRACKET:
        return "]";
    case TK_OPEN_BRACE:
        return "{";
    case TK_CLOSE_BRACE:
        return "}";
    default:
        return "??";
    }
}

Token::Token(TokenClass token_class, const String& value, const SourceLocation& location)
    : m_token_class(token_class),
      m_value(value),
      m_location(location)
{
    std::memset(m_flags, 0, sizeof(m_flags));
}

Token::Token(TokenClass token_class, const String& value, Flags flags, const SourceLocation& location)
    : m_token_class(token_class),
      m_value(value),
      m_location(location)
{
    std::memcpy(m_flags, flags, sizeof(m_flags));
}

Token::Token(const Token& other)
    : m_token_class(other.m_token_class),
      m_value(other.m_value),
      m_location(other.m_location)
{
    std::memcpy(m_flags, other.m_flags, sizeof(m_flags));
}

Token& Token::operator=(const Token& other)
{
    if (this != &other)
    {
        m_token_class = other.m_token_class;
        m_value = other.m_value;
        m_location = other.m_location;

        std::memcpy(m_flags, other.m_flags, sizeof(m_flags));
    }

    return *this;
}

bool Token::IsContinuationToken() const
{
    return m_token_class == TK_COMMA || m_token_class == TK_COLON || m_token_class == TK_DOT || m_token_class == TK_OPEN_PARENTH || m_token_class == TK_OPEN_BRACKET || m_token_class == TK_OPEN_BRACE;
}

} // namespace hyperion::buildtool
