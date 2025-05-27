/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rtc/RTCDataChannel.hpp>

#ifdef HYP_LIBDATACHANNEL
    #include <rtc/datachannel.hpp>
#endif

namespace hyperion {

void RTCDataChannel::Send(const String& str)
{
    Send(ByteBuffer(str.Size(), str.Data()));
}

void NullRTCDataChannel::Send(const ByteBuffer&)
{
    // Do nothing
}

#ifdef HYP_LIBDATACHANNEL

void LibDataChannelRTCDataChannel::Send(const ByteBuffer& byte_buffer)
{
    AssertThrow(m_data_channel != nullptr);

    m_data_channel->send(reinterpret_cast<const rtc::byte*>(byte_buffer.Data()), byte_buffer.Size());
}

#endif

} // namespace hyperion