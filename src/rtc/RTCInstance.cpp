/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rtc/RTCInstance.hpp>
#include <rtc/RTCTrack.hpp>

namespace hyperion {

RTCInstance::RTCInstance(RTCServerParams server_params)
{
#ifdef HYP_LIBDATACHANNEL
    m_server = MakeRefCountedPtr<LibDataChannelRTCServer>(std::move(server_params));
#else
    m_server = MakeRefCountedPtr<NullRTCServer>(std::move(server_params));
#endif // HYP_LIBDATACHANNEL
}

RC<RTCTrack> RTCInstance::CreateTrack(RTCTrackType track_type)
{
#ifdef HYP_LIBDATACHANNEL
    return MakeRefCountedPtr<LibDataChannelRTCTrack>(track_type);
#else
    return MakeRefCountedPtr<NullRTCTrack>(track_type);
#endif // HYP_LIBDATACHANNEL
}

RC<RTCStream> RTCInstance::CreateStream(RTCStreamType stream_type, UniquePtr<RTCStreamEncoder> &&encoder)
{
#ifdef HYP_LIBDATACHANNEL
    return MakeRefCountedPtr<LibDataChannelRTCStream>(stream_type, std::move(encoder));
#else
    return MakeRefCountedPtr<NullRTCStream>(stream_type, std::move(encoder));
#endif // HYP_LIBDATACHANNEL
}

} // namespace hyperion