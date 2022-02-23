#ifndef HYPERION_JSON_TOKEN_STREAM_H
#define HYPERION_JSON_TOKEN_STREAM_H

#include "token.h"
#include "../../system/debug.h"

#include <vector>

namespace hyperion {
namespace json {
struct TokenStreamInfo {
    std::string filepath;

    TokenStreamInfo(const std::string &filepath)
        : filepath(filepath)
    {
    }

    TokenStreamInfo(const TokenStreamInfo &other)
        : filepath(other.filepath)
    {
    }
};

class TokenStream {
public:
    TokenStream(const TokenStreamInfo &info) : m_position(0), m_info(info) {}
    TokenStream(const TokenStream &other) = delete;

    inline Token Peek(int n = 0) const
    {
        size_t pos = m_position + n;
        if (pos >= m_tokens.size()) {
            return Token::EMPTY;
        }
        return m_tokens[pos];
    }

    inline void Push(const Token &token) { m_tokens.push_back(token); }
    inline bool HasNext() const { return m_position < m_tokens.size(); }
    inline Token Next() { AssertThrow(m_position < m_tokens.size()); return m_tokens[m_position++]; }
    inline Token Last() const { AssertThrow(!m_tokens.empty()); return m_tokens.back(); }
    inline size_t GetSize() const { return m_tokens.size(); }
    inline size_t GetPosition() const { return m_position; }
    inline const TokenStreamInfo &GetInfo() const { return m_info; }
    inline void SetPosition(size_t position) { m_position = position; }
    inline bool Eof() const { return m_position >= m_tokens.size(); }

    std::vector<Token> m_tokens;
    size_t m_position;

private:
    TokenStreamInfo m_info;
};

} // namespace json
} // namespace hyperion

#endif