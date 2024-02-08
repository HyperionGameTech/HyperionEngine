#ifndef HYP_JSON_TOKEN_HPP
#define HYP_JSON_TOKEN_HPP

#include <script/SourceLocation.hpp>
#include <core/lib/String.hpp>

#include <string>

namespace hyperion::json {

enum TokenClass
{
    TK_EMPTY,
    TK_INTEGER,
    TK_FLOAT,
    TK_STRING,
    TK_IDENT,
    TK_NEWLINE,
    TK_COMMA,
    TK_SEMICOLON,
    TK_COLON,
    TK_DOT,
    TK_OPEN_PARENTH,
    TK_CLOSE_PARENTH,
    TK_OPEN_BRACKET,
    TK_CLOSE_BRACKET,
    TK_OPEN_BRACE,
    TK_CLOSE_BRACE
};

class Token
{
public:
    static String TokenTypeToString(TokenClass token_class);
    
    static const Token EMPTY;

public:
    using Flags = char[4];

    Token(
        TokenClass token_class,
        const String &value,
        const SourceLocation &location
    );

    Token(
        TokenClass token_class,
        const String &value,
        Flags flags,
        const SourceLocation &location
    );

    Token(const Token &other);

    TokenClass GetTokenClass() const { return m_token_class; }
    const String &GetValue() const { return m_value; }
    const Flags &GetFlags() const { return m_flags; }
    const SourceLocation &GetLocation() const { return m_location; }
    bool Empty() const { return m_token_class == TK_EMPTY; }
    
    Token &operator=(const Token &other)
    {
        m_token_class = other.m_token_class;
        m_value = other.m_value;
        m_location = other.m_location;
        
        return *this;
    }

    // return true if not empty
    explicit operator bool() const { return m_token_class != TK_EMPTY; }

    bool IsContinuationToken() const;

private:
    TokenClass m_token_class;
    String m_value;
    Flags m_flags;
    SourceLocation m_location;
};

} // namespace hyperion::json

#endif
