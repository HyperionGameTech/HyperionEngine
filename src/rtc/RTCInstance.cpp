/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rtc/RTCInstance.hpp>
#include <rtc/RTCTrack.hpp>

namespace hyperion::v2 {

RTCInstance::RTCInstance(RTCServerParams server_params)
{
#ifdef HYP_LIBDATACHANNEL
    m_server.Reset(new LibDataChannelRTCServer(std::move(server_params)));
#else
    m_server.Reset(new NullRTCServer(std::move(server_params)));
#endif // HYP_LIBDATACHANNEL
}


RC<RTCTrack> RTCInstance::CreateTrack(RTCTrackType track_type)
{
#ifdef HYP_LIBDATACHANNEL
    return RC<RTCTrack>(new LibDataChannelRTCTrack(track_type));
#else
    return RC<RTCTrack>(new NullRTCTrack(track_type));
#endif // HYP_LIBDATACHANNEL
}

RC<RTCStream> RTCInstance::CreateStream(RTCStreamType stream_type, UniquePtr<RTCStreamEncoder> &&encoder)
{
#ifdef HYP_LIBDATACHANNEL
    return RC<RTCStream>(new LibDataChannelRTCStream(stream_type, std::move(encoder)));
#else
    return RC<RTCStream>(new NullRTCStream(stream_type, std::move(encoder)));
#endif // HYP_LIBDATACHANNEL
}

} // namespace hyperion::v2