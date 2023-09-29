#ifndef TOKEN_STREAM_HPP
#define TOKEN_STREAM_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/String.hpp>
#include <script/compiler/Token.hpp>
#include <system/Debug.hpp>
#include <Types.hpp>

#include <vector>

namespace hyperion::compiler {

struct TokenStreamInfo
{
    String filepath;

    TokenStreamInfo(const String &filepath)
        : filepath(filepath)
    {
    }

    TokenStreamInfo(const TokenStreamInfo &other)
        : filepath(other.filepath)
    {
    }
};

class TokenStream
{
public:
    TokenStream(const TokenStreamInfo &info);
    TokenStream(const TokenStream &other) = delete;
    
    Token Peek(int n = 0) const
    {
        SizeType pos = m_position + n;

        if (pos >= m_tokens.Size()) {
            return Token::EMPTY;
        }

        return m_tokens[pos];
    }

    void Push(const Token &token) { m_tokens.PushBack(token); }
    bool HasNext() const { return m_position < m_tokens.Size(); }
    Token Next() { AssertThrow(m_position < m_tokens.Size()); return m_tokens[m_position++]; }
    Token Last() const { AssertThrow(!m_tokens.Empty()); return m_tokens.Back(); }
    SizeType GetSize() const { return m_tokens.Size(); }
    SizeType GetPosition() const { return m_position; }
    const TokenStreamInfo &GetInfo() const { return m_info; }
    void SetPosition(SizeType position) { m_position = position; }
    bool Eof() const { return m_position >= m_tokens.Size(); }

    Array<Token> m_tokens;
    SizeType m_position;

private:
    TokenStreamInfo m_info;
};

} // namespace hyperion::compiler

#endif
