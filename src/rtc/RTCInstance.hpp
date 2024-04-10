#ifndef HYPERION_V2_RTC_INSTANCE_HPP
#define HYPERION_V2_RTC_INSTANCE_HPP

#include <rtc/RTCClientList.hpp>
#include <rtc/RTCServer.hpp>
#include <rtc/RTCTrack.hpp>
#include <rtc/RTCStream.hpp>
#include <rtc/RTCStreamEncoder.hpp>

#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/UniquePtr.hpp>

namespace hyperion::v2 {

class HYP_API RTCInstance
{
public:
    RTCInstance(RTCServerParams server_params);
    RTCInstance(const RTCInstance &other)               = delete;
    RTCInstance &operator=(const RTCInstance &other)    = delete;
    RTCInstance(RTCInstance &&other)                    = delete;
    RTCInstance &operator=(RTCInstance &&other)         = delete;
    ~RTCInstance()                                      = default;

    const RC<RTCServer> &GetServer() const
        { return m_server; }

    RC<RTCTrack> CreateTrack(RTCTrackType track_type);
    RC<RTCStream> CreateStream(RTCStreamType stream_type, UniquePtr<RTCStreamEncoder> &&encoder);

private:
    RC<RTCServer> m_server;
};

} // namespace hyperion::v2

#endif