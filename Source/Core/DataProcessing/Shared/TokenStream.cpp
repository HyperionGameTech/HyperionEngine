#include <Core/DataProcessing/Shared/TokenStream.hpp>

namespace Hyperion::DataProcessing {

TokenStream::TokenStream(const TokenStreamInfo& info)
    : m_tokens(),
      m_position(0),
      m_info(info)
{
}

void TokenStream::SetSource(ByteBuffer&& source)
{
    m_source = std::move(source);

    m_tokens.Clear();
    m_tokens.Reserve(m_source.Size() / 8);
    m_position = 0;
}

} // namespace Hyperion::DataProcessing
