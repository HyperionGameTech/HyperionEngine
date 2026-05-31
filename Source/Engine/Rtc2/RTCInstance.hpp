/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <RTC/RTCClientList.hpp>
#include <RTC/RTCServer.hpp>
#include <RTC/RTCTrack.hpp>
#include <RTC/RTCStream.hpp>
#include <RTC/RTCStreamEncoder.hpp>

#include <Core/memory/RefCountedPtr.hpp>
#include <Core/memory/UniquePtr.hpp>

namespace Hyperion {

class ENGINE_API RTCInstance
{
public:
    RTCInstance(RTCServerParams serverParams);
    RTCInstance(const RTCInstance& other) = delete;
    RTCInstance& operator=(const RTCInstance& other) = delete;
    RTCInstance(RTCInstance&& other) = delete;
    RTCInstance& operator=(RTCInstance&& other) = delete;
    ~RTCInstance() = default;

    const RC<RTCServer>& GetServer() const
    {
        return m_server;
    }

    RC<RTCTrackBase> CreateTrack(RTCTrackType trackType);
    RC<RTCStream> CreateStream(RTCStreamType streamType, UniquePtr<RTCStreamEncoder>&& encoder);

private:
    RC<RTCServer> m_server;
};

} // namespace Hyperion
