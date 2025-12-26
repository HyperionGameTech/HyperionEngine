#include <script/compiler/Lexer.hpp>
#include <script/compiler/Operator.hpp>
#include <script/compiler/CompilerError.hpp>
#include <script/compiler/Keywords.hpp>

namespace Hyperion {

using namespace utf;

const HashMap<String, Lexer::NumericSuffixInfo> Lexer::s_numericSuffixes = {
    { "u", { "u", CBS_32 } },
    { "i", { "i", CBS_32 } },
    { "l", { "l", CBS_64 } },
    { "il", { "il", CBS_64 } },
    { "ul", { "ul", CBS_64 } },
    { "f", { "f", CBS_32, /* hex */ false } }
};

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

    while (m_sourceStream.HasNext() && m_sourceStream.Peek() != '\0')
    {
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
            if (token && /*token.GetTokenClass() != TK_NEWLINE &&*/ !token.IsContinuationToken())
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

                // add newline
                m_tokenStream->Push(Token(TK_NEWLINE, "newline", location));
            }
        }
    }
}

Token Lexer::NextToken()
{
    SourceLocation location = m_sourceLocation;

    std::array<Char32, 3> ch;
    int totalPosChange = 0;
    for (int i = 0; i < 3; i++)
    {
        int posChange = 0;
        ch[i] = m_sourceStream.Next(posChange);
        totalPosChange += posChange;
    }

    // go back to previous position
    m_sourceStream.GoBack(totalPosChange);

    if (ch[0] == '\"' || ch[0] == '\'')
    {
        return ReadStringLiteral();
    }
    else if (ch[0] == '0' && (ch[1] == 'x' || ch[1] == 'X'))
    {
        return ReadHexNumberLiteral();
    }
    else if (utf::IsDecimal(ch[0]) || (ch[0] == '.' && utf::IsDecimal(ch[1])))
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
        /*if (ch[2] == '*') {
            return ReadDocumentation();
        } else {
            return ReadBlockComment();
        }*/
    }
    else if (ch[0] == '#')
    {
        return ReadNameLiteral();
    }
    else if (utf::IsAlphabetical(ch[0]) || ch[0] == '_' || ch[0] == '$')
    {
        return ReadIdentifier();
    }
    else if (ch[0] == '<' && ch[1] == '-')
    {
        for (int i = 0; i < 2; i++)
        {
            int posChange = 0;
            m_sourceStream.Next(posChange);

            m_sourceLocation.GetColumn()++;
        }
        return Token(TK_LEFT_ARROW, "<-", location);
    }
    else if (ch[0] == '-' && ch[1] == '>')
    {
        for (int i = 0; i < 2; i++)
        {
            int posChange = 0;
            m_sourceStream.Next(posChange);

            m_sourceLocation.GetColumn()++;
        }

        return Token(TK_RIGHT_ARROW, "->", location);
    }
    else if (ch[0] == '=' && ch[1] == '>')
    {
        for (int i = 0; i < 2; i++)
        {
            int posChange = 0;
            m_sourceStream.Next(posChange);

            m_sourceLocation.GetColumn()++;
        }
        return Token(TK_FAT_ARROW, "=>", location);
    }
    else if (ch[0] == '+' || ch[0] == '-' || ch[0] == '*' || ch[0] == '/' || ch[0] == '%' || ch[0] == '^' || ch[0] == '&' || ch[0] == '|' || ch[0] == '<' || ch[0] == '>' || ch[0] == '=' || ch[0] == '!' || ch[0] == '~')
    {
        return ReadOperator();
    }
    else if (ch[0] == ',')
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;

        return Token(TK_COMMA, ",", location);
    }
    else if (ch[0] == ';')
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;

        return Token(TK_SEMICOLON, ";", location);
    }
    else if (ch[0] == ':')
    {
        if (ch[1] == ':')
        {
            for (int i = 0; i < 2; i++)
            {
                int posChange = 0;
                m_sourceStream.Next(posChange);

                m_sourceLocation.GetColumn()++;
            }

            return Token(TK_DOUBLE_COLON, "::", location);
        }
        else if (ch[1] == '=')
        {
            for (int i = 0; i < 2; i++)
            {
                int posChange = 0;
                m_sourceStream.Next(posChange);

                m_sourceLocation.GetColumn()++;
            }

            return Token(TK_DEFINE, ":=", location);
        }

        // single ':' -> plain colon token
        {
            int posChange = 0;
            m_sourceStream.Next(posChange);

            m_sourceLocation.GetColumn()++;

            return Token(TK_COLON, ":", location);
        }
    }

    else if (ch[0] == '?')
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;

        return Token(TK_QUESTION_MARK, "?", location);
    }
    else if (ch[0] == '.')
    {
        if (ch[1] == '.' && ch[2] == '.')
        {
            for (int i = 0; i < 3; i++)
            {
                int posChange = 0;
                m_sourceStream.Next(posChange);

                m_sourceLocation.GetColumn()++;
            }

            return Token(TK_ELLIPSIS, "...", location);
        }
        else
        {
            int posChange = 0;
            m_sourceStream.Next(posChange);

            m_sourceLocation.GetColumn()++;

            return Token(TK_DOT, ".", location);
        }
    }
    else if (ch[0] == '(')
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;

        return Token(TK_OPEN_PARENTH, "(", location);
    }
    else if (ch[0] == ')')
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;

        return Token(TK_CLOSE_PARENTH, ")", location);
    }
    else if (ch[0] == '[')
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;

        return Token(TK_OPEN_BRACKET, "[", location);
    }
    else if (ch[0] == ']')
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;

        return Token(TK_CLOSE_BRACKET, "]", location);
    }
    else if (ch[0] == '{')
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;

        return Token(TK_OPEN_BRACE, "{", location);
    }
    else if (ch[0] == '}')
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;

        return Token(TK_CLOSE_BRACE, "}", location);
    }
    else
    {
        int posChange = 0;
        utf::Char32 badToken = m_sourceStream.Next(posChange);

        String badTokenStr;
        badTokenStr.Append(badToken);

        m_compilationUnit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_unexpected_token,
            location,
            badTokenStr));

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
        int posChange = 0;
        Char32 esc = m_sourceStream.Next(posChange);

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
                String("\\") + esc));
        }
    }

    return 0;
}

Token Lexer::ReadStringLiteral()
{
    // the location for the start of the string
    SourceLocation location = m_sourceLocation;

    String value;
    int posChange = 0;

    Char32 delim = m_sourceStream.Next(posChange);
    m_sourceLocation.GetColumn()++;

    // the character as utf-32
    Char32 ch = m_sourceStream.Next(posChange);
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
            value.Append(esc);
        }
        else
        {
            // Append the character itself
            value.Append(ch);
        }

        ch = m_sourceStream.Next(posChange);

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

        int posChange = 0;
        m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;
    }

    Token::Flags tokenFlags;
    Memory::MemSet(tokenFlags, 0, sizeof(tokenFlags));

    Char32 ch = m_sourceStream.Peek();

    bool hasExponent = false;

    while (m_sourceStream.HasNext() && utf::IsDecimal(ch))
    {
        int posChange = 0;
        Char32 nextCh = m_sourceStream.Next(posChange);
        value.Append(nextCh);

        m_sourceLocation.GetColumn()++;

        if (tokenClass != TK_FLOAT)
        {
            if (m_sourceStream.HasNext())
            {
                // the character as a utf-32 character
                Char32 ch = m_sourceStream.Peek();
                if (ch == (Char32)'.')
                {
                    int posChange = 0;
                    m_sourceStream.Next(posChange);

                    tokenClass = TK_FLOAT;
                    value.Append(ch);

                    m_sourceLocation.GetColumn();
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
                value.Append(ch);

                int posChange = 0;
                m_sourceStream.Next(posChange);

                m_sourceLocation.GetColumn()++;

                ch = m_sourceStream.Peek();

                // Handle negative, positive exponent
                if (ch == '-' || ch == '+')
                {
                    value.Append(ch);

                    int posChange = 0;
                    m_sourceStream.Next(posChange);

                    m_sourceLocation.GetColumn()++;
                }
            }
        }

        ch = m_sourceStream.Peek();
    }

    String suffix;
    ch = m_sourceStream.Peek();
    while (m_sourceStream.HasNext() && utf::IsAlphabetical(ch))
    {
        suffix.Append(utf::ToUtf8Chars(ch));
        m_sourceStream.Next();
        m_sourceLocation.GetColumn()++;
        ch = m_sourceStream.Peek();
    }

    if (!suffix.Empty())
    {
        const String lowerSuffix = suffix.ToLower();
        bool isValidSuffix = false;

        for (const auto& it : s_numericSuffixes)
        {
            if (lowerSuffix == it.first)
            {
                isValidSuffix = true;

                Memory::StrCpy(&tokenFlags[0], it.second.flags, sizeof(tokenFlags));

                break;
            }
        }

        if (!isValidSuffix)
        {
            // Unknown suffix - report error
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unrecognized_numeric_suffix,
                location,
                suffix));
        }
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

        int posChange = 0;
        Char32 nextCh = m_sourceStream.Next(posChange);
        value.Append(nextCh);

        m_sourceLocation.GetColumn()++;
    }

    Token::Flags tokenFlags;
    Memory::MemSet(tokenFlags, 0, sizeof(tokenFlags));

    Char32 ch = m_sourceStream.Peek();

    while (m_sourceStream.HasNext() && utf::IsHexadecimal(ch))
    {
        int posChange = 0;
        Char32 nextCh = m_sourceStream.Next(posChange);
        value.Append(nextCh);

        m_sourceLocation.GetColumn()++;

        ch = m_sourceStream.Peek();
    }

    // suffix

    String suffix;

    ch = m_sourceStream.Peek();

    while (m_sourceStream.HasNext() && utf::IsAlphabetical(ch))
    {
        suffix.Append(utf::ToUtf8Chars(ch));
        m_sourceStream.Next();
        m_sourceLocation.GetColumn()++;
        ch = m_sourceStream.Peek();
    }

    if (!suffix.Empty())
    {
        const String lowerSuffix = suffix.ToLower();
        bool isValidSuffix = false;

        for (const auto& it : s_numericSuffixes)
        {
            if (lowerSuffix == it.first && it.second.hex)
            {
                isValidSuffix = true;

                Memory::StrCpy(&tokenFlags[0], it.second.flags, sizeof(tokenFlags));

                break;
            }
        }

        if (!isValidSuffix)
        {
            // Unknown suffix - report error
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unrecognized_numeric_suffix,
                location,
                suffix));
        }
    }

    int64 num = std::strtoll(value.Data(), 0, 16);

    return Token(TK_INTEGER, String::ToString(num), tokenFlags, location);
}

Token Lexer::ReadLineComment()
{
    SourceLocation location = m_sourceLocation;

    // read '//'
    for (int i = 0; i < 2; i++)
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;
    }

    // read until newline or EOF is reached
    while (m_sourceStream.HasNext() && m_sourceStream.Peek() != '\n')
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;
    }

    return Token(TK_NEWLINE, "newline", location);
}

Token Lexer::ReadBlockComment()
{
    SourceLocation location = m_sourceLocation;

    // read '/*'
    for (int i = 0; i < 2; i++)
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;
    }

    Char32 previous = 0;
    while (HasNext())
    {
        if (m_sourceStream.Peek() == (Char32)'/' && previous == (Char32)'*')
        {
            int posChange = 0;
            m_sourceStream.Next(posChange);

            m_sourceLocation.GetColumn()++;

            break;
        }
        else if (m_sourceStream.Peek() == (Char32)'\n')
        {
            // just reset column and increment line
            m_sourceLocation.GetColumn() = 0;
            m_sourceLocation.GetLine()++;
        }

        int posChange = 0;
        previous = m_sourceStream.Next(posChange);

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
        int posChange = 0;
        m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;
    }

    Char32 previous = 0;
    while (HasNext())
    {
        if (m_sourceStream.Peek() == (Char32)'/' && previous == (Char32)'*')
        {
            int posChange = 0;
            m_sourceStream.Next(posChange);

            m_sourceLocation.GetColumn()++;

            break;
        }
        else
        {
            // append value
            value.Append(m_sourceStream.Peek());

            if (m_sourceStream.Peek() == (Char32)'\n')
            {
                // just reset column and increment line
                m_sourceLocation.GetColumn() = 0;
                m_sourceLocation.GetLine()++;
            }
        }

        int posChange = 0;
        previous = m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;
    }

    return Token::EMPTY;
}

