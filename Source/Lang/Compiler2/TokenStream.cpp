#include <Lang/compiler/TokenStream.hpp>

namespace Hyperion {

TokenStream::TokenStream(const TokenStreamInfo& info)
    : m_position(0),
      m_info(info)
{
}

} // namespace Hyperion
