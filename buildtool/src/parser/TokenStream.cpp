/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <parser/TokenStream.hpp>

namespace hyperion::buildtool {

TokenStream::TokenStream(const TokenStreamInfo &info)
    : m_position(0),
      m_info(info)
{
}

TokenStream::TokenStream(const TokenStream &other)
    : m_tokens(other.m_tokens),
      m_position(other.m_position),
      m_info(other.m_info)
{
}

TokenStream &TokenStream::operator=(const TokenStream &other)
{
    if (this == &other) {
        return *this;
    }

    m_tokens = other.m_tokens;
    m_position = other.m_position;
    m_info = other.m_info;

    return *this;
}

TokenStream::TokenStream(TokenStream &&other) noexcept
    : m_tokens(std::move(other.m_tokens)),
      m_position(other.m_position),
      m_info(std::move(other.m_info))
{
    other.m_position = 0;
}

TokenStream &TokenStream::operator=(TokenStream &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    m_tokens = std::move(other.m_tokens);
    m_position = other.m_position;
    m_info = std::move(other.m_info);

    other.m_position = 0;

    return *this;
}

} // namespace hyperion::buildtool
