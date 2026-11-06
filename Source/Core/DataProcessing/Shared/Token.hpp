#pragma once

#include <Core/DataProcessing/Shared/SourceLocation.hpp>
#include <Core/Containers/String.hpp>
#include <Core/Utilities/StringView.hpp>

namespace Hyperion::DataProcessing {

enum TokenClass
{
    TK_EMPTY,
    TK_INTEGER,
    TK_FLOAT,
    TK_STRING,
    TK_AT_STRING,
    TK_IDENT,
    TK_NEWLINE,
    TK_COMMA,
    TK_SEMICOLON,
    TK_COLON,
    TK_DOT,
    TK_EQUALS,
    TK_PIPE,
    TK_OPEN_PARENTH,
    TK_CLOSE_PARENTH,
    TK_OPEN_BRACKET,
    TK_CLOSE_BRACKET,
    TK_OPEN_BRACE,
    TK_CLOSE_BRACE
};

class CORE_API Token
{
public:
    static String TokenTypeToString(TokenClass tokenClass);
    static const Token empty;

    using Flags = char[4];

    Token();
    Token(TokenClass tokenClass, UTF8StringView value, const SourceLocation& location);
    Token(TokenClass tokenClass, UTF8StringView value, const Flags flags, const SourceLocation& location);

    Token(const Token& other) = default;
    Token& operator=(const Token& other) = default;

    TokenClass GetTokenClass() const { return m_tokenClass; }
    UTF8StringView GetValue() const { return m_value; }
    const Flags& GetFlags() const { return m_flags; }
    const SourceLocation& GetLocation() const { return m_location; }

    bool Empty() const { return m_tokenClass == TK_EMPTY; }

    explicit operator bool() const { return m_tokenClass != TK_EMPTY; }

    bool IsContinuationToken() const;

private:
    TokenClass m_tokenClass;
    UTF8StringView m_value;
    Flags m_flags;
    SourceLocation m_location;
};

} // namespace Hyperion::DataProcessing
