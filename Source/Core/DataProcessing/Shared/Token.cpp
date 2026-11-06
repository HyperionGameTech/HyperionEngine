#include <Core/DataProcessing/Shared/Token.hpp>
#include <cstring>

namespace Hyperion::DataProcessing {

const Token Token::empty = Token();

Token::Token()
    : m_tokenClass(TK_EMPTY),
      m_value(),
      m_flags {},
      m_location(SourceLocation::eof)
{
}

String Token::TokenTypeToString(TokenClass tokenClass)
{
    switch (tokenClass)
    {
    case TK_INTEGER:          return "integer";
    case TK_FLOAT:            return "float";
    case TK_STRING:           return "string";
    case TK_AT_STRING:        return "@string";
    case TK_IDENT:            return "identifier";
    case TK_NEWLINE:          return "newline";
    case TK_COMMA:            return ",";
    case TK_SEMICOLON:        return ";";
    case TK_COLON:            return ":";
    case TK_DOT:              return ".";
    case TK_EQUALS:           return "=";
    case TK_PIPE:             return "|";
    case TK_OPEN_PARENTH:     return "(";
    case TK_CLOSE_PARENTH:    return ")";
    case TK_OPEN_BRACKET:     return "[";
    case TK_CLOSE_BRACKET:    return "]";
    case TK_OPEN_BRACE:       return "{";
    case TK_CLOSE_BRACE:      return "}";
    case TK_EMPTY:            return "<empty>";
    default:                  return "??";
    }
}

Token::Token(TokenClass tokenClass, UTF8StringView value, const SourceLocation& location)
    : m_tokenClass(tokenClass),
      m_value(value),
      m_flags {},
      m_location(location)
{
}

Token::Token(TokenClass tokenClass, UTF8StringView value, const Flags flags, const SourceLocation& location)
    : m_tokenClass(tokenClass),
      m_value(value),
      m_location(location)
{
    std::memcpy(m_flags, flags, sizeof(m_flags));
}

bool Token::IsContinuationToken() const
{
    return m_tokenClass == TK_COMMA || m_tokenClass == TK_COLON || m_tokenClass == TK_DOT
        || m_tokenClass == TK_EQUALS || m_tokenClass == TK_PIPE
        || m_tokenClass == TK_OPEN_PARENTH || m_tokenClass == TK_OPEN_BRACKET || m_tokenClass == TK_OPEN_BRACE;
}

} // namespace Hyperion::DataProcessing
