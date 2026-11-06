/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/DataProcessing/Shared/Lexer.hpp>
#include <Core/DataProcessing/Shared/CompilerError.hpp>

#include <cctype>
#include <cstring>

namespace Hyperion::DataProcessing {

using namespace utf;

static bool IsSpecialFloatName(UTF8StringView text)
{
    const auto matches = [&text](const char* name) -> bool
    {
        const size_t length = std::strlen(name);

        if (text.Size() != length)
        {
            return false;
        }

        for (size_t i = 0; i < length; i++)
        {
            if (std::tolower((unsigned char)text.Data()[i]) != name[i])
            {
                return false;
            }
        }

        return true;
    };

    return matches("inf") || matches("infinity") || matches("nan");
}

Lexer::Lexer(
    TokenStream* tokenStream,
    ErrorList<CompilerError>* errorList)
    : m_sourceStream(tokenStream->GetSourceData(), tokenStream->GetSourceSize()),
      m_tokenStream(tokenStream),
      m_errorList(errorList),
      m_sourceLocation(0, 0, &tokenStream->GetInfo().filepath)
{
}

Lexer::Lexer(const Lexer& other)
    : m_sourceStream(other.m_sourceStream),
      m_tokenStream(other.m_tokenStream),
      m_errorList(other.m_errorList),
      m_sourceLocation(other.m_sourceLocation)
{
}

UTF8StringView Lexer::Slice(size_t start, size_t end) const
{
    const char* source = m_tokenStream->GetSourceData();

    return UTF8StringView(source + start, source + end);
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

        SkipWhitespace();

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

Token Lexer::ReadPunctuation(TokenClass tokenClass)
{
    const SourceLocation location = m_sourceLocation;
    const size_t start = m_sourceStream.GetPosition();

    int posChange = 0;
    m_sourceStream.Next(posChange);
    m_sourceLocation.GetColumn() += posChange;

    return Token(tokenClass, Slice(start, m_sourceStream.GetPosition()), location);
}

Token Lexer::NextToken()
{
    const SourceLocation location = m_sourceLocation;

    Char32 ch[3] = { 0 };
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
    else if (ch[0] == '@' && (ch[1] == '\"' || ch[1] == '\''))
    {
        return ReadAssetPathLiteral();
    }
    else if (ch[0] == '0' && (ch[1] == 'x' || ch[1] == 'X'))
    {
        return ReadHexNumberLiteral();
    }
    else if (IsDecimal(ch[0]) || (ch[0] == '.' && IsDecimal(ch[1])) || (ch[0] == '-' && (IsDecimal(ch[1]) || ch[1] == 'i' || ch[1] == 'n')) || (ch[0] == '+' && (IsDecimal(ch[1]) || ch[1] == 'i' || ch[1] == 'n')))
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

    switch (ch[0])
    {
    case ',':
        return ReadPunctuation(TK_COMMA);
    case ';':
        return ReadPunctuation(TK_SEMICOLON);
    case ':':
        return ReadPunctuation(TK_COLON);
    case '.':
        return ReadPunctuation(TK_DOT);
    case '=':
        return ReadPunctuation(TK_EQUALS);
    case '|':
        return ReadPunctuation(TK_PIPE);
    case '(':
        return ReadPunctuation(TK_OPEN_PARENTH);
    case ')':
        return ReadPunctuation(TK_CLOSE_PARENTH);
    case '[':
        return ReadPunctuation(TK_OPEN_BRACKET);
    case ']':
        return ReadPunctuation(TK_CLOSE_BRACKET);
    case '{':
        return ReadPunctuation(TK_OPEN_BRACE);
    case '}':
        return ReadPunctuation(TK_CLOSE_BRACE);
    default:
        break;
    }

    int posChange = 0;
    utf::Char32 badToken = m_sourceStream.Next(posChange);

    utf::Char8 badTokenStr[sizeof(utf::Char32) + 1] = { '\0' };
    utf::Char32to8(badToken, badTokenStr);

    m_errorList->AddError(CompilerError(
        ErrorLevel::Error,
        MSG_UNEXPECTED_TOKEN,
        location,
        String(badTokenStr)));

    m_sourceLocation.GetColumn() += posChange;

    return Token::empty;
}

Char32 Lexer::ReadEscapeCode()
{
    // location of the start of the escape code
    SourceLocation location = m_sourceLocation;

    if (HasNext())
    {
        int posChange = 0;
        utf::Char32 esc = m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn() += posChange;

        utf::Char8 chars[sizeof(utf::Char32) + 1] = { '\0' };
        utf::Char32to8(esc, chars);

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
            m_errorList->AddError(CompilerError(
                ErrorLevel::Error,
                MSG_UNRECOGNIZED_ESCAPE_SEQUENCE,
                location,
                String("\\") + reinterpret_cast<const String::CharType*>(chars)));
        }
    }

    return 0;
}

Token Lexer::ReadStringLiteral(TokenClass tokenClass)
{
    // the location for the start of the string
    SourceLocation location = m_sourceLocation;

    int posChange = 0;

    Char32 delim = m_sourceStream.Next(posChange);
    m_sourceLocation.GetColumn() += posChange;

    const size_t contentStart = m_sourceStream.GetPosition();

    // only a string with escapes gets its own copy, the rest point into the source
    String escapedValue;
    bool hasEscapes = false;

    size_t charStart = m_sourceStream.GetPosition();

    // the character as utf-32
    Char32 ch = m_sourceStream.Next(posChange);
    m_sourceLocation.GetColumn() += posChange;

    while (ch != delim)
    {
        if (ch == (Char32)'\n' || !HasNext())
        {
            // unterminated string literal
            m_errorList->AddError(CompilerError(
                ErrorLevel::Error,
                MSG_UNTERMINATED_STRING_LITERAL,
                m_sourceLocation));

            if (ch == (Char32)'\n')
            {
                // increment line and reset column
                m_sourceLocation.GetColumn() = 0;
                m_sourceLocation.GetLine()++;
            }

            break;
        }

        if (ch == (Char32)'\\')
        {
            if (!hasEscapes)
            {
                escapedValue = String(Slice(contentStart, charStart));
                hasEscapes = true;
            }

            Char32 esc = ReadEscapeCode();
            escapedValue.Append(utf::ToUtf8Chars(esc));
        }
        else if (hasEscapes)
        {
            escapedValue.Append(utf::ToUtf8Chars(ch));
        }

        charStart = m_sourceStream.GetPosition();

        ch = m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn() += posChange;
    }

    if (hasEscapes)
    {
        return Token(tokenClass, m_tokenStream->Store(std::move(escapedValue)), location);
    }

    return Token(tokenClass, Slice(contentStart, charStart), location);
}

Token Lexer::ReadAssetPathLiteral()
{
    SourceLocation location = m_sourceLocation;

    // consume the leading '@'
    int posChange = 0;
    m_sourceStream.Next(posChange);
    m_sourceLocation.GetColumn() += posChange;

    // read the string body and emit as TK_AT_STRING
    return ReadStringLiteral(TK_AT_STRING);
}

Token Lexer::ReadNumberLiteral()
{
    SourceLocation location = m_sourceLocation;

    // assume integer to start
    TokenClass tokenClass = TK_INTEGER;

    size_t valueStart = m_sourceStream.GetPosition();

    // allow '-' or '+' at the start, '+' is left out of the value
    if (m_sourceStream.Peek() == '-')
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn() += posChange;
    }
    else if (m_sourceStream.Peek() == '+')
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn() += posChange;

