#include <script/compiler/Lexer.hpp>
#include <script/compiler/Operator.hpp>
#include <script/compiler/CompilerError.hpp>
#include <script/compiler/Keywords.hpp>

#include <array>
#include <sstream>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace hyperion::compiler {

using namespace utf;

Lexer::Lexer(
    const SourceStream &source_stream,
    TokenStream *token_stream,
    CompilationUnit *compilation_unit)
    : m_source_stream(source_stream),
      m_token_stream(token_stream),
      m_compilation_unit(compilation_unit),
      m_source_location(0, 0, source_stream.GetFile()->GetFilePath())
{
}

Lexer::Lexer(const Lexer &other)
    : m_source_stream(other.m_source_stream),
      m_token_stream(other.m_token_stream),
      m_compilation_unit(other.m_compilation_unit),
      m_source_location(other.m_source_location)
{
}

void Lexer::Analyze()
{
    // skip initial whitespace
    SkipWhitespace();

    while (m_source_stream.HasNext() && m_source_stream.Peek() != '\0') {
        Token token = NextToken();
        if (!token.Empty()) {
            m_token_stream->Push(token);
        }

        // SkipWhitespace() returns true if there was a newline
        const SourceLocation location = m_source_location;

        if (SkipWhitespace()) {
            // add the `newline` statement terminator if not a continuation token
            if (token && /*token.GetTokenClass() != TK_NEWLINE &&*/ !token.IsContinuationToken()) {
                // skip whitespace before next token
                SkipWhitespace();

                // check if next token is connected
                if (m_source_stream.HasNext() && m_source_stream.Peek() != '\0') {
                    auto peek = m_source_stream.Peek();
                    if (peek == '{' || peek == '.') {
                        // do not add newline
                        continue;
                    }
                }

                // add newline
                m_token_stream->Push(Token(TK_NEWLINE, "newline", location));
            }
        }
    }
}

