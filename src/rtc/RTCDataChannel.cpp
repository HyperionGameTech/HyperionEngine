/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>
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

void LibDataChannelRTCDataChannel::Send(const ByteBuffer& byteBuffer)
{
    Assert(m_dataChannel != nullptr);

    m_dataChannel->send(reinterpret_cast<const rtc::byte*>(byteBuffer.Data()), byteBuffer.Size());
}

#endif

} // namespace hyperion