        valueStart = m_sourceStream.GetPosition();
    }

    // allows support for floats starting with '.', which read as "0."
    bool hasLeadingDot = false;
    size_t leadingDotPosition = 0;

    if (m_sourceStream.Peek() == '.')
    {
        tokenClass = TK_FLOAT;
        hasLeadingDot = true;
        leadingDotPosition = m_sourceStream.GetPosition();

        int posChange = 0;
        m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn() += posChange;
    }

    Token::Flags tokenFlags {};

    Char32 ch = m_sourceStream.Peek();

    if (ch == (Char32)'i' || ch == (Char32)'n')
    {
        const size_t identStart = m_sourceStream.GetPosition();

        while (m_sourceStream.HasNext() && (utf::IsAlphabetical(ch) || ch == (Char32)'_'))
        {
            int posChange = 0;
            m_sourceStream.Next(posChange);

            m_sourceLocation.GetColumn() += posChange;

            ch = m_sourceStream.HasNext() ? m_sourceStream.Peek() : 0;
        }

        if (IsSpecialFloatName(Slice(identStart, m_sourceStream.GetPosition())))
        {
            return Token(TK_FLOAT, Slice(valueStart, m_sourceStream.GetPosition()), location);
        }

        m_errorList->AddError(CompilerError(
            ErrorLevel::Error,
            MSG_UNEXPECTED_TOKEN,
            location,
            String(Slice(valueStart, m_sourceStream.GetPosition()))));

        return Token::empty;
    }

    bool hasExponent = false;

    while (m_sourceStream.HasNext() && IsDecimal(ch))
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn() += posChange;

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
                        m_sourceLocation.GetColumn() += posChange;
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

                int posChange = 0;
                m_sourceStream.Next(posChange);
                m_sourceLocation.GetColumn() += posChange;

                ch = m_sourceStream.Peek();

                // Handle exponent sign
                if (ch == (Char32)'-' || ch == (Char32)'+')
                {
                    int posChange = 0;
                    m_sourceStream.Next(posChange);
                    m_sourceLocation.GetColumn() += posChange;
                }
            }
        }

        ch = m_sourceStream.Peek();
    }

    const size_t valueEnd = m_sourceStream.GetPosition();

    switch ((char)ch)
    {
    case 'u':
    case 'f':
    case 'i':
        tokenFlags[0] = (char)ch;

        if (m_sourceStream.HasNext())
        {
            m_sourceStream.Next();
            ch = m_sourceStream.Peek();
        }

        break;
    }

    if (hasLeadingDot)
    {
        String value(Slice(valueStart, leadingDotPosition));
        value += "0";
        value += Slice(leadingDotPosition, valueEnd);

        return Token(tokenClass, m_tokenStream->Store(std::move(value)), tokenFlags, location);
    }

    return Token(tokenClass, Slice(valueStart, valueEnd), tokenFlags, location);
}