Token Lexer::NextToken()
{
    SourceLocation location = m_source_location;

    std::array<u32char, 3> ch;
    int total_pos_change = 0;
    for (int i = 0; i < 3; i++) {
        int pos_change = 0;
        ch[i] = m_source_stream.Next(pos_change);
        total_pos_change += pos_change;
    }

    // go back to previous position
    m_source_stream.GoBack(total_pos_change);

    if (ch[0] == '\"' || ch[0] == '\'') {
        return ReadStringLiteral();
    } else if (ch[0] == '0' && (ch[1] == 'x' || ch[1] == 'X')) {
        return ReadHexNumberLiteral();
    } else if (utf32_isdigit(ch[0]) || (ch[0] == '.' && utf32_isdigit(ch[1]))) {
        return ReadNumberLiteral();
    } else if (ch[0] == '/' && ch[1] == '/') {
        return ReadLineComment();
    } else if (ch[0] == '/' && ch[1] == '*') {
        return ReadBlockComment();
        /*if (ch[2] == '*') {
            return ReadDocumentation();
        } else {
            return ReadBlockComment();
        }*/
    } else if (ch[0] == '#') {
        return ReadDirective();
    }  else if (utf32_isalpha(ch[0]) || ch[0] == '_' || ch[0] == '$') {
        return ReadIdentifier();
    } else if (ch[0] == '<' && ch[1] == '-') {
        for (int i = 0; i < 2; i++) {
            int pos_change = 0;
            m_source_stream.Next(pos_change);
            m_source_location.GetColumn() += pos_change;
        }
        return Token(TK_LEFT_ARROW, "<-", location);
    } else if (ch[0] == '-' && ch[1] == '>') {
        for (int i = 0; i < 2; i++) {
            int pos_change = 0;
            m_source_stream.Next(pos_change);
            m_source_location.GetColumn() += pos_change;
        }
        return Token(TK_RIGHT_ARROW, "->", location);
    } else if (ch[0] == '=' && ch[1] == '>') {
        for (int i = 0; i < 2; i++) {
            int pos_change = 0;
            m_source_stream.Next(pos_change);
            m_source_location.GetColumn() += pos_change;
        }
        return Token(TK_FAT_ARROW, "=>", location);
    } else if (ch[0] == '+' || ch[0] == '-' ||
               ch[0] == '*' || ch[0] == '/' ||
               ch[0] == '%' || ch[0] == '^' ||
               ch[0] == '&' || ch[0] == '|' ||
               ch[0] == '<' || ch[0] == '>' ||
               ch[0] == '=' || ch[0] == '!' ||
               ch[0] == '~')
    {
        return ReadOperator();
    } else if (ch[0] == ',') {
        int pos_change = 0;
        m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
        return Token(TK_COMMA, ",", location);
    } else if (ch[0] == ';') {
        int pos_change = 0;
        m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
        return Token(TK_SEMICOLON, ";", location);
    } else if (ch[0] == ':') {
        if (ch[1] == ':') {
            for (int i = 0; i < 2; i++) {
                int pos_change = 0;
                m_source_stream.Next(pos_change);
                m_source_location.GetColumn() += pos_change;
            }
            return Token(TK_DOUBLE_COLON, "::", location);
        } else if (ch[1] == '=') {
            for (int i = 0; i < 2; i++) {
                int pos_change = 0;
                m_source_stream.Next(pos_change);
                m_source_location.GetColumn() += pos_change;
            }
            return Token(TK_DEFINE, ":=", location);
        } else {
            int pos_change = 0;
            m_source_stream.Next(pos_change);
            m_source_location.GetColumn() += pos_change;
            return Token(TK_COLON, ":", location);
        }
    } else if (ch[0] == '?') {
        int pos_change = 0;
        m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
        return Token(TK_QUESTION_MARK, "?", location);
    } else if (ch[0] == '.') {
        if (ch[1] == '.' && ch[2] == '.') {
            for (int i = 0; i < 3; i++) {
                int pos_change = 0;
                m_source_stream.Next(pos_change);
                m_source_location.GetColumn() += pos_change;
            }
            return Token(TK_ELLIPSIS, "...", location);
        } else {
            int pos_change = 0;
            m_source_stream.Next(pos_change);
            m_source_location.GetColumn() += pos_change;
            return Token(TK_DOT, ".", location);
        }
    } else if (ch[0] == '(') {
        int pos_change = 0;
        m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
        return Token(TK_OPEN_PARENTH, "(", location);
    } else if (ch[0] == ')') {
        int pos_change = 0;
        m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
        return Token(TK_CLOSE_PARENTH, ")", location);
    } else if (ch[0] == '[') {
        int pos_change = 0;
        m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
        return Token(TK_OPEN_BRACKET, "[", location);
    } else if (ch[0] == ']') {
        int pos_change = 0;
        m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
        return Token(TK_CLOSE_BRACKET, "]", location);
    } else if (ch[0] == '{') {
        int pos_change = 0;
        m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
        return Token(TK_OPEN_BRACE, "{", location);
    } else if (ch[0] == '}') {
        int pos_change = 0;
        m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
        return Token(TK_CLOSE_BRACE, "}", location);
    } else {
        int pos_change = 0;
        utf::u32char bad_token = m_source_stream.Next(pos_change);

        char bad_token_str[sizeof(bad_token)] = { '\0' };
        utf::char32to8(bad_token, bad_token_str);
        
        m_compilation_unit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_unexpected_token,
            location,
            std::string(bad_token_str)
        ));

        m_source_location.GetColumn() += pos_change;

        return Token::EMPTY;
    }
}

u32char Lexer::ReadEscapeCode()
{
    // location of the start of the escape code
    SourceLocation location = m_source_location;

    if (HasNext()) {
        int pos_change = 0;
        u32char esc = m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;

        // TODO: add support for unicode escapes
        switch (esc) {
        case 't': return '\t';
        case 'b': return '\b';
        case 'n': return '\n';
        case 'r': return '\r';
        case 'f': return '\f';
        case '\'':
        case '\"':
        case '\\':
            // return the escape itself
            return esc;
        default:
            m_compilation_unit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unrecognized_escape_sequence,
                location,
                std::string("\\") + utf::get_bytes(esc)
            ));
        }
    }

    return 0;
}

