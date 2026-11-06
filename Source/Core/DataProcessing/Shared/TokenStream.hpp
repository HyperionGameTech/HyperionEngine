#pragma once

#include <Core/DataProcessing/Shared/Token.hpp>
#include <Core/Containers/Array.hpp>
#include <Core/Containers/List.hpp>
#include <Core/Memory/ByteBuffer.hpp>

namespace Hyperion::DataProcessing {

struct CORE_API TokenStreamInfo
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

class CORE_API TokenStream
{
public:
    TokenStream(const TokenStreamInfo& info);
    TokenStream(const TokenStream& other) = delete;
    TokenStream& operator=(const TokenStream& other) = delete;

    void SetSource(ByteBuffer&& source);

    const char* GetSourceData() const { return reinterpret_cast<const char*>(m_source.Data()); }
    size_t GetSourceSize() const { return m_source.Size(); }

    // for token values that differ from the source text, like strings with escapes
    UTF8StringView Store(String&& value)
    {
        const String& stored = m_storedValues.EmplaceBack(std::move(value));

        return stored;
    }

    const Token& Peek(int n = 0) const
    {
        size_t pos = m_position + n;

        if (pos >= m_tokens.Size())
        {
            return Token::empty;
        }

        return m_tokens[pos];
    }

    void Push(const Token& token) { m_tokens.PushBack(token); }

    bool HasNext() const { return m_position < m_tokens.Size(); }

    const Token& Next()
    {
        if (m_position < m_tokens.Size())
        {
            return m_tokens[m_position++];
        }

        return Token::empty;
    }

    const Token& Last() const
    {
        if (m_tokens.Any())
        {
            return m_tokens.Back();
        }

        return Token::empty;
    }

    size_t GetSize() const { return m_tokens.Size(); }
    size_t GetPosition() const { return m_position; }
    const TokenStreamInfo& GetInfo() const { return m_info; }
    void SetPosition(size_t position) { m_position = position; }
    bool Eof() const { return m_position >= m_tokens.Size(); }

    Array<Token> m_tokens;
    size_t m_position;

private:
    TokenStreamInfo m_info;
    ByteBuffer m_source;
    List<String> m_storedValues;
};

} // namespace Hyperion::DataProcessing
