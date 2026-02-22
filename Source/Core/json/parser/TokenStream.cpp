/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Core/json/parser/TokenStream.hpp>

namespace Hyperion::JSON {

TokenStream::TokenStream(const TokenStreamInfo& info)
    : m_position(0),
      m_info(info)
{
}

} // namespace Hyperion::JSON