Token Lexer::ReadStringLiteral()
{
    // the location for the start of the string
    SourceLocation location = m_source_location;

    String value;
    int pos_change = 0;

    u32char delim = m_source_stream.Next(pos_change);
    m_source_location.GetColumn() += pos_change;

    // the character as utf-32
    u32char ch = m_source_stream.Next(pos_change);
    m_source_location.GetColumn() += pos_change;

    while (ch != delim) {
        if (ch == (u32char)'\n' || !HasNext()) {
            // unterminated string literal
            m_compilation_unit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unterminated_string_literal,
                m_source_location
            ));

            if (ch == (u32char)'\n') {
                // increment line and reset column
                m_source_location.GetColumn() = 0;
                m_source_location.GetLine()++;
            }

            break;
        }

        // determine whether to read an escape sequence
        if (ch == (u32char)'\\') {
            u32char esc = ReadEscapeCode();
            // append the bytes
            value.Append(utf::get_bytes(esc));
        } else {
            // Append the character itself
            value.Append(utf::get_bytes(ch));
        }

        ch = m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
    }

    return Token(TK_STRING, value, location);
}

Token Lexer::ReadNumberLiteral()
{
    SourceLocation location = m_source_location;

    // store the value in a string
    String value;

    // assume integer to start
    TokenClass token_class = TK_INTEGER;

    // allows support for floats starting with '.'
    if (m_source_stream.Peek() == '.') {
        token_class = TK_FLOAT;
        value = "0.";
        int pos_change = 0;
        m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
    }

    Token::Flags token_flags;
    std::memset(token_flags, 0, sizeof(token_flags));

    u32char ch = m_source_stream.Peek();

    bool has_exponent = false;

    while (m_source_stream.HasNext() && utf32_isdigit(ch)) {
        int pos_change = 0;
        u32char next_ch = m_source_stream.Next(pos_change);
        value.Append(utf::get_bytes(next_ch));
        m_source_location.GetColumn() += pos_change;

        if (token_class != TK_FLOAT) {
            if (m_source_stream.HasNext()) {
                // the character as a utf-32 character
                u32char ch = m_source_stream.Peek();
                if (ch == (u32char)'.') {
                    // read next to check if after is a digit
                    int pos_change = 0;
                    m_source_stream.Next(pos_change);
                    
                    u32char next = m_source_stream.Peek();
                    if (!utf::utf32_isalpha(next) && next != (u32char)'_') {
                        // type is a float because of '.' and not an identifier after
                        token_class = TK_FLOAT;
                        value.Append(utf::get_bytes(ch));
                        m_source_location.GetColumn() += pos_change;
                    } else {
                        // not a float literal, so go back on the '.'
                        m_source_stream.GoBack(pos_change);
                    }
                }
            }
        }

        if (m_source_stream.HasNext()) {
            u32char ch = m_source_stream.Peek();

            if (!has_exponent && (ch == (u32char)'e' || ch == (u32char)'E')) {
                has_exponent = true;

                token_class = TK_FLOAT;
                value.Append(utf::get_bytes(ch));

                int pos_change = 0;
                m_source_stream.Next(pos_change);
                m_source_location.GetColumn() += pos_change;

                ch = m_source_stream.Peek();

                // Handle negative exponent
                if (ch == (u32char)'-') {
                    value.Append(utf::get_bytes(ch));

                    int pos_change = 0;
                    m_source_stream.Next(pos_change);
                    m_source_location.GetColumn() += pos_change;

                    m_source_location.GetColumn() += pos_change;
                }
            }
        }
        
        ch = m_source_stream.Peek();
    }

    switch ((char)ch) {
    case 'u':
    case 'f':
    case 'i':
        token_flags[0] = (char)ch;

        if (m_source_stream.HasNext()) {
            m_source_stream.Next();
            ch = m_source_stream.Peek();
        }

        break;
    }

    return Token(token_class, value, token_flags, location);
}

Token Lexer::ReadHexNumberLiteral()
{
    // location of the start of the hex number
    SourceLocation location = m_source_location;

    // store the value in a string
    String value;

    // read the "0x"
    for (int i = 0; i < 2; i++) {
        if (!m_source_stream.HasNext()) {
            break;
        }

        int pos_change = 0;
        u32char next_ch = m_source_stream.Next(pos_change);
        value.Append(utf::get_bytes(next_ch));
        m_source_location.GetColumn() += pos_change;
    }

    Token::Flags token_flags;
    std::memset(token_flags, 0, sizeof(token_flags));

    u32char ch = m_source_stream.Peek();

    while (m_source_stream.HasNext() && utf32_isxdigit(ch)) {
        int pos_change = 0;
        u32char next_ch = m_source_stream.Next(pos_change);
        value.Append(utf::get_bytes(next_ch));
        m_source_location.GetColumn() += pos_change;
        ch = m_source_stream.Peek();
    }

    switch ((char)ch) {
    case 'u':
    case 'i':
        token_flags[0] = (char)ch;

        if (m_source_stream.HasNext()) {
            m_source_stream.Next();
            ch = m_source_stream.Peek();
        }

        break;
    }

    Int64 num = std::strtoll(value.Data(), 0, 16);
    std::stringstream ss;
    ss << num;

    return Token(TK_INTEGER, value, token_flags, location);
}

Token Lexer::ReadLineComment()
{
    SourceLocation location = m_source_location;

    // read '//'
    for (int i = 0; i < 2; i++) {
        int pos_change = 0;
        m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
    }

    // read until newline or EOF is reached
    while (m_source_stream.HasNext() && m_source_stream.Peek() != '\n') {
        int pos_change = 0;
        m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
    }

    return Token(TK_NEWLINE, "newline", location);
}

Token Lexer::ReadBlockComment()
{
    SourceLocation location = m_source_location;

    // read '/*'
    for (int i = 0; i < 2; i++) {
        int pos_change = 0;
        m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
    }

    u32char previous = 0;
    while (HasNext()) {
        if (m_source_stream.Peek() == (u32char)'/' && previous == (u32char)'*') {
            int pos_change = 0;
            m_source_stream.Next(pos_change);
            m_source_location.GetColumn() += pos_change;
            break;
        } else if (m_source_stream.Peek() == (u32char)'\n') {
            // just reset column and increment line
            m_source_location.GetColumn() = 0;
            m_source_location.GetLine()++;
        }
        int pos_change = 0;
        previous = m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
    }

    return Token::EMPTY;
}

Token Lexer::ReadDocumentation()
{
    SourceLocation location = m_source_location;

    std::string value;

    // read '/**'
    for (int i = 0; i < 3; i++) {
        int pos_change = 0;
        m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
    }

    u32char previous = 0;
    while (HasNext()) {
        if (m_source_stream.Peek() == (u32char)'/' && previous == (u32char)'*') {
            int pos_change = 0;
            m_source_stream.Next(pos_change);
            m_source_location.GetColumn() += pos_change;
            break;
        } else {
            char ch[4] = {'\0'};
            utf::char32to8(m_source_stream.Peek(), ch);
            // append value
            value += ch;
            
            if (m_source_stream.Peek() == (u32char)'\n') {
                // just reset column and increment line
                m_source_location.GetColumn() = 0;
                m_source_location.GetLine()++;
            }
        }
        int pos_change = 0;
        previous = m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
    }

    return Token::EMPTY;
}

Token Lexer::ReadOperator()
{
    // location of the start of the hex number
    SourceLocation location = m_source_location;

    std::array<u32char, 2> ch;
    int total_pos_change = 0;
    for (int i = 0; i < 2; i++) {
        int pos_change = 0;
        ch[i] = m_source_stream.Next(pos_change);
        total_pos_change += pos_change;
    }
    // go back
    m_source_stream.GoBack(total_pos_change);

    String op_1 = utf::get_bytes(ch[0]);
    String op_2 = op_1 + utf::get_bytes(ch[1]);

    if (Operator::IsUnaryOperator(op_2) || Operator::IsBinaryOperator(op_2)) {
        int pos_change_1 = 0, pos_change_2 = 0;
        m_source_stream.Next(pos_change_1);
        m_source_stream.Next(pos_change_2);
        m_source_location.GetColumn() += pos_change_1 + pos_change_2;
        return Token(TK_OPERATOR, op_2, location);
    } else if (Operator::IsUnaryOperator(op_1) || Operator::IsBinaryOperator(op_1)) {
        int pos_change = 0;
        m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
        return Token(TK_OPERATOR, op_1, location);
    }

    return Token::EMPTY;
}

Token Lexer::ReadDirective()
{
    SourceLocation location = m_source_location;

    // read '#'
    int pos_change = 0;
    m_source_stream.Next(pos_change);
    m_source_location.GetColumn() += pos_change;

    // store the name
    String value;

    // the character as a utf-32 character
    u32char ch = m_source_stream.Peek();

    while (utf32_isdigit(ch) || ch == (u32char)('_') || utf32_isalpha(ch)) {
        int pos_change = 0;
        ch = m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
        // append the raw bytes
        value.Append(utf::get_bytes(ch));
        // set ch to be the next character in the buffer
        ch = m_source_stream.Peek();
    }

    return Token(TK_DIRECTIVE, value, location);
}

Token Lexer::ReadIdentifier()
{
    SourceLocation location = m_source_location;

    // store the name in this string
    String value;

    // the character as a utf-32 character
    auto ch = m_source_stream.Peek();

    while (utf32_isdigit(ch) || utf32_isalpha(ch) || ch == '_' || ch == '$') {
        int pos_change = 0;
        ch = m_source_stream.Next(pos_change);
        m_source_location.GetColumn() += pos_change;
        // append the raw bytes
        value.Append(utf::get_bytes(ch));
        // set ch to be the next character in the buffer
        ch = m_source_stream.Peek();

        // if (ch == ':') {
        //     int pos_change = 0;
        //     ch = m_source_stream.Next(pos_change);
        //     m_source_location.GetColumn() += pos_change;
            
        //     return Token(TK_LABEL, value, location);
        // }
    }

    // read operator in the case that the string is "operator", like c++
    if (value == "operator") {
        // allow for other operators not defined in operator list such as "operator[]" and "operator[]="
        static const String other_operators[] {
            "[]=", "[]"
        };

        for (const String &op : other_operators) {
            // check if next tokens are the operator
            
            const SizeType len = op.Length();

            int pos_change = 0;

            bool is_operator = true;

            for (SizeType i = 0; i < len; i++) {
                if (m_source_stream.Peek() != op.GetChar(i)) {
                    is_operator = false;
                    break;
                }

                int char_pos_change = 0;
                m_source_stream.Next(char_pos_change);
                m_source_location.GetColumn() += char_pos_change;

                pos_change += char_pos_change;
            }

            if (is_operator) {
                return Token(TK_IDENT, "operator" + op, location);
            }

            // rewind
            m_source_stream.GoBack(pos_change);
            m_source_location.GetColumn() -= pos_change;
        }

        if (Token operator_token = ReadOperator()) {
            value += operator_token.GetValue();

            const Operator *op = nullptr;

            if (!Operator::IsBinaryOperator(operator_token.GetValue(), op)) {
                AssertThrow(Operator::IsUnaryOperator(operator_token.GetValue(), op));
            }

            AssertThrow(op != nullptr);

            if (!op->SupportsOverloading()) {
                m_compilation_unit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_cannot_overload_operator,
                    m_source_location,
                    operator_token.GetValue()
                ));
            }
        }
    }

    return Token(Keyword::IsKeyword(value) ? TK_KEYWORD : TK_IDENT, value, location);
}

bool Lexer::HasNext()
{
    if (!m_source_stream.HasNext()) {
        m_compilation_unit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_unexpected_eof,
            m_source_location
        ));

        return false;
    }

    return true;
}

bool Lexer::SkipWhitespace()
{
    bool had_newline = false;

    while (m_source_stream.HasNext() && utf32_isspace(m_source_stream.Peek())) {
        int pos_change = 0;
        if (m_source_stream.Next(pos_change) == (u32char)'\n') {
            m_source_location.GetLine()++;
            m_source_location.GetColumn() = 0;
            had_newline = true;
        } else {
            m_source_location.GetColumn() += pos_change;
        }
    }

    return had_newline;
}

} // namespace hyperion::compiler
