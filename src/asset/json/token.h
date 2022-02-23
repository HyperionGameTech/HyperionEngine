#ifndef HYPERION_JSON_TOKEN_H
#define HYPERION_JSON_TOKEN_H

#include "source_location.h"

#include <string>

namespace hyperion {
namespace json {
enum TokenClass {
    TK_EMPTY,
    TK_INTEGER,
    TK_FLOAT,
    TK_STRING,
    TK_IDENT,
    TK_KEYWORD,
    TK_OPERATOR,
    TK_DIRECTIVE,
    TK_NEWLINE,
    TK_COMMA,
    TK_SEMICOLON,
    TK_COLON,
    TK_DOUBLE_COLON,
    TK_DEFINE,
    TK_QUESTION_MARK,
    TK_DOT,
    TK_ELLIPSIS,
    TK_LEFT_ARROW,
    TK_RIGHT_ARROW,
    TK_FAT_ARROW,
    TK_OPEN_PARENTH,
    TK_CLOSE_PARENTH,
    TK_OPEN_BRACKET,
    TK_CLOSE_BRACKET,
    TK_OPEN_BRACE,
    TK_CLOSE_BRACE
};

class Token {
public:
    static std::string TokenTypeToString(TokenClass token_class);

    static const Token EMPTY;

public:
    Token(TokenClass token_class,
        const std::string &value,
        const SourceLocation &location);
    Token(const Token &other);

    inline TokenClass GetTokenClass() const { return m_token_class; }
    inline const std::string &GetValue() const { return m_value; }
    inline const SourceLocation &GetLocation() const { return m_location; }
    inline bool Empty() const { return m_token_class == TK_EMPTY; }

    inline Token &operator=(const Token &other)
    {
        m_token_class = other.m_token_class;
        m_value = other.m_value;
        m_location = other.m_location;

        return *this;
    }

    // return true if not empty
    inline explicit operator bool() const { return m_token_class != TK_EMPTY; }

    bool IsContinuationToken() const;

private:
    TokenClass m_token_class;
    std::string m_value;
    SourceLocation m_location;
};

} // namespace json
} // namespace hyperion

#endif