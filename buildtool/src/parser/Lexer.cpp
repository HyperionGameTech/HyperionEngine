/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <parser/Lexer.hpp>
#include <parser/CompilerError.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/memory/Memory.hpp>

#include <sstream>
#include <cstdlib>

namespace Hyperion::CodeGen {

using namespace utf;

Lexer::Lexer(
    const SourceStream& sourceStream,
    TokenStream* tokenStream,
    CompilationUnit* compilationUnit)
    : m_sourceStream(sourceStream),
      m_tokenStream(tokenStream),
      m_compilationUnit(compilationUnit),
      m_sourceLocation(0, 0, sourceStream.GetFile()->GetFilePath())
{
}

Lexer::Lexer(const Lexer& other)
    : m_sourceStream(other.m_sourceStream),
      m_tokenStream(other.m_tokenStream),
      m_compilationUnit(other.m_compilationUnit),
      m_sourceLocation(other.m_sourceLocation)
{
}

void Lexer::Analyze()
{
    // skip initial whitespace
    SkipWhitespace();

    while (m_sourceStream.HasNext())
    {
        if (!m_sourceStream.Peek())
        {
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unexpected_eof,
                m_sourceLocation));

            return;
        }

        Token token = NextToken();
        if (!token.Empty())
        {
            m_tokenStream->Push(token);
        }

        // SkipWhitespace() returns true if there was a newline
        const SourceLocation location = m_sourceLocation;

        if (SkipWhitespace())
        {
            // add the `newline` statement terminator if not a continuation token
            if (token && !token.IsContinuationToken())
            {
                // skip whitespace before next token
                SkipWhitespace();

                // check if next token is connected
                if (m_sourceStream.HasNext() && m_sourceStream.Peek() != '\0')
                {
                    auto peek = m_sourceStream.Peek();
                    if (peek == '{' || peek == '.')
                    {
                        // do not add newline
                        continue;
                    }
                }
            }
        }
    }
}

Token Lexer::NextToken()
{
    SourceLocation location = m_sourceLocation;

    FixedArray<Char32, 3> ch;

    int totalPosChange = 0;

    for (int i = 0; i < 3; i++)
    {
        int posChange = 0;
        ch[i] = m_sourceStream.Next(posChange);
        totalPosChange += posChange;
    }

    // go back to previous position
    m_sourceStream.GoBack(totalPosChange);

    if (ch[0] == '\"')
    {
        return ReadStringLiteral();
    }
    else if (ch[0] == '0' && (ch[1] == 'x' || ch[1] == 'X'))
    {
        return ReadHexNumberLiteral();
    }
    else if (IsDecimal(ch[0]) || (ch[0] == '.' && IsDecimal(ch[1])))
    {
        return ReadNumberLiteral();
    }
    else if (ch[0] == '/' && ch[1] == '/')
    {
        return ReadLineComment();
    }
    else if (ch[0] == '/' && ch[1] == '*')
    {
        return ReadBlockComment();
    }
    else if (IsAlphabetical(ch[0]) || ch[0] == '_' || ch[0] == '$')
    {
        return ReadIdentifier();
    }
    else if (ch[0] == '-' && ch[1] == '>')
    {
        for (int i = 0; i < 2; i++)
        {
            m_sourceStream.Next();

            m_sourceLocation.GetColumn()++;
        }

        return Token(TK_RIGHT_ARROW, "->", location);
    }
    else if (ch[0] == '+' || ch[0] == '-' || ch[0] == '*' || ch[0] == '/' || ch[0] == '%' || ch[0] == '^' || ch[0] == '&' || ch[0] == '|' || ch[0] == '<' || ch[0] == '>' || ch[0] == '=' || ch[0] == '!' || ch[0] == '~')
    {
        return ReadOperator();
    }
    else if (ch[0] == ',')
    {
        m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;

        return Token(TK_COMMA, ",", location);
    }
    else if (ch[0] == ';')
    {
        m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;

        return Token(TK_SEMICOLON, ";", location);
    }
    else if (ch[0] == ':')
    {
        if (ch[1] == ':')
        {
            for (int i = 0; i < 2; i++)
            {
                m_sourceStream.Next();

                m_sourceLocation.GetColumn()++;
            }

            return Token(TK_DOUBLE_COLON, "::", location);
        }
        else
        {
            m_sourceStream.Next();

            m_sourceLocation.GetColumn()++;

            return Token(TK_COLON, ":", location);
        }
    }
    else if (ch[0] == '?')
    {
        m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;

        return Token(TK_QUESTION_MARK, "?", location);
    }
    else if (ch[0] == '.')
    {
        if (ch[1] == '.' && ch[2] == '.')
        {
            for (int i = 0; i < 3; i++)
            {
                m_sourceStream.Next();

                m_sourceLocation.GetColumn()++;
            }

            return Token(TK_ELLIPSIS, "...", location);
        }
        else
        {
            m_sourceStream.Next();

            m_sourceLocation.GetColumn()++;

            return Token(TK_DOT, ".", location);
        }
    }
    else if (ch[0] == '(')
    {
        m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;

        return Token(TK_OPEN_PARENTH, "(", location);
    }
    else if (ch[0] == ')')
    {
        m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;

        return Token(TK_CLOSE_PARENTH, ")", location);
    }
    else if (ch[0] == '[')
    {
        m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;

        return Token(TK_OPEN_BRACKET, "[", location);
    }
    else if (ch[0] == ']')
    {
        m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;

        return Token(TK_CLOSE_BRACKET, "]", location);
    }
    else if (ch[0] == '{')
    {
        m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;

        return Token(TK_OPEN_BRACE, "{", location);
    }
    else if (ch[0] == '}')
    {
        m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;

        return Token(TK_CLOSE_BRACE, "}", location);
    }
    else
    {
        Char32 badTokenU32 = m_sourceStream.Next();

        m_compilationUnit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_unexpected_token,
            location,
            UTF32String(&badTokenU32, &badTokenU32 + 1).ToUtf8()));

        m_sourceLocation.GetColumn()++;

        return Token::EMPTY;
    }
}

Char32 Lexer::ReadEscapeCode()
{
    // location of the start of the escape code
    SourceLocation location = m_sourceLocation;

    if (HasNext())
    {
        Char32 esc = m_sourceStream.Next();
        m_sourceLocation.GetColumn()++;

        // TODO: add support for unicode escapes
        switch (esc)
        {
        case 't':
            return '\t';
        case 'b':
            return '\b';
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 'f':
            return '\f';
        case '\'':
        case '\"':
        case '\\':
            // return the escape itself
            return esc;
        default:
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unrecognized_escape_sequence,
                location,
                String("\\") + utf::ToUtf8Chars(esc)));
        }
    }

    return 0;
}

Token Lexer::ReadStringLiteral()
{
    // the location for the start of the string
    SourceLocation location = m_sourceLocation;

    String value;

    Char32 delim = m_sourceStream.Next();
    m_sourceLocation.GetColumn()++;

    // the character as utf-32
    Char32 ch = m_sourceStream.Next();
    m_sourceLocation.GetColumn()++;

    while (ch != delim)
    {
        if (ch == (Char32)'\n' || !HasNext())
        {
            // unterminated string literal
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unterminated_string_literal,
                m_sourceLocation));

            if (ch == (Char32)'\n')
            {
                // increment line and reset column
                m_sourceLocation.GetColumn() = 0;
                m_sourceLocation.GetLine()++;
            }

            break;
        }

        // determine whether to read an escape sequence
        if (ch == (Char32)'\\')
        {
            Char32 esc = ReadEscapeCode();
            // append the bytes
            value.Append(utf::ToUtf8Chars(esc));
        }
        else
        {
            // Append the character itself
            value.Append(utf::ToUtf8Chars(ch));
        }

        ch = m_sourceStream.Next();
        m_sourceLocation.GetColumn()++;
    }

    return Token(TK_STRING, value, location);
}

Token Lexer::ReadNumberLiteral()
{
    SourceLocation location = m_sourceLocation;

    // store the value in a string
    String value;

    // assume integer to start
    TokenClass tokenClass = TK_INTEGER;

    // allows support for floats starting with '.'
    if (m_sourceStream.Peek() == '.')
    {
        tokenClass = TK_FLOAT;
        value = "0.";

        m_sourceStream.Next();
        m_sourceLocation.GetColumn()++;
    }

    Token::Flags tokenFlags;
    Memory::MemSet(tokenFlags, 0, sizeof(tokenFlags));

    Char32 ch = m_sourceStream.Peek();

    bool hasExponent = false;

    while (m_sourceStream.HasNext() && IsDecimal(ch))
    {
        int posChange = 0;

        Char32 nextChar = m_sourceStream.Next();
        value.Append(utf::ToUtf8Chars(nextChar));

        m_sourceLocation.GetColumn()++;

        if (tokenClass != TK_FLOAT)
        {
            if (m_sourceStream.HasNext())
            {
                // the character as a utf-32 character
                Char32 ch = m_sourceStream.Peek();
                if (ch == (Char32)'.')
                {
                    // read next to check if after is a digit
                    int posChange = 0;
                    m_sourceStream.Next(posChange);

                    Char32 next = m_sourceStream.Peek();
                    if (!utf::IsAlphabetical(next) && next != (Char32)'_')
                    {
                        // type is a float because of '.' and not an identifier after
                        tokenClass = TK_FLOAT;
                        value.Append(utf::ToUtf8Chars(ch));

                        m_sourceLocation.GetColumn()++;
                    }
                    else
                    {
                        // not a float literal, so go back on the '.'
                        m_sourceStream.GoBack(posChange);
                    }
                }
            }
        }

        if (m_sourceStream.HasNext())
        {
            Char32 ch = m_sourceStream.Peek();

            if (!hasExponent && (ch == (Char32)'e' || ch == (Char32)'E'))
            {
                hasExponent = true;

                tokenClass = TK_FLOAT;
                value.Append(utf::ToUtf8Chars(ch));

                int posChange = 0;
                m_sourceStream.Next(posChange);

                m_sourceLocation.GetColumn()++;

                ch = m_sourceStream.Peek();

                // Handle negative, positive exponent
                if (ch == '-' || ch == '+')
                {
                    value.Append(utf::ToUtf8Chars(ch));

                    int posChange = 0;
                    m_sourceStream.Next(posChange);

                    m_sourceLocation.GetColumn()++;
                }
            }
        }

        ch = m_sourceStream.Peek();
    }

    switch (tolower(ch))
    {
    case 'u': // fallthrough
    case 'i':
    {
        tokenFlags[0] = tolower(ch);
        value.Append(utf::ToUtf8Chars(ch));
        m_sourceStream.Next();
        m_sourceLocation.GetColumn()++;

        if (m_sourceStream.HasNext())
        {
            ch = m_sourceStream.Peek();

            if (ch == 'l' || ch == 'L')
            {
                tokenFlags[1] = 'l';
                value.Append(utf::ToUtf8Chars(ch));
                m_sourceStream.Next();
                m_sourceLocation.GetColumn()++;

                if (m_sourceStream.HasNext())
                {
                    ch = m_sourceStream.Peek();

                    if (ch == 'l' || ch == 'L')
                    {
                        tokenFlags[2] = 'l';
                        value.Append(utf::ToUtf8Chars(ch));
                        m_sourceStream.Next();
                        m_sourceLocation.GetColumn()++;
                    }
                }
            }
        }

        break;
    }
    case 'f':
        tokenFlags[0] = (char)ch;
        value.Append(utf::ToUtf8Chars(ch));

        if (m_sourceStream.HasNext())
        {
            m_sourceStream.Next();
            ch = m_sourceStream.Peek();
        }

        break;
    }

    return Token(tokenClass, value, tokenFlags, location);
}

Token Lexer::ReadHexNumberLiteral()
{
    // location of the start of the hex number
    SourceLocation location = m_sourceLocation;

    // store the value in a string
    String value;

    // read the "0x"
    for (int i = 0; i < 2; i++)
    {
        if (!m_sourceStream.HasNext())
        {
            break;
        }

        Char32 nextChar = m_sourceStream.Next();
        value.Append(utf::ToUtf8Chars(nextChar));

        m_sourceLocation.GetColumn()++;
    }

    Token::Flags tokenFlags;
    Memory::MemSet(tokenFlags, 0, sizeof(tokenFlags));

    Char32 ch = m_sourceStream.Peek();

    while (m_sourceStream.HasNext() && IsHexadecimal(ch))
    {
        Char32 nextChar = m_sourceStream.Next();

        value.Append(utf::ToUtf8Chars(nextChar));

        m_sourceLocation.GetColumn()++;

        ch = m_sourceStream.Peek();
    }

    switch (ch)
    {
    case 'u':
    case 'i':
        tokenFlags[0] = (char)ch;

        if (m_sourceStream.HasNext())
        {
            m_sourceStream.Next();
            ch = m_sourceStream.Peek();
        }

        break;
    }

    int64 num = std::strtoll(value.Data(), 0, 16);

    return Token(TK_INTEGER, value, tokenFlags, location);
}

Token Lexer::ReadLineComment()
{
    SourceLocation location = m_sourceLocation;

    // read '//'
    for (int i = 0; i < 2; i++)
    {
        m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;
    }

    // read until newline or EOF is reached
    while (m_sourceStream.HasNext() && m_sourceStream.Peek() != '\n')
    {
        m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;
    }

    return Token::EMPTY;
}

Token Lexer::ReadBlockComment()
{
    SourceLocation location = m_sourceLocation;

    // read '/*'
    for (int i = 0; i < 2; i++)
    {
        m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;
    }

    Char32 previous = 0;

    while (HasNext())
    {
        if (m_sourceStream.Peek() == (Char32)'/' && previous == (Char32)'*')
        {
            m_sourceStream.Next();

            m_sourceLocation.GetColumn()++;

            break;
        }
        else if (m_sourceStream.Peek() == (Char32)'\n')
        {
            // just reset column and increment line
            m_sourceLocation.GetColumn() = 0;
            m_sourceLocation.GetLine()++;
        }

        previous = m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;
    }

    return Token::EMPTY;
}

Token Lexer::ReadDocumentation()
{
    SourceLocation location = m_sourceLocation;

    String value;

    // read '/**'
    for (int i = 0; i < 3; i++)
    {
        m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;
    }

    Char32 previous = 0;

    while (HasNext())
    {
        if (m_sourceStream.Peek() == (Char32)'/' && previous == (Char32)'*')
        {
            m_sourceStream.Next();

            m_sourceLocation.GetColumn()++;

            break;
        }
        else
        {
            utf::Char8 ch[4] = { '\0' };
            utf::Char32to8(m_sourceStream.Peek(), ch);

            // append value
            value += reinterpret_cast<const String::CharType*>(ch);

            if (m_sourceStream.Peek() == (Char32)'\n')
            {
                // just reset column and increment line
                m_sourceLocation.GetColumn() = 0;
                m_sourceLocation.GetLine()++;
            }
        }

        previous = m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;
    }

    return Token::EMPTY;
}

Token Lexer::ReadOperator()
{
    // location of the start of the hex number
    SourceLocation location = m_sourceLocation;

    FixedArray<Char32, 2> ch;

    int totalPosChange = 0;

    for (int i = 0; i < 2; i++)
    {
        int posChange = 0;
        ch[i] = m_sourceStream.Next(posChange);
        totalPosChange += posChange;
    }

    // go back
    m_sourceStream.GoBack(totalPosChange);

    String op_1 = utf::ToUtf8Chars(ch[0]);
    String op_2 = op_1 + utf::ToUtf8Chars(ch[1]);

    if (Operator::IsUnaryOperator(op_2) || Operator::IsBinaryOperator(op_2))
    {
        m_sourceStream.Next();
        m_sourceStream.Next();

        m_sourceLocation.GetColumn() += 2;

        return Token(TK_OPERATOR, op_2, location);
    }
    else if (Operator::IsUnaryOperator(op_1) || Operator::IsBinaryOperator(op_1))
    {
        m_sourceStream.Next();

        m_sourceLocation.GetColumn()++;

        return Token(TK_OPERATOR, op_1, location);
    }

    return Token::EMPTY;
}

Token Lexer::ReadIdentifier()
{
    SourceLocation location = m_sourceLocation;

    // store the name in this string
    String value;

    // the character as a utf-32 character
    auto ch = m_sourceStream.Peek();

    while (IsDecimal(ch) || IsAlphabetical(ch) || ch == '_' || ch == '$')
    {
        ch = m_sourceStream.Next();

        // append the raw bytes
        value.Append(utf::ToUtf8Chars(ch));

        // set ch to be the next character in the buffer
        ch = m_sourceStream.Peek();

        m_sourceLocation.GetColumn()++;

        // if (ch == ':') {
        //     int posChange = 0;
        //     ch = m_sourceStream.Next(posChange);

        //     m_sourceLocation.GetColumn()++;

        //     return Token(TK_LABEL, value, location);
        // }
    }

    // read operator in the case that the string is "operator", like c++
    if (value == "operator")
    {
        // allow for other operators not defined in operator list such as "operator[]" and "operator[]="
        static const String otherOperators[] {
            "[]=", "[]"
        };

        for (const String& op : otherOperators)
        {
            // check if next tokens are the operator

            const SizeType len = op.Length();

            int posChange = 0;

            bool isOperator = true;

            for (SizeType i = 0; i < len; i++)
            {
                if (m_sourceStream.Peek() != op.GetChar(i))
                {
                    isOperator = false;
                    break;
                }

                int charPosChange = 0;
                m_sourceStream.Next(charPosChange);

                posChange += charPosChange;
            }

            if (isOperator)
            {
                m_sourceLocation.GetColumn() += len;

                return Token(TK_IDENT, "operator" + op, location);
            }

            // rewind
            m_sourceStream.GoBack(posChange);
        }

        if (Token operatorToken = ReadOperator())
        {
            value += operatorToken.GetValue();

            const Operator* op = nullptr;

            if (!Operator::IsBinaryOperator(operatorToken.GetValue(), op))
            {
                Assert(Operator::IsUnaryOperator(operatorToken.GetValue(), op));
            }

            Assert(op != nullptr);

            if (!op->SupportsOverloading())
            {
                m_compilationUnit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_cannot_overload_operator,
                    m_sourceLocation,
                    operatorToken.GetValue()));
            }
        }
    }

    return Token(TK_IDENT, value, location);
}

bool Lexer::HasNext()
{
    if (!m_sourceStream.HasNext())
    {
        m_compilationUnit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_unexpected_eof,
            m_sourceLocation));

        return false;
    }

    return true;
}

bool Lexer::SkipWhitespace()
{
    bool hadNewline = false;

    while (m_sourceStream.HasNext() && IsWhitespace(m_sourceStream.Peek()))
    {
        int posChange = 0;
        if (m_sourceStream.Next(posChange) == (Char32)'\n')
        {
            m_sourceLocation.GetLine()++;
            m_sourceLocation.GetColumn() = 0;

            hadNewline = true;
        }
        else
        {
            m_sourceLocation.GetColumn()++;
        }
    }

    return hadNewline;
}

} // namespace Hyperion::CodeGen