Token Lexer::ReadHexNumberLiteral()
{
    // location of the start of the hex number
    SourceLocation location = m_sourceLocation;

    const size_t valueStart = m_sourceStream.GetPosition();

    // read the "0x"
    for (int i = 0; i < 2; i++)
    {
        if (!m_sourceStream.HasNext())
        {
            break;
        }

        int posChange = 0;
        m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn() += posChange;
    }

    Token::Flags tokenFlags {};

    Char32 ch = m_sourceStream.Peek();

    while (m_sourceStream.HasNext() && IsHexadecimal(ch))
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn() += posChange;
        ch = m_sourceStream.Peek();
    }

    const size_t valueEnd = m_sourceStream.GetPosition();

    switch ((char)ch)
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

    return Token(TK_INTEGER, Slice(valueStart, valueEnd), tokenFlags, location);
}

Token Lexer::ReadLineComment()
{
    SourceLocation location = m_sourceLocation;

    // read '//'
    for (int i = 0; i < 2; i++)
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn() += posChange;
    }

    // read until newline or EOF is reached
    while (m_sourceStream.HasNext() && m_sourceStream.Peek() != '\n')
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn() += posChange;
    }

    return Token::empty;
}

Token Lexer::ReadBlockComment()
{
    SourceLocation location = m_sourceLocation;

    // read '/*'
    for (int i = 0; i < 2; i++)
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn() += posChange;
    }

    Char32 previous = 0;
    while (HasNext())
    {
        if (m_sourceStream.Peek() == (Char32)'/' && previous == (Char32)'*')
        {
            int posChange = 0;
            m_sourceStream.Next(posChange);
            m_sourceLocation.GetColumn() += posChange;
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
        m_sourceLocation.GetColumn() += posChange;
    }

    return Token::empty;
}

Token Lexer::ReadIdentifier()
{
    SourceLocation location = m_sourceLocation;

    const size_t start = m_sourceStream.GetPosition();

    // the character as a utf-32 character
    auto ch = m_sourceStream.Peek();

    while (IsDecimal(ch) || IsAlphabetical(ch) || ch == '_' || ch == '$')
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn() += posChange;
        // set ch to be the next character in the buffer
        ch = m_sourceStream.Peek();
    }

    const UTF8StringView value = Slice(start, m_sourceStream.GetPosition());

    if (IsSpecialFloatName(value))
    {
        return Token(TK_FLOAT, value, location);
    }

    return Token(TK_IDENT, value, location);
}

bool Lexer::HasNext()
{
    if (!m_sourceStream.HasNext())
    {
        m_errorList->AddError(CompilerError(
            ErrorLevel::Error,
            MSG_UNEXPECTED_EOF,
            m_sourceLocation));

        return false;
    }

    return true;
}

Token Lexer::ReadDocumentation()
{
    SourceLocation location = m_sourceLocation;

    // read '/**'
    for (int i = 0; i < 3; i++)
    {
        int posChange = 0;
        m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn() += posChange;
    }

    Char32 previous = 0;

    while (HasNext())
    {
        if (m_sourceStream.Peek() == (Char32)'/' && previous == (Char32)'*')
        {
            int posChange = 0;
            m_sourceStream.Next(posChange);
            m_sourceLocation.GetColumn() += posChange;
            break;
        }
        else if (m_sourceStream.Peek() == (Char32)'\n')
        {
            m_sourceLocation.GetColumn() = 0;
            m_sourceLocation.GetLine()++;
        }

        int posChange = 0;
        previous = m_sourceStream.Next(posChange);
        m_sourceLocation.GetColumn() += posChange;
    }

    return Token::empty;
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
            m_sourceLocation.GetColumn() += posChange;
        }
    }

    return hadNewline;
}

} // namespace Hyperion::DataProcessing