Token Lexer::ReadOperator()
{
    // location of the start of the hex number
    SourceLocation location = m_sourceLocation;

    FixedArray<Char32, 2> ch = { 0, 0 };

    int totalPosChange = 0;
    for (int i = 0; i < 2; i++)
    {
        int posChange = 0;
        ch[i] = m_sourceStream.Next(posChange);
        totalPosChange += posChange;
    }

    // go back
    m_sourceStream.GoBack(totalPosChange);

    String op_1;
    op_1.Append(ch[0]);

    String op_2 = op_1;
    op_2.Append(ch[1]);

    if (op_2.Length() > op_1.Length() && (Operator::IsUnaryOperator(op_2) || Operator::IsBinaryOperator(op_2)))
    {
        m_sourceStream.Next();
        m_sourceStream.Next();

        m_sourceLocation.GetColumn() += 2;

        return Token(TK_OPERATOR, op_2, location);
    }

    if (Operator::IsUnaryOperator(op_1) || Operator::IsBinaryOperator(op_1))
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

    while (utf::IsDecimal(ch) || utf::IsAlphabetical(ch) || ch == '_' || ch == '$')
    {
        int posChange = 0;
        ch = m_sourceStream.Next(posChange);

        m_sourceLocation.GetColumn()++;

        // append the raw bytes
        value.Append(ch);
        // set ch to be the next character in the buffer
        ch = m_sourceStream.Peek();

        // if (ch == ':') {
        //     int posChange = 0;
        //     ch = m_sourceStream.Next(posChange);
        //     m_sourceLocation.GetColumn() += posChange;

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
            int posChangeChars = 0;

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

                ++posChangeChars;
                m_sourceLocation.GetColumn()++;

                posChange += charPosChange;
            }

            if (isOperator)
            {
                return Token(TK_IDENT, "operator" + op, location);
            }

            // rewind
            m_sourceStream.GoBack(posChange);
            m_sourceLocation.GetColumn() -= posChangeChars;
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

    return Token(Keyword::IsKeyword(value) ? TK_KEYWORD : TK_IDENT, value, location);
}

Token Lexer::ReadNameLiteral()
{
    // starting location should be at the '#'
    SourceLocation location = m_sourceLocation;

    // consume '#'
    int posChange = 0;
    m_sourceStream.Next(posChange);
    m_sourceLocation.GetColumn()++;

    String value;

    Char32 nextCh = m_sourceStream.Peek();

    // quoted name: :"..." or :'...'
    if (nextCh == '"' || nextCh == '\'')
    {
        // consume delimiter
        Char32 delim = m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn()++;

        Char32 ch = m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn()++;

        while (ch != delim)
        {
            if (ch == (Char32)'\\')
            {
                // escape sequence
                Char32 esc = ReadEscapeCode();
                value.Append(esc);
            }
            else
            {
                value.Append(ch);
            }

            ch = m_sourceStream.Next(posChange);
            m_sourceLocation.GetColumn()++;

            if (!m_sourceStream.HasNext())
            {
                m_compilationUnit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_unterminated_string_literal,
                    m_sourceLocation));

                break;
            }
        }

        return Token(TK_NAME_LITERAL, value, location);
    }

    // unquoted: read identifier rules
    Char32 ch = m_sourceStream.Peek();

    while (utf::IsDecimal(ch) || utf::IsAlphabetical(ch) || ch == '_' || ch == '$')
    {
        int pos = 0;
        ch = m_sourceStream.Next(pos);
        m_sourceLocation.GetColumn()++;

        value.Append(ch);
        ch = m_sourceStream.Peek();
    }

    return Token(TK_NAME_LITERAL, value, location);
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

    while (m_sourceStream.HasNext() && utf::IsWhitespace(m_sourceStream.Peek()))
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

} // namespace Hyperion
