/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>
#include <rtc/RTCInstance.hpp>
#include <rtc/RTCTrack.hpp>

namespace Hyperion {

RTCInstance::RTCInstance(RTCServerParams serverParams)
{
#ifdef HYP_LIBDATACHANNEL
    m_server = MakeRefCountedPtr<LibDataChannelRTCServer>(std::move(serverParams));
#else
    m_server = MakeRefCountedPtr<NullRTCServer>(std::move(serverParams));
#endif // HYP_LIBDATACHANNEL
}

RC<RTCTrackBase> RTCInstance::CreateTrack(RTCTrackType trackType)
{
#ifdef HYP_LIBDATACHANNEL
    return MakeRefCountedPtr<LibDataChannelRTCTrack>(trackType);
#else
    return MakeRefCountedPtr<NullRTCTrack>(trackType);
#endif // HYP_LIBDATACHANNEL
}

RC<RTCStream> RTCInstance::CreateStream(RTCStreamType streamType, UniquePtr<RTCStreamEncoder>&& encoder)
{
#ifdef HYP_LIBDATACHANNEL
    return MakeRefCountedPtr<LibDataChannelRTCStream>(streamType, std::move(encoder));
#else
    return MakeRefCountedPtr<NullRTCStream>(streamType, std::move(encoder));
#endif // HYP_LIBDATACHANNEL
}

} // namespace Hyperion
