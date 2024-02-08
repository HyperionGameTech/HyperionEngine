#include <util/json/parser/TokenStream.hpp>

namespace hyperion::json {

TokenStream::TokenStream(const TokenStreamInfo &info)
    : m_position(0),
      m_info(info)
{
}

} // namespace hyperion::json
