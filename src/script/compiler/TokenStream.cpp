#include <script/compiler/TokenStream.hpp>

namespace hyperion {

TokenStream::TokenStream(const TokenStreamInfo& info)
    : m_position(0),
      m_info(info)
{
}

} // namespace hyperion
