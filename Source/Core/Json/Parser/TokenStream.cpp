/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/JSON/Parser/TokenStream.hpp>

namespace Hyperion::JSON {

TokenStream::TokenStream(const TokenStreamInfo& info)
    : m_position(0),
      m_info(info)
{
}

} // namespace Hyperion::JSON
