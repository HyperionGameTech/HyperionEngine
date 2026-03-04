#pragma once

#include <Core/containers/Array.hpp>
#include <Core/containers/String.hpp>
#include <script/compiler/Token.hpp>
#include <Core/debug/Debug.hpp>
#include <Core/Types.hpp>

#include <vector>

namespace Hyperion {

struct TokenStreamInfo
{
    String filepath;

    TokenStreamInfo(const String& filepath)
        : filepath(filepath)
    {
    }

    TokenStreamInfo(const TokenStreamInfo& other)
        : filepath(other.filepath)
    {
    }
};

class TokenStream
{
public:
    TokenStream(const TokenStreamInfo& info);
    TokenStream(const TokenStream& other) = delete;

    Token Peek(int n = 0) const
    {
        size_t pos = m_position + n;

        if (pos >= m_tokens.Size())
        {
            return Token::EMPTY;
        }

        return m_tokens[pos];
    }

    void Push(const Token& token)
    {
        m_tokens.PushBack(token);
    }
    bool HasNext() const
    {
        return m_position < m_tokens.Size();
    }
    Token Next()
    {
        Assert(m_position < m_tokens.Size());
        return m_tokens[m_position++];
    }
    Token Last() const
    {
        Assert(!m_tokens.Empty());
        return m_tokens.Back();
    }
    size_t GetSize() const
    {
        return m_tokens.Size();
    }
    size_t GetPosition() const
    {
        return m_position;
    }
    const TokenStreamInfo& GetInfo() const
    {
        return m_info;
    }
    void SetPosition(size_t position)
    {
        m_position = position;
    }
    bool Eof() const
    {
        return m_position >= m_tokens.Size();
    }

    Array<Token> m_tokens;
    size_t m_position;

private:
    TokenStreamInfo m_info;
};

} // namespace Hyperion
