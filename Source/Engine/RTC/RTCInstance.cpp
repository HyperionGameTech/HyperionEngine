/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>
#include <RTC/RTCInstance.hpp>
#include <RTC/RTCTrack.hpp>

namespace Hyperion {

RTCInstance::RTCInstance(RTCServerParams serverParams)
{
#ifdef HYP_LIBDATACHANNEL
    m_server = MakeShared<LibDataChannelRTCServer>(std::move(serverParams));
#else
    m_server = MakeShared<NullRTCServer>(std::move(serverParams));
#endif // HYP_LIBDATACHANNEL
}

SharedPtr<RTCTrackBase> RTCInstance::CreateTrack(RTCTrackType trackType)
{
#ifdef HYP_LIBDATACHANNEL
    return MakeShared<LibDataChannelRTCTrack>(trackType);
#else
    return MakeShared<NullRTCTrack>(trackType);
#endif // HYP_LIBDATACHANNEL
}

SharedPtr<RTCStream> RTCInstance::CreateStream(RTCStreamType streamType, UniquePtr<RTCStreamEncoder>&& encoder)
{
#ifdef HYP_LIBDATACHANNEL
    return MakeShared<LibDataChannelRTCStream>(streamType, std::move(encoder));
#else
    return MakeShared<NullRTCStream>(streamType, std::move(encoder));
#endif // HYP_LIBDATACHANNEL
}

} // namespace Hyperion
