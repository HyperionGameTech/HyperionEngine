#include "parser.h"

#include <memory>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <iostream>

namespace hyperion {
namespace json {

Parser::Parser(TokenStream *token_stream,
    State *state)
    : m_token_stream(token_stream),
      m_state(state)
{
}

Parser::Parser(const Parser &other)
    : m_token_stream(other.m_token_stream),
      m_state(other.m_state)
{
}

Token Parser::Match(TokenClass token_class, bool read)
{
    Token peek = m_token_stream->Peek();

    if (peek && peek.GetTokenClass() == token_class) {
        if (read && m_token_stream->HasNext()) {
            m_token_stream->Next();
        }

        return peek;
    }

    return Token::EMPTY;
}

Token Parser::MatchAhead(TokenClass token_class, int n)
{
    Token peek = m_token_stream->Peek(n);

    if (peek && peek.GetTokenClass() == token_class) {
        return peek;
    }

    return Token::EMPTY;
}

Token Parser::Expect(TokenClass token_class, bool read)
{
    Token token = Match(token_class, read);

    if (!token) {
        const SourceLocation location = CurrentLocation();

        m_state->AddError(Error(
            "Expected " + Token::TokenTypeToString(token_class)
        ));
    }

    return token;
}

bool Parser::ExpectEndOfStmt()
{
    const SourceLocation location = CurrentLocation();

    if (!Match(TK_NEWLINE, true) && !Match(TK_SEMICOLON, true)) {
        m_state->AddError(Error(
            "Expected end of statement"
        ));

        // skip until end of statement, end of line, or end of file.
        do {
            m_token_stream->Next();
        } while (m_token_stream->HasNext() &&
            !Match(TK_NEWLINE, true) &&
            !Match(TK_SEMICOLON, true));

        return false;
    }

    return true;
}

SourceLocation Parser::CurrentLocation() const
{
    if (m_token_stream->GetSize() != 0 && !m_token_stream->HasNext()) {
        return m_token_stream->Last().GetLocation();
    }

    return m_token_stream->Peek().GetLocation();
}

void Parser::SkipStatementTerminators()
{
    // read past statement terminator tokens
    while (Match(TK_SEMICOLON, true) || Match(TK_NEWLINE, true));
}

JSONValue Parser::Parse()
{
    SkipStatementTerminators();

    return ParseExpression();
}

JSONValue Parser::ParseExpression()
{
    return ParseTerm();
}

JSONValue Parser::ParseTerm()
{
    Token token = m_token_stream->Peek();

    JSONValue term;

    if (!token) {
        m_state->AddError(Error(
            "Unexpected end of file"
        ));

        if (m_token_stream->HasNext()) {
            m_token_stream->Next();
        }

        return term;
    }

    if (Match(TK_OPEN_BRACKET)) {
        auto expr = ParseArray();
        term.value = expr;
        term.type = JSONValue::JSON_ARRAY;
    } else if (Match(TK_OPEN_BRACE)) {
        auto expr = ParseObject();
        term.value = expr;
        term.type = JSONValue::JSON_OBJECT;
    } else if (Match(TK_INTEGER)) {
        auto expr = ParseIntegerLiteral();
        term.value = expr;
        term.type = JSONValue::JSON_NUMBER;
    } else if (Match(TK_FLOAT)) {
        auto expr = ParseFloatLiteral();
        term.value = expr;
        term.type = JSONValue::JSON_NUMBER;
    } else if (Match(TK_STRING)) {
        auto expr = ParseStringLiteral();
        term.value = expr;
        term.type = JSONValue::JSON_STRING;
    } else if (Match(TK_IDENT)) {
        auto token = m_token_stream->Peek();

        if (token.GetValue() == "true") {
            auto expr = JSONBoolean{true};
            term.value = expr;
            term.type = JSONValue::JSON_BOOLEAN;

            m_token_stream->Next();
        } else if (token.GetValue() == "false") {
            auto expr = JSONBoolean{false};
            term.value = expr;
            term.type = JSONValue::JSON_BOOLEAN;

            m_token_stream->Next();
        } else if (token.GetValue() == "null") {
            auto expr = JSONNull{};
            term.value = expr;
            term.type = JSONValue::JSON_NULL;

            m_token_stream->Next();
        }
        // TODO
    } else {
        if (token.GetTokenClass() == TK_NEWLINE) {
            m_state->AddError(Error(
                "Unexpected end of line"
            ));
        } else {
            m_state->AddError(Error(
                "Unexpected token " + token.GetValue()
            ));
        }

        if (m_token_stream->HasNext()) {
            m_token_stream->Next();
        }
    }

    return term;
}

JSONString Parser::ParseStringLiteral()
{
    if (Token token = Expect(TK_STRING, true)) {
        return JSONString{token.GetValue()};
    }

    return JSONString{""};
}


JSONNumber Parser::ParseIntegerLiteral()
{
    if (Token token = Expect(TK_INTEGER, true)) {
        std::istringstream ss(token.GetValue());
        int64_t value;
        ss >> value;

        return JSONNumber{value};
    }

    return JSONNumber{int64_t(0)};
}

JSONNumber Parser::ParseFloatLiteral()
{
    if (Token token = Expect(TK_FLOAT, true)) {
        std::istringstream ss(token.GetValue());
        double value;
        ss >> value;

        return JSONNumber{value};
    }

    return JSONNumber{0.0};
}

JSONObject Parser::ParseObject()
{
    JSONObject object;

    if (!Expect(TK_OPEN_BRACE, true)) {
        m_state->AddError(Error("Expected opening brace"));

        return object;
    }

    Token token = Token::EMPTY;

    while ((token = Match(TK_STRING, true))) {
        if (!Expect(TK_COLON, true)) {
            m_state->AddError(Error("Expected colon after key"));

            break;
        }

        object.m_values[token.GetValue()] = ParseExpression();

        if (!Match(TK_COMMA, true)) {
            break;
        }
    }

    if (!Expect(TK_CLOSE_BRACE, true)) {
        m_state->AddError(Error("Expected closing brace"));
    }

    return object;
}

JSONArray Parser::ParseArray()
{
    JSONArray object;

    if (!Expect(TK_OPEN_BRACKET, true)) {
        m_state->AddError(Error("Expected opening bracket"));

        return object;
    }

    while (true) {
        if (Match(TK_CLOSE_BRACKET, false)) {
            break;
        }

        object.m_values.push_back(ParseExpression());

        if (!Match(TK_COMMA, true)) {
            break;
        }
    }

    if (!Expect(TK_CLOSE_BRACKET, true)) {
        m_state->AddError(Error("Expected closing bracket"));
    }

    return object;
}

} // namespace json
} // namespace hyperion